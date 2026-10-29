supla_linux_register_extension(
  NAME socomec
  INIT_FUNCTION initSocomecExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/socomec_extension.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
)
