cmake_minimum_required(VERSION 3.16)

project(Exporter LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Sql Network REQUIRED)

add_executable(Exporter
  main.cpp
  Exporter.h
  Exporter.cpp
)
# Підключаємо Qt
target_link_libraries(Exporter PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Oracle # <-- Якщо 'Oracle' це бібліотека
)

target_include_directories(Exporter PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "../Oracle" # Додаємо шлях до заголовків нашої бібліотеки
)

