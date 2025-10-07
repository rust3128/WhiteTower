# Цей скрипт призначений для підключення в CMakeLists.txt підпроектів
# Він очікує, що в теці підпроекту є файли build_number.txt та version.h.in

# Використовуємо ${PROJECT_NAME}, яке встановлено в CMakeLists.txt підпроекту (напр., "Conduit")
message(STATUS "Configuring versioning for target: ${PROJECT_NAME}")

# --- Шляхи тепер відносні до поточного підпроекту ---
set(BUILD_NUMBER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/build_number.txt")
set(VERSION_H_IN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in")
set(GENERATED_VERSION_H_PATH "${CMAKE_CURRENT_BINARY_DIR}/version.h")

# --- Читання номера збірки ---
if(NOT EXISTS "${BUILD_NUMBER_FILE}")
    file(WRITE "${BUILD_NUMBER_FILE}" "0")
endif()
file(READ "${BUILD_NUMBER_FILE}" CURRENT_BUILD_NUMBER)
string(STRIP "${CURRENT_BUILD_NUMBER}" CURRENT_BUILD_NUMBER)

# --- Налаштування змінних для version.h.in ---
set(VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(VERSION_BUILD "${CURRENT_BUILD_NUMBER}")
set(VERSION_STR "${PROJECT_VERSION}") # PROJECT_VERSION вже включає BUILD

string(TIMESTAMP BUILD_DATE "%d.%m.%Y")
string(TIMESTAMP BUILD_TIME "%H:%M:%S")
string(TIMESTAMP BUILD_DATETIME "%d.%m.%Y %H:%M:%S")

# --- Генерація version.h під час конфігурації ---
configure_file("${VERSION_H_IN_PATH}" "${GENERATED_VERSION_H_PATH}" @ONLY)

# --- Налаштування пост-збіркового скрипта ---
set(POST_BUILD_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/_post_build_increment_template.cmake.in")
set(POST_BUILD_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/post_build_script.cmake")

# Передаємо в шаблон шляхи, специфічні для цього підпроекту
configure_file("${POST_BUILD_TEMPLATE}" "${POST_BUILD_SCRIPT}" @ONLY)

# --- Додавання команди POST_BUILD ---
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P "${POST_BUILD_SCRIPT}"
    COMMENT "Incrementing build number for ${PROJECT_NAME}"
)

# Вказуємо, що тека збірки має бути в шляхах для #include
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
