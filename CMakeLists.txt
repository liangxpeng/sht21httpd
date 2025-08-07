cmake_minimum_required(VERSION 3.0)

project(sht21httpd)

add_executable(sht21httpd i2c.cc sht21httpd.cc)

find_library(MICROHTTPD_LIB microhttpd)
target_link_libraries(sht21httpd PRIVATE
                      ${MICROHTTPD_LIB}
                     pthread
                    )
