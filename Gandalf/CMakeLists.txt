cmake_minimum_required(VERSION 3.16)
project(Gandalf VERSION 3.0.0)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Знаходимо Qt і необхідні компоненти для GUI-додатку
# Core - базовий модуль, Gui - для вікон, Widgets - для кнопок, полів і т.д.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)

# Список файлів з вихідним кодом для нашого додатку
add_executable(Gandalf
    main.cpp
    res/resources.rc
    MainWindow.h
    MainWindow.cpp
    mainwindow.ui
    resource.qrc
    Users/userlistdialog.h Users/userlistdialog.cpp Users/userlistdialog.ui
    Users/usereditdialog.h Users/usereditdialog.cpp Users/usereditdialog.ui
)

# Вказуємо, де шукати заголовні файли (зокрема, з нашої бібліотеки Oracle)
target_include_directories(Gandalf PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}"
)

# Лінкуємо наш додаток з потрібними бібліотеками
target_link_libraries(Gandalf PRIVATE
    Qt6::Widgets  # <-- Головна бібліотека для Qt Widgets
    Qt6::Sql
    Oracle        # <-- Наша спільна бібліотека
)

# Підключаємо нашу систему версіонування (опціонально, але рекомендовано)
include(../versioning.cmake)
