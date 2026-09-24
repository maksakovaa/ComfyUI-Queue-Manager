cmake_minimum_required(VERSION 3.16)
project(ComfyUIQueueManager VERSION 2.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/ComfyApi.h src/ComfyApi.cpp
    src/QueueManager.h src/QueueManager.cpp
    src/Job.h
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
