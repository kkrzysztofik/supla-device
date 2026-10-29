/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_SOCOMEC_TRANSPORT_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_SOCOMEC_TRANSPORT_H_

#include <driver/uart.h>
#include <supla/sensor/socomec_countis.h>

namespace Supla {

// RS-485 half duplex; DE/RE driven by the UART on the RTS pin. Wire the
// transceiver's /RE to the same RTS-driven DE line (or otherwise keep the
// receiver disabled during TX): an always-enabled receiver echoes the request
// back and breaks every poll.
class EspIdfSocomecTransport : public Socomec::Transport {
 public:
  EspIdfSocomecTransport(uart_port_t uart, int txPin, int rxPin, int dePin,
                         int baud);
  bool write(const uint8_t *data, size_t len) override;
  int readAvailable(uint8_t *buffer, size_t maxLen) override;
  void flushRx() override;

 private:
  uart_port_t uart;
  bool ready = false;
};

}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_SOCOMEC_TRANSPORT_H_
