set(CURRENT_TARGET rocketjoe)

set(${CURRENT_TARGET}_headers
        configuration.hpp
        init_service.hpp
)

set(${CURRENT_TARGET}_sources
        configuration.cpp
        init_service.cpp
        main.cpp
)

include_directories(${Python3_INCLUDE_DIRS})

add_executable(${CURRENT_TARGET} ${${CURRENT_TARGET}_sources} ${${CURRENT_TARGET}_headers})

target_link_libraries(
        ${CURRENT_TARGET} PRIVATE
        rocketjoe::log
        rocketjoe::configuration
        rocketjoe::python_sandbox
        rocketjoe::process_pool
        dl
        ${CONAN_LIBS}
)



if (DEV_MODE)



endif ()

#install(TARGETS
#        ${CURRENT_TARGET}

#        RUNTIME DESTINATION bin
#        LIBRARY DESTINATION lib
#        ARCHIVE DESTINATION lib
#)
