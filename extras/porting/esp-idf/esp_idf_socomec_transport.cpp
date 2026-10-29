/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "esp_idf_socomec_transport.h"

#include <supla/log_wrapper.h>

namespace Supla {

EspIdfSocomecTransport::EspIdfSocomecTransport(uart_port_t uart, int txPin,
                                               int rxPin, int dePin, int baud)
    : uart(uart) {
  uart_config_t config = {};
  config.baud_rate = baud;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;
  ready = uart_param_config(uart, &config) == ESP_OK &&
          uart_set_pin(uart, txPin, rxPin, dePin, UART_PIN_NO_CHANGE) ==
              ESP_OK &&
          uart_driver_install(uart, 256, 0, 0, nullptr, 0) == ESP_OK &&
          uart_set_mode(uart, UART_MODE_RS485_HALF_DUPLEX) == ESP_OK;
  if (!ready) {
    SUPLA_LOG_ERROR("Socomec: UART%d setup failed", uart);
  }
}

bool EspIdfSocomecTransport::write(const uint8_t *data, size_t len) {
  return ready && uart_write_bytes(uart, data, len) == static_cast<int>(len);
}

int EspIdfSocomecTransport::readAvailable(uint8_t *buffer, size_t maxLen) {
  if (!ready) {
    return -1;
  }
  return uart_read_bytes(uart, buffer, maxLen, 0);  // 0 ticks: never blocks
}

void EspIdfSocomecTransport::flushRx() {
  if (ready) {
    uart_flush_input(uart);
  }
}

}  // namespace Supla
