cmake_minimum_required (VERSION 3.5)

file(GLOB_RECURSE SOURCE_FILES *.cpp *.c)

if (NOT DEFINED COMPONENT_DIR)

    project (hdlc_demo_multithread)

    add_executable(hdlc_demo_multithread ${SOURCE_FILES})

    target_link_libraries(hdlc_demo_multithread tinyproto)

    if (WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(${PROJECT_NAME} Threads::Threads)

    elseif (UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(${PROJECT_NAME} Threads::Threads)

    elseif (APPLE)
        target_link_libraries(tinyproto PRIVATE
            "-framework IOKit"
            "-framework CoreFoundation"
        )
    endif()

else()

    idf_component_register(SRCS ${SOURCE_FILES}
                           INCLUDE_DIRS ".")

endif()
