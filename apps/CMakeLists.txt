include_directories(${CMAKE_SOURCE_DIR})

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

endif()

add_subdirectory(rocketjoe)
add_subdirectory(rocketjoe_engine)
add_subdirectory(rocketjoe_kernel)
add_subdirectory(wsgi_module)