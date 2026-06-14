set(_BYD_DIR ${SUPLA_LINUX_PORT_DIR}/byd)

find_package(CURL REQUIRED)

supla_linux_register_extension(
  NAME byd
  INIT_FUNCTION initBydExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/byd_extension.cpp
    ${_BYD_DIR}/byd_types.cpp
    ${_BYD_DIR}/byd_hash.cpp
    ${_BYD_DIR}/byd_pkcs7.cpp
    ${_BYD_DIR}/byd_bangcle.cpp
    ${_BYD_DIR}/byd_aes.cpp
    ${_BYD_DIR}/byd_sign.cpp
    ${_BYD_DIR}/byd_session.cpp
    ${_BYD_DIR}/byd_http_client.cpp
    ${_BYD_DIR}/byd_cloud_client.cpp
    ${_BYD_DIR}/byd_readings.cpp
    ${_BYD_DIR}/byd_vehicle_poller.cpp
    ${_BYD_DIR}/byd_poller_client.cpp
    ${_BYD_DIR}/byd_measurement.cpp
    ${_BYD_DIR}/byd_meter.cpp
    ${_BYD_DIR}/byd_thermometer.cpp
    ${_BYD_DIR}/byd_binary.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_BYD_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
  LIBRARIES
    CURL::libcurl
    pthread
)
