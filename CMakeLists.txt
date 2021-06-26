cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(BatteryAlarm)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Battery Alarm")
set(VITA_TITLEID "BALM00001")
set(VITA_VERSION "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
	main.c
	debugScreen.c
)

target_link_libraries(${PROJECT_NAME}
  taihen_stub
  taihenModuleUtils_stub
  SceLibKernel_stub
  SceKernelModulemgr_stub
  SceKernelThreadmgr_stub
  SceProcessmgr_stub
  SceDisplay_stub
  SceIofilemgr_stub
  ScePower_stub
  SceRtc_stub
  SceTriggerUtil_stub
  SceSysmodule_stub
  SceCtrl_stub
  SceAVConfig_stub
  k
  gcc
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
	VERSION ${VITA_VERSION}
	NAME ${VITA_APP_NAME}
	FILE icon0.png sce_sys/icon0.png
	FILE bg.png sce_sys/livearea/contents/bg.png
	FILE startup.png sce_sys/livearea/contents/startup.png
	FILE template.xml sce_sys/livearea/contents/template.xml
	FILE alarm.wav alarm.wav
	FILE incoming_dialog.xml sce_sys/incoming_dialog.xml
)
