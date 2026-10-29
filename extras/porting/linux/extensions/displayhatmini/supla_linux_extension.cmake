find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod>=2.0)

# LVGL's LV_CONF_PATH is stringified with a macro (see lv_conf_internal.h),
# and preprocessor tokens in the path are macro-expanded first: with GNU
# dialects "linux" is a predefined macro, so any path containing "linux"
# (this repo!) turns into a bogus "/extras/porting/1/...". Use the standard
# LV_CONF_INCLUDE_SIMPLE mechanism instead and put lv_conf.h on the include
# path of the lvgl target; the definition is PUBLIC, so our sources see the
# same configuration.
unset(LV_CONF_PATH CACHE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.2.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lvgl)
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set(_DHM_DIR ${SUPLA_LINUX_PORT_DIR}/displayhatmini)

supla_linux_register_extension(
  NAME displayhatmini
  INIT_FUNCTION initDisplayHatMiniExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/displayhatmini_extension.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hat_gpio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hat_display.cpp
    ${_DHM_DIR}/hat_model.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_DHM_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
  LIBRARIES
    lvgl
    PkgConfig::GPIOD
)
