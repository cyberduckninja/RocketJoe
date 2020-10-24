cmake_minimum_required(VERSION 2.8.12)

cmake_policy(SET CMP0048 NEW)
set(CMAKE_PROJECT_VERSION_MAJOR 0)
set(CMAKE_PROJECT_VERSION_MINOR 1)
set(CMAKE_PROJECT_VERSION_PATCH 0)
set(CMAKE_PROJECT_VERSION "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${CMAKE_PROJECT_VERSION_PATCH}")
PROJECT(wsgi VERSION "${CMAKE_PROJECT_VERSION}")

find_package(Python3 COMPONENTS Interpreter Development)
include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${Python3_INCLUDE_DIRS})
find_package(pybind11 REQUIRED)


pybind11_add_module(${PROJECT_NAME} wsgi.cpp)

target_link_libraries(
        ${PROJECT_NAME} PRIVATE
        ${CONAN_LIBS}
        dl
)