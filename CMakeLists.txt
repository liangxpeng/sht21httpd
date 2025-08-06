cmake_minimum_required(VERSION 3.26.5)

project(test)

#include_directories(${PROJECT_SOURCE_DIR})

add_executable(test i2c.cc sht21httpd.cc)

find_library(MICROHTTPD_LIB microhttpd)
target_link_libraries(test PRIVATE
                      ${MICROHTTPD_LIB}
                      pthread
                     )
