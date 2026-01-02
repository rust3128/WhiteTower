cmake_minimum_required(VERSION 3.16)

project(Isengard LANGUAGES CXX)

# Включаємо стандартні налаштування для Qt
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Знаходимо Qt
find_package(Qt6 COMPONENTS Core Network REQUIRED)

# Створюємо виконуваний файл
add_executable(Isengard
    main.cpp
    Bot/TelegramClient.h
    Bot/TelegramClient.cpp
    Bot/Bot.h
    Bot/Bot.cpp
    Bot/AttachmentManager.h
    Bot/AttachmentManager.cpp

)



target_include_directories(Isengard PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}"
)

# === ОСНОВНА ЗМІНА ТУТ ===
# Вказуємо, що Isengard залежить від Qt Core ТА нашої бібліотеки Oracle
target_link_libraries(Isengard PRIVATE
    Qt6::Core
    Qt6::Network
    Oracle
)
