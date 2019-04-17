cmake_minimum_required(VERSION 2.8.9)
project(brewhouse)

# -----------  Google Test -----------------------------------
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()

# Now simply link against gtest or gtest_main as needed. Eg
# add_executable(example example.cpp)
# target_link_libraries(example gtest_main)
# add_test(NAME example_test COMMAND example)
# ----------  End Google Test stuff --------------------


file(GLOB TWITSOURCES "../third_party/libtwitcurl/*.cpp")

set (CMAKE_CXX_FLAGS "-std=c++14 -Wall -g")
set (CMAKE_EXE_LINKER_FLAGS "-lcurl -lpthread")
add_library(twitcurl STATIC ${TWITSOURCES})


add_library(scale raw_scale.cc scale_filter.cc)
TARGET_LINK_LIBRARIES(scale pthread)


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
TARGET_LINK_LIBRARIES(scale_test scale brewhub pthread)

add_executable(physical_tests physical_tests.cc)
TARGET_LINK_LIBRARIES(physical_tests scale brewhub pthread)

add_executable(manual_tests manual_tests.cc)
TARGET_LINK_LIBRARIES(manual_tests scale brewhub pthread)

add_executable(scale_alarm_test scale_alarm_test.cc)
TARGET_LINK_LIBRARIES(scale_alarm_test scale brewhub pthread)

add_executable(serial_test serial_test.cc)

add_executable(brew_types_test brew_types_test.cc)
target_link_libraries(brew_types_test brewhub gtest_main)
add_test(NAME brew_types_test COMMAND brew_types_test)

add_executable(fake_scale_test fake_scale_test.cc)
target_link_libraries(fake_scale_test scale brewhub gtest_main)
add_test(NAME fake_scale_test COMMAND fake_scale_test)

# set(wxWidgets_CONFIGURATION mswu)
# find_package(wxWidgets COMPONENTS core base REQUIRED)
# include(${wxWidgets_USE_FILE})
# 
# add_executable(wxhello hello.cpp)
# target_link_libraries(wxhello ${wxWidgets_LIBRARIES})
