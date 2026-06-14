set(_INGECON_DIR ${SUPLA_LINUX_PORT_DIR}/ingecon)

supla_linux_register_extension(
  NAME ingecon
  INIT_FUNCTION initIngeconExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/ingecon_extension.cpp
    ${_INGECON_DIR}/ingecon_types.cpp
    ${_INGECON_DIR}/ingecon_modbus_rtu.cpp
    ${_INGECON_DIR}/ingecon_serial_port.cpp
    ${_INGECON_DIR}/ingecon_bus.cpp
    ${_INGECON_DIR}/ingecon_bus_client.cpp
    ${_INGECON_DIR}/ingecon_inverter.cpp
    ${_INGECON_DIR}/ingecon_dc_meter.cpp
    ${_INGECON_DIR}/ingecon_measurement.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_INGECON_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
  LIBRARIES
    pthread
)
