cmake_minimum_required(VERSION 2.8.9)
project(brewhouse)

file(GLOB TWITSOURCES "../third_party/libtwitcurl/*.cpp")

set (CMAKE_CXX_FLAGS "-std=c++14 -Wall")
set (CMAKE_EXE_LINKER_FLAGS "-lcurl -lpthread")
add_library(twitcurl STATIC ${TWITSOURCES})


add_library(scale raw_scale.cc scale_filter.cc)


add_library(brewhub SimulatedGrainfather.cc valves.cc brew_types.cc grainfather2.cc brew_session.cc winch.cc gpio.cc logger.h logger.cc)

add_executable(twitterbrew twitter_brew.cpp)
TARGET_LINK_LIBRARIES(twitterbrew twitcurl curl pthread)

include_directories(../third_party/rapidjson/include)
# add_executable(brewhub_manual brewhub.cc)
# TARGET_LINK_LIBRARIES(brewhub_manual brewhub twitcurl curl pthread)

add_executable(sheets_test sheets_test.cc)
TARGET_LINK_LIBRARIES(sheets_test brewhub twitcurl curl pthread)

add_executable(brew_session_test brew_session_test.cc)
TARGET_LINK_LIBRARIES(brew_session_test brewhub scale twitcurl curl pthread)

add_executable(grainfather_test grainfather_test.cc)
TARGET_LINK_LIBRARIES(grainfather_test brewhub pthread)

add_executable(scale_test scale_test.cc)
TARGET_LINK_LIBRARIES(scale_test scale brewhub)

add_executable(serial_test serial_test.cc)

# set(wxWidgets_CONFIGURATION mswu)
# find_package(wxWidgets COMPONENTS core base REQUIRED)
# include(${wxWidgets_USE_FILE})
# 
# add_executable(wxhello hello.cpp)
# target_link_libraries(wxhello ${wxWidgets_LIBRARIES})
