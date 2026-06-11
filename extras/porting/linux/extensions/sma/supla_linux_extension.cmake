set(_SMA_DIR ${SUPLA_LINUX_PORT_DIR}/sma)

supla_linux_register_extension(
  NAME sma
  INIT_FUNCTION initSmaExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/sma_extension.cpp
    ${_SMA_DIR}/sma_serial_port.cpp
    ${_SMA_DIR}/smanet_framer.cpp
    ${_SMA_DIR}/sma_channel_codec.cpp
    ${_SMA_DIR}/sma_cinfo_parser.cpp
    ${_SMA_DIR}/sma_profile_loader.cpp
    ${_SMA_DIR}/sma_device_profiles.cpp
    ${_SMA_DIR}/sma_bus.cpp
    ${_SMA_DIR}/sma_bus_client.cpp
    ${_SMA_DIR}/smadata_client.cpp
    ${_SMA_DIR}/sma_inverter.cpp
    ${_SMA_DIR}/sma_dc_meter.cpp
    ${_SMA_DIR}/sma_thermometer.cpp
    ${_SMA_DIR}/sma_measurement.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_SMA_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
  LIBRARIES
    pthread
)
