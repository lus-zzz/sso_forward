cmake_minimum_required(VERSION 3.0.0)
project(sso_forward VERSION 0.1.0)

include(CTest)
enable_testing()

add_definitions("/utf-8")

set(output_directory ${CMAKE_SOURCE_DIR}/release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${output_directory})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${output_directory})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_directory})


add_executable(sso_forward main.cpp)

find_package(libhv CONFIG REQUIRED)
target_link_libraries(sso_forward PRIVATE hv)

find_package(OpenSSL REQUIRED)
target_link_libraries(sso_forward PRIVATE OpenSSL::SSL OpenSSL::Crypto)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
