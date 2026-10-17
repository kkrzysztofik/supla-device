/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "byd_hash.h"

#include <gtest/gtest.h>

using Supla::Linux::Byd::md5Hex;
using Supla::Linux::Byd::pwdLoginKey;
using Supla::Linux::Byd::sha1Mixed;

TEST(BydHashTest, Md5HexUppercase) {
  EXPECT_EQ(md5Hex("abc"), "900150983CD24FB0D6963F7D28E17F72");
}

TEST(BydHashTest, PwdLoginKeyDoubleMd5) {
  EXPECT_EQ(pwdLoginKey("secret"), md5Hex(md5Hex("secret")));
}

TEST(BydHashTest, Sha1MixedNonEmpty) {
  const std::string mixed = sha1Mixed("test");
  EXPECT_FALSE(mixed.empty());
}
