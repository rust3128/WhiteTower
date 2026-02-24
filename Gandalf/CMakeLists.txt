cmake_minimum_required(VERSION 3.16)
project(Gandalf VERSION 3.0.0)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Знаходимо Qt і необхідні компоненти для GUI-додатку
# Core - базовий модуль, Gui - для вікон, Widgets - для кнопок, полів і т.д.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent Network Core5Compat)

# Список файлів з вихідним кодом для нашого додатку
add_executable(Gandalf WIN32
    main.cpp
    res/resources.rc
    MainWindow.h
    MainWindow.cpp
    mainwindow.ui
    resource.qrc
    Users/userlistdialog.h Users/userlistdialog.cpp Users/userlistdialog.ui
    Users/usereditdialog.h Users/usereditdialog.cpp Users/usereditdialog.ui
    Clients/clientslistdialog.h Clients/clientslistdialog.cpp Clients/clientslistdialog.ui
    Settings/settingsdialog.h Settings/settingsdialog.cpp Settings/settingsdialog.ui
    ApplicationController.h
    ApplicationController.cpp
    Clients/objectslistdialog.h Clients/objectslistdialog.cpp Clients/objectslistdialog.ui
    Settings/exporttasksdialog.h Settings/exporttasksdialog.cpp Settings/exporttasksdialog.ui
    Clients/SyncManager.h
    Clients/SyncManager.cpp
    Clients/syncstatusdialog.h Clients/syncstatusdialog.cpp Clients/syncstatusdialog.ui
    Settings/SqlHighlighter.h
    Settings/SqlHighlighter.cpp
    Terminals/StationSearchWidget.h
    Terminals/StationSearchWidget.cpp
    Terminals/StationDataContext.h
    Terminals/StationDataContext.cpp
    Terminals/generalinfowidget.h Terminals/generalinfowidget.cpp Terminals/generalinfowidget.ui
    Terminals/poscardwidget.h Terminals/poscardwidget.cpp Terminals/poscardwidget.ui
    Terminals/workplacewidget.h Terminals/workplacewidget.cpp Terminals/workplacewidget.ui
    Terminals/workplacedata.h
    Terminals/workplacedata.cpp
    Terminals/pingdialog.cpp Terminals/pingdialog.h Terminals/pingdialog.ui Terminals/pingmodel.cpp Terminals/pingmodel.h



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
    Qt6::Concurrent
    Qt6::Network
    Qt6::Core5Compat
    Oracle        # <-- Наша спільна бібліотека
)

# Підключаємо нашу систему версіонування (опціонально, але рекомендовано)
include(../versioning.cmake)
