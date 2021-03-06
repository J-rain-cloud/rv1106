cmake_minimum_required(VERSION 3.5)

project(rkipc)

option(COMPILE_FOR_RV1106_IPC "compile for rv1106 ipc" OFF)
option(COMPILE_FOR_RV1106_BATTERY_IPC "compile for rv1106 battery ipc" OFF)
option(COMPILE_FOR_RV1126_RKMEDIA "compile for rv1126 ipc rkmedia" OFF)
option(COMPILE_FOR_RV1126_ROCKIT "compile for rv1126 ipc rockit" OFF)
option(COMPILE_FOR_RV1126_BATTERY_IPC "compile for rv1126 battery ipc" OFF)
option(COMPILE_FOR_RV1126_SNAPSHOT "compile for rv1126 snapshot" OFF)
option(COMPILE_FOR_RK3588 "compile for rk3588 ipc" OFF)
option(COMPILE_FOR_RK3588_MULTI_IPC "compile for rk3588 multi-ipc" OFF)


set(RKIPC_CROSS_COMPILE arm-rockchip830-linux-uclibcgnueabihf)
message(STATUS "CROSS COMPILE TOOL: ${RKIPC_CROSS_COMPILE}")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-rpath-link,${PROJECT_SOURCE_DIR}/lib/${RKIPC_CROSS_COMPILE}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wl,-rpath-link,${PROJECT_SOURCE_DIR}/lib/${RKIPC_CROSS_COMPILE}")

if(COMPILE_FOR_RV1106_IPC)
  add_subdirectory(src/rv1106_ipc)
endif()

if(COMPILE_FOR_RV1106_BATTERY_IPC)
  add_subdirectory(src/rv1106_battery_ipc)
endif()

if(COMPILE_FOR_RV1126_RKMEDIA)
  add_subdirectory(src/rv1126_ipc_rkmedia)
endif()

if(COMPILE_FOR_RV1126_ROCKIT)
  add_subdirectory(src/rv1126_ipc_rockit)
endif()

if(COMPILE_FOR_RV1126_BATTERY_IPC)
  add_subdirectory(src/rv1126_battery_ipc)
endif()

if(COMPILE_FOR_RV1126_SNAPSHOT)
  add_subdirectory(src/rv1126_snapshot)
endif()

if(COMPILE_FOR_RK3588)
  add_subdirectory(src/rk3588_ipc)
endif()

if(COMPILE_FOR_RK3588_MULTI_IPC)
  add_subdirectory(src/rk3588_multi_ipc)
endif()
