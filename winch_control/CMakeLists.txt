cmake_minimum_required(VERSION 2.8.9)
project(brewhouse)

file(GLOB TWITSOURCES "../third_party/libtwitcurl/*.cpp")

set (CMAKE_CXX_FLAGS "-std=c++11")
set (CMAKE_EXE_LINKER_FLAGS "-lcurl -lpthread")
add_library(twitcurl STATIC ${TWITSOURCES})

add_library(brewhub scale.cc gpio.cc)

add_executable(twitterbrew twitter_brew.cpp)
TARGET_LINK_LIBRARIES(twitterbrew twitcurl curl pthread)

include_directories(../third_party/rapidjson/include)
add_executable(brewhub_manual brewhub.cc)
TARGET_LINK_LIBRARIES(brewhub_manual brewhub twitcurl curl pthread)

add_executable(sheets_test sheets_test.cc)
TARGET_LINK_LIBRARIES(sheets_test brewhub twitcurl curl pthread)
