cmake_minimum_required(VERSION 3.16)

project(Oracle LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Sql Network)

add_library(Oracle STATIC


  AppParams.h
  AppParams.cpp
  Logger.h
  Logger.cpp
  ConfigManager.h
  ConfigManager.cpp
  criptpass.cpp criptpass.h qaesencryption.cpp qaesencryption.h
  DbManager.h
  DbManager.cpp
  User.h
  User.cpp
  SessionManager.h
  SessionManager.cpp
  ApiClient.h
  ApiClient.cpp
  RedmineClient.h
  RedmineClient.cpp
  JiraClient.h
  JiraClient.cpp
  StationStruct.h
  IWorkplaceGenerator.h
  DatabaseWorkplaceGenerator.h
  DatabaseWorkplaceGenerator.cpp
  WorkplaceGeneratorFactory.h
)

target_link_libraries(Oracle PRIVATE Qt${QT_VERSION_MAJOR}::Core
  Qt6::Sql
  Qt6::Network
)

target_compile_definitions(Oracle PRIVATE ORACLE_LIBRARY)
