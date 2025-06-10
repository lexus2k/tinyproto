cmake_minimum_required (VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(EXAMPLES "Build examples and tiny_loopback" OFF)
option(UNITTEST "Build unit tests" OFF)
option(CUSTOM "Do not use built-in HAL, but use Custom instead" OFF)
option(ENABLE_FD_LOGS "Enable full duplex protocol logs" OFF)
# set(LOG_LEVEL "0" CACHE STRING "Logging level option" FORCE)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.c)
file(GLOB_RECURSE HEADER_FILES src/*.h)

if (NOT DEFINED COMPONENT_DIR)

    project (tinyproto)

    add_library(tinyproto STATIC ${HEADER_FILES} ${SOURCE_FILES})
    target_include_directories(tinyproto PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:include/tinyproto>)
    if(APPLE)
        target_link_libraries(tinyproto PRIVATE
            "-framework IOKit"
            "-framework CoreFoundation"
        )
    endif()
    if (EXAMPLES)
        add_subdirectory(examples/linux/loopback)
        add_subdirectory(examples/linux/hdlc_demo)
        add_subdirectory(examples/linux/hdlc_demo_multithread)
    endif()

    if (UNITTEST)
        add_subdirectory(unittest)
    endif()

    # Install the library and headers
    install(DIRECTORY src/ DESTINATION include/tinyproto
            FILES_MATCHING
            PATTERN "*.h"
            PATTERN "*.hpp"
            PATTERN "src/hal/single_core" EXCLUDE)
            
    install(TARGETS tinyproto
            EXPORT tinyprotoTargets
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
            INCLUDES DESTINATION include)

    install(EXPORT tinyprotoTargets
            FILE TinyProtoTargets.cmake
            NAMESPACE TinyProto::
            DESTINATION lib/cmake/tinyproto)

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TinyProtoConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/TinyProtoConfig.cmake
        INSTALL_DESTINATION lib/cmake/tinyproto)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/TinyProtoConfig.cmake
        DESTINATION lib/cmake/tinyproto)

else()

    idf_component_register(SRCS ${SOURCE_FILES}
                           INCLUDE_DIRS "src" PRIV_REQUIRES esp_timer)

endif()

if (CUSTOM)
    add_definitions("-DTINY_CUSTOM_PLATFORM=1")
endif()
if (ENABLE_FD_LOGS)
    add_definitions("-DTINY_DEBUG=1")
    add_definitions("-DTINY_FD_DEBUG=1")
    if (LOG_LEVEL)
       add_definitions("-DTINY_LOG_LEVEL_DEFAULT=${LOG_LEVEL}")
    endif()
endif()
