cmake_minimum_required(VERSION 3.16)

project(Conduit LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql Network HttpServer)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Sql Network HttpServer)

add_executable(Conduit
  main.cpp
  WebServer.h
  WebServer.cpp
)

target_include_directories(Conduit PRIVATE
#    "${CMAKE_CURRENT_BINARY_DIR}"
#    "${CMAKE_SOURCE_DIR}/Oracle"
    "${CMAKE_SOURCE_DIR}"
)


target_link_libraries(Conduit PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::HttpServer
    Oracle)

include(GNUInstallDirs)
install(TARGETS Conduit
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
