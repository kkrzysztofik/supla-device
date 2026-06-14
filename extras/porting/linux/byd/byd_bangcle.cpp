/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_bangcle.h"

#include "byd_pkcs7.h"

#include <supla/log_wrapper.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

constexpr unsigned char kZeroIv[16] = {};

uint32_t readLeU32(const unsigned char* data) {
  uint32_t value = 0;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

void writeLeU32(unsigned char* out, uint32_t value) {
  std::memcpy(out, &value, sizeof(value));
}

void prepareAesMatrix(const unsigned char* input, unsigned char* output) {
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      output[col * 8 + row] = input[col + row * 4];
    }
  }
}

void xorInto(unsigned char* target, const unsigned char* source, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    target[i] ^= source[i];
  }
}

std::string normaliseEnvelopeInput(const std::string& envelope) {
  std::string cleaned;
  cleaned.reserve(envelope.size());
  for (char ch : envelope) {
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      continue;
    }
    if (ch == '-') {
      cleaned.push_back('+');
    } else if (ch == '_') {
      cleaned.push_back('/');
    } else {
      cleaned.push_back(ch);
    }
  }
  if (cleaned.empty() || cleaned[0] != 'F') {
    return {};
  }
  cleaned.erase(0, 1);
  const size_t remainder = cleaned.size() % 4;
  if (remainder != 0) {
    cleaned.append(4 - remainder, '=');
  }
  return cleaned;
}

std::vector<unsigned char> base64Decode(const std::string& input) {
  static const int kDecode[256] = {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
      52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
      -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
      15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
      -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
      41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  };

  std::vector<unsigned char> out;
  out.reserve(input.size() * 3 / 4);
  int val = 0;
  int valb = -8;
  for (unsigned char c : input) {
    if (c == '=') {
      break;
    }
    const int decoded = kDecode[c];
    if (decoded < 0) {
      continue;
    }
    val = (val << 6) + decoded;
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

std::string base64Encode(const unsigned char* data, size_t len) {
  static const char* kTable =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    const unsigned int b0 = data[i];
    const unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0;
    const unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0;
    out.push_back(kTable[(b0 >> 2) & 0x3F]);
    out.push_back(kTable[((b0 << 4) | (b1 >> 4)) & 0x3F]);
    if (i + 1 < len) {
      out.push_back(kTable[((b1 << 2) | (b2 >> 6)) & 0x3F]);
    } else {
      out.push_back('=');
    }
    if (i + 2 < len) {
      out.push_back(kTable[b2 & 0x3F]);
    } else {
      out.push_back('=');
    }
  }
  return out;
}

}  // namespace

struct BangcleTablesView {
  const std::vector<unsigned char>& invRound;
  const std::vector<unsigned char>& invXor;
  const std::vector<unsigned char>& invFirst;
  const std::vector<unsigned char>& round;
  const std::vector<unsigned char>& xorTables;
  const std::vector<unsigned char>& finalTables;
  const unsigned char* permDecrypt;
  const unsigned char* permEncrypt;
};

void decryptBlockAuth(const BangcleTablesView& tables,
                      const unsigned char* block,
                      unsigned char* output,
                      int roundStart = 1) {
  unsigned char state[32] = {};
  unsigned char temp64[64] = {};
  unsigned char tmp32[32] = {};

  prepareAesMatrix(block, state);
  const int param3 = roundStart;

  for (int rnd = 9; rnd > std::max(0, param3 - 1); --rnd) {
    const int lVar21 = rnd * 4;
    int permPtr = 0;

    for (int i = 0; i < 4; ++i) {
      const unsigned char bVar3 = tables.permDecrypt[permPtr];
      const int lVar16 = i * 8;
      const int base = i * 16;

      for (int j = 0; j < 4; ++j) {
        const int uVar7 = (bVar3 + j) & 3;
        const unsigned char byteVal = state[lVar16 + uVar7];
        const size_t idx = byteVal + (i + (lVar21 + uVar7) * 4) * 256;
        const uint32_t value = readLeU32(&tables.invRound[idx * 4]);
        writeLeU32(&temp64[base + j * 4], value);
      }
      permPtr += 2;
    }

    int iVar15 = 1;
    for (int lVar21Xor = 0; lVar21Xor < 4; ++lVar21Xor) {
      int pbVar18Offset = lVar21Xor;
      for (int lVar9Xor = 0; lVar9Xor < 4; ++lVar9Xor) {
        const unsigned char local10 = temp64[pbVar18Offset];
        unsigned char uVar6 = local10 & 0xF;
        unsigned char uVar26 = local10 & 0xF0;

        const unsigned char localF0 = temp64[pbVar18Offset + 0x10];
        const unsigned char localF1 = temp64[pbVar18Offset + 0x20];
        const unsigned char localF2 = temp64[pbVar18Offset + 0x30];

        const int lVar2 = lVar9Xor * 0x18 + rnd * 0x60;
        int iVar25 = iVar15;

        for (int lVar16 = 0; lVar16 < 3; ++lVar16) {
          unsigned char bVar3Inner = localF2;
          if (lVar16 == 0) {
            bVar3Inner = localF0;
          } else if (lVar16 == 1) {
            bVar3Inner = localF1;
          }

          const unsigned char uVar1 = (bVar3Inner << 4) & 0xFF;
          const unsigned char uVar27 = uVar6 | uVar1;
          uVar26 = static_cast<unsigned char>(
              ((uVar26 >> 4) | ((bVar3Inner >> 4) << 4)) & 0xFF);

          const size_t idx1 = (lVar2 + (iVar25 - 1)) * 0x100 + uVar27;
          uVar6 = tables.invXor[idx1] & 0xF;

          const size_t idx2 = (lVar2 + iVar25) * 0x100 + uVar26;
          const unsigned char bVar3New = tables.invXor[idx2];
          uVar26 = (bVar3New & 0xF) << 4;
          iVar25 += 2;
        }

        state[lVar9Xor + lVar21Xor * 8] = (uVar26 | uVar6) & 0xFF;
        pbVar18Offset += 4;
      }
      iVar15 += 6;
    }
  }

  if (param3 == 1) {
    std::memcpy(tmp32, state, 32);
    int uVar8 = 1;
    int uVar10 = 3;
    int uVar12 = 2;

    for (int row = 0; row < 4; ++row) {
      const size_t idx0 = tmp32[row] + row * 0x400;
      state[row] = tables.invFirst[idx0];

      const int row1 = uVar10 & 3;
      const size_t idx1 = tmp32[8 + row1] + row1 * 0x400 + 0x100;
      state[8 + row] = tables.invFirst[idx1];

      const int row2 = uVar12 & 3;
      const size_t idx2 = tmp32[0x10 + row2] + row2 * 0x400 + 0x200;
      state[0x10 + row] = tables.invFirst[idx2];

      const int row3 = uVar8 & 3;
      const size_t idx3 = tmp32[0x18 + row3] + row3 * 0x400 + 0x300;
      state[0x18 + row] = tables.invFirst[idx3];

      ++uVar8;
      ++uVar10;
      ++uVar12;
    }
  }

  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      output[col + row * 4] = state[col * 8 + row];
    }
  }
}

void encryptBlockAuth(const BangcleTablesView& tables,
                      const unsigned char* block,
                      unsigned char* output,
                      int roundEnd = 10) {
  unsigned char state[32] = {};
  unsigned char temp64[64] = {};
  unsigned char tmp32[32] = {};

  prepareAesMatrix(block, state);
  const int param3 = roundEnd;
  const int rounds = std::min(9, std::max(0, param3));

  for (int rnd = 0; rnd < rounds; ++rnd) {
    const int lVar21 = rnd * 4;
    int permPtr = 0;

    for (int i = 0; i < 4; ++i) {
      const unsigned char bVar4 = tables.permEncrypt[permPtr];
      const int lVar16 = i * 8;
      const int base = i * 16;

      for (int j = 0; j < 4; ++j) {
        const int uVar8 = (bVar4 + j) & 3;
        const unsigned char byteVal = state[lVar16 + uVar8];
        const size_t idx = byteVal + (i + (lVar21 + uVar8) * 4) * 256;
        const uint32_t value = readLeU32(&tables.round[idx * 4]);
        writeLeU32(&temp64[base + j * 4], value);
      }
      permPtr += 2;
    }

    int iVar16 = 1;
    for (int lVar22 = 0; lVar22 < 4; ++lVar22) {
      int pbVar19Offset = lVar22;
      for (int lVar10 = 0; lVar10 < 4; ++lVar10) {
        const unsigned char local10 = temp64[pbVar19Offset];
        unsigned char uVar7 = local10 & 0xF;
        unsigned char uVar26 = local10 & 0xF0;

        const unsigned char localF0 = temp64[pbVar19Offset + 0x10];
        const unsigned char localF1 = temp64[pbVar19Offset + 0x20];
        const unsigned char localF2 = temp64[pbVar19Offset + 0x30];

        const int lVar2 = lVar10 * 0x18 + rnd * 0x60;
        int iVar25 = iVar16;

        for (int lVar17 = 0; lVar17 < 3; ++lVar17) {
          unsigned char bVar4Inner = localF2;
          if (lVar17 == 0) {
            bVar4Inner = localF0;
          } else if (lVar17 == 1) {
            bVar4Inner = localF1;
          }

          const unsigned char uVar1 = (bVar4Inner << 4) & 0xFF;
          const unsigned char uVar27 = uVar7 | uVar1;
          uVar26 = static_cast<unsigned char>(
              ((uVar26 >> 4) | ((bVar4Inner >> 4) << 4)) & 0xFF);

          const size_t idx1 = (lVar2 + (iVar25 - 1)) * 0x100 + uVar27;
          uVar7 = tables.xorTables[idx1] & 0xF;

          const size_t idx2 = (lVar2 + iVar25) * 0x100 + uVar26;
          const unsigned char bVar4New = tables.xorTables[idx2];
          uVar26 = (bVar4New & 0xF) << 4;
          iVar25 += 2;
        }

        state[lVar10 + lVar22 * 8] = (uVar26 | uVar7) & 0xFF;
        pbVar19Offset += 4;
      }
      iVar16 += 6;
    }
  }

  if (param3 == 10) {
    std::memcpy(tmp32, state, 32);
    const int uVar13 = 3;
    const int uVar9 = 2;
    const int uVar11 = 1;
    const int uVar8Enc = 0;

    for (int row = 0; row < 4; ++row) {
      const int row0 = (uVar8Enc + row) & 3;
      state[row] = tables.finalTables[tmp32[row0] + row0 * 0x400];

      const int row1 = (uVar11 + row) & 3;
      state[8 + row] = tables.finalTables[tmp32[8 + row1] + row1 * 0x400 + 0x100];

      const int row2 = (uVar9 + row) & 3;
      state[0x10 + row] = tables.finalTables[tmp32[0x10 + row2] + row2 * 0x400 + 0x200];

      const int row3 = (uVar13 + row) & 3;
      state[0x18 + row] = tables.finalTables[tmp32[0x18 + row3] + row3 * 0x400 + 0x300];
    }
  }

  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      output[col + row * 4] = state[col * 8 + row];
    }
  }
}

std::vector<unsigned char> decryptCbc(const BangcleTablesView& tables,
                                      const unsigned char* data,
                                      size_t len) {
  std::vector<unsigned char> result(len);
  unsigned char prev[16];
  std::memcpy(prev, kZeroIv, 16);

  for (size_t offset = 0; offset < len; offset += 16) {
    unsigned char decrypted[16];
    decryptBlockAuth(tables, data + offset, decrypted, 1);
    xorInto(decrypted, prev, 16);
    std::memcpy(&result[offset], decrypted, 16);
    std::memcpy(prev, data + offset, 16);
  }
  return result;
}

std::vector<unsigned char> encryptCbc(const BangcleTablesView& tables,
                                      const unsigned char* data,
                                      size_t len) {
  std::vector<unsigned char> result(len);
  unsigned char prev[16];
  std::memcpy(prev, kZeroIv, 16);

  for (size_t offset = 0; offset < len; offset += 16) {
    unsigned char block[16];
    std::memcpy(block, data + offset, 16);
    xorInto(block, prev, 16);
    unsigned char encrypted[16];
    encryptBlockAuth(tables, block, encrypted, 10);
    std::memcpy(&result[offset], encrypted, 16);
    std::memcpy(prev, encrypted, 16);
  }
  return result;
}

BangcleCodec::BangcleCodec(const std::string& tablesPath)
    : tablesPath_(tablesPath) {}

bool BangcleCodec::loadTables() {
  if (loaded_) {
    return true;
  }

  std::vector<std::string> candidates;
  if (!tablesPath_.empty()) {
    candidates.push_back(tablesPath_);
  }
  candidates.push_back("extras/porting/linux/byd/data/bangcle_tables.bin");
  candidates.push_back("../extras/porting/linux/byd/data/bangcle_tables.bin");

  std::ifstream in;
  for (const auto& path : candidates) {
    in.open(path, std::ios::binary);
    if (in) {
      break;
    }
  }
  if (!in) {
    SUPLA_LOG_ERROR("BYD: failed to open bangcle tables");
    return false;
  }

  std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (data.size() < 72) {
    SUPLA_LOG_ERROR("BYD: bangcle table file too short");
    return false;
  }
  if (data[0] != 'B' || data[1] != 'G' || data[2] != 'T' || data[3] != 'B') {
    SUPLA_LOG_ERROR("BYD: bangcle table bad magic");
    return false;
  }

  const auto loadTable = [&](size_t index, size_t expectedLen,
                             std::vector<unsigned char>* out) -> bool {
    const size_t idxOffset = 8 + index * 8;
    uint32_t offset = 0;
    uint32_t length = 0;
    std::memcpy(&offset, &data[idxOffset], 4);
    std::memcpy(&length, &data[idxOffset + 4], 4);
    if (length != expectedLen || offset + length > data.size()) {
      SUPLA_LOG_ERROR("BYD: bangcle table %zu invalid", index);
      return false;
    }
    out->assign(data.begin() + offset, data.begin() + offset + length);
    return true;
  };

  std::vector<unsigned char> permDecrypt;
  std::vector<unsigned char> permEncrypt;
  if (!loadTable(0, 0x28000, &invRound_) || !loadTable(1, 0x3C000, &invXor_) ||
      !loadTable(2, 0x1000, &invFirst_) || !loadTable(3, 0x28000, &round_) ||
      !loadTable(4, 0x3C000, &xorTables_) || !loadTable(5, 0x1000, &finalTables_) ||
      !loadTable(6, 8, &permDecrypt) || !loadTable(7, 8, &permEncrypt)) {
    return false;
  }
  std::memcpy(permDecrypt_, permDecrypt.data(), 8);
  std::memcpy(permEncrypt_, permEncrypt.data(), 8);

  loaded_ = true;
  return true;
}

std::string BangcleCodec::encodeEnvelope(const std::string& plaintext) const {
  if (!loaded_) {
    return {};
  }
  BangcleTablesView view{invRound_,     invXor_,      invFirst_, round_,
                         xorTables_,    finalTables_, permDecrypt_, permEncrypt_};
  const auto padded = addPkcs7(reinterpret_cast<const unsigned char*>(plaintext.data()),
                               plaintext.size());
  const auto ciphertext = encryptCbc(view, padded.data(), padded.size());
  return "F" + base64Encode(ciphertext.data(), ciphertext.size());
}

std::vector<unsigned char> BangcleCodec::decodeEnvelope(
    const std::string& envelope) const {
  if (!loaded_) {
    return {};
  }
  BangcleTablesView view{invRound_,     invXor_,      invFirst_, round_,
                         xorTables_,    finalTables_, permDecrypt_, permEncrypt_};
  const std::string b64 = normaliseEnvelopeInput(envelope);
  const auto ciphertext = base64Decode(b64);
  if (ciphertext.empty() || (ciphertext.size() % 16) != 0) {
    return {};
  }
  return stripPkcs7(decryptCbc(view, ciphertext.data(), ciphertext.size()));
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla
