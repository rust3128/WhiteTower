cmake_minimum_required(VERSION 3.16)

# Встановлюємо версію тут. Номер збірки додасться автоматично.
project(Conduit LANGUAGES CXX VERSION 1.0)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Знаходимо Qt один раз, перераховуючи всі потрібні компоненти
find_package(Qt6 REQUIRED COMPONENTS Core Sql Network HttpServer)

# Вказуємо лише ті файли, які потрібно компілювати
add_executable(Conduit
    main.cpp
    WebServer.h
    WebServer.cpp
)

target_include_directories(Conduit PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(Conduit PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::HttpServer
    Oracle
)

include(GNUInstallDirs)
install(TARGETS Conduit
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# --- Підключення версіонування та документації ---

# Цей скрипт тепер працює з коректно визначеним проектом
include(../versioning.cmake)

# Ця команда тепер буде коректно оброблена, бо структура проекту правильна
add_custom_target(docs
    COMMAND apidoc -i ${CMAKE_CURRENT_SOURCE_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/apidoc
    COMMENT "Generating API documentation with apiDoc..."
)
