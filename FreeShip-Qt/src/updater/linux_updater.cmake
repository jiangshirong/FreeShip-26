# Linux update helper (FreeShipUpdater), counterpart of tools/updates/Updater.cs.
# Trust key, family and feed come from tools/updates/channel.json at configure
# time; the Linux feed is <Windows feed directory>/linux-amd64/latest.json.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(ZLIB REQUIRED)

set(FS_UPDATE_CHANNEL "${CMAKE_CURRENT_SOURCE_DIR}/tools/updates/channel.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FS_UPDATE_CHANNEL}")
file(READ "${FS_UPDATE_CHANNEL}" FS_UPDATE_CHANNEL_JSON)
string(JSON FS_UPDATE_WINDOWS_ENDPOINT GET "${FS_UPDATE_CHANNEL_JSON}" endpoint)
string(JSON FS_UPDATE_PUBLIC_KEY GET "${FS_UPDATE_CHANNEL_JSON}" public_key_xml)
string(JSON FS_UPDATE_FAMILY GET "${FS_UPDATE_CHANNEL_JSON}" family)
if(NOT FS_UPDATE_WINDOWS_ENDPOINT MATCHES "^https://.+/latest\\.json$")
  message(FATAL_ERROR "Unexpected update endpoint in ${FS_UPDATE_CHANNEL}")
endif()
string(REGEX REPLACE "/latest\\.json$" "/linux-amd64/latest.json" FS_UPDATE_LINUX_ENDPOINT "${FS_UPDATE_WINDOWS_ENDPOINT}")
set(FS_UPDATE_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/updater")
file(WRITE "${FS_UPDATE_HEADER_DIR}/updater_channel.h.in"
"// Generated from tools/updates/channel.json. Do not edit.\n#pragma once\nnamespace fs::updater::channel {\n\
inline constexpr char kEndpoint[] = R\"FSU(${FS_UPDATE_LINUX_ENDPOINT})FSU\";\n\
inline constexpr char kPublicKeyXml[] = R\"FSU(${FS_UPDATE_PUBLIC_KEY})FSU\";\n\
inline constexpr char kFamily[] = R\"FSU(${FS_UPDATE_FAMILY})FSU\";\n}\n")
configure_file("${FS_UPDATE_HEADER_DIR}/updater_channel.h.in" "${FS_UPDATE_HEADER_DIR}/updater_channel.h" COPYONLY)

function(fs_add_linux_updater target test)
  add_executable(${target} src/updater/linux_updater.cpp)
  target_include_directories(${target} PRIVATE "${FS_UPDATE_HEADER_DIR}")
  target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Network OpenSSL::Crypto ZLIB::ZLIB)
  target_compile_options(${target} PRIVATE -Wall -Wextra)
  if(test)
    target_compile_definitions(${target} PRIVATE FS_UPDATER_TEST=1)
  endif()
endfunction()
# Production helper: accepts no endpoint, key or path overrides.
fs_add_linux_updater(FreeShipUpdater OFF)
# Test helper for FreeShip-Tests (loopback HTTP feed, private test key).
fs_add_linux_updater(FreeShipUpdaterTest ON)
