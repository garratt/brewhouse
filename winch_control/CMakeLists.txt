cmake_minimum_required(VERSION 2.8.9)
project(brewhouse)
 
#Bring the headers, such as Student.h into the project
include_directories(include)
 
#Can manually add the sources using the set command as follows:
#set(SOURCES src/mainapp.cpp src/Student.cpp)
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB TWITSOURCES "../third_party/libtwitcurl/*.cpp")
 

add_library(twitcurl STATIC ${TWITSOURCES})


add_executable(twitterbrew twitter_brew.cpp)
set (CMAKE_CXX_FLAGS "-lcurl -lpthread")
TARGET_LINK_LIBRARIES(twitterbrew twitcurl)
target_compile_options(twitterbrew PRIVATE -lcurl -lpthread)

include_directories(../third_party/rapidjson/include)
add_executable(brewhub brewhub.cc)
add_executable(sheets_test sheets_test.cc)
