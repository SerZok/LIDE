cmake_minimum_required(VERSION 3.19)
set (PROJECT_NAME LIDE)
set (PROJECT_VERSION 0.1.0)

project(${PROJECT_NAME} 
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Lisp IDE"
    LANGUAGES CXX
)

set(CMAKE_PREFIX_PATH "C:\\Qt\\6.10.2\\msvc2022_64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Путь к Qt
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

qt_standard_project_setup(
    I18N_SOURCE_LANGUAGE ru_RU
    I18N_TRANSLATED_LANGUAGES en_US
)

set(CMAKE_AUTOUIC_SEARCH_PATHS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

file(GLOB_RECURSE PROJECT_SOURCES 
    "src/app/*.cpp"
    "src/ui/*.cpp"
    "src/*.cpp"
)

file(GLOB_RECURSE PROJECT_HEADERS
    "src/app/*.h"
    "src/ui/*.h"
    "src/*.h"
)

file(GLOB_RECURSE PROJECT_UIS
    "src/ui/*.ui"
    "src/*.ui"
)

file(GLOB_RECURSE PROJECT_RESOURCES
    "src/*.qrc"
)

# Создание exe'шника
qt_add_executable(${PROJECT_NAME} 
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_UIS}
    ${PROJECT_RESOURCES}
)

qt_add_translations(${PROJECT_NAME}
    TS_OUTPUT_DIRECTORY "src/translations" 
)

add_dependencies(${PROJECT_NAME} update_translations)


# Подключение библиотек
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
)

# Добавляем пути для include файлов
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_BINARY_DIR}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME}  PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Запуск windeployqt после сборки
    find_program(WINDEPLOYQT windeployqt REQUIRED)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${WINDEPLOYQT} $<TARGET_FILE:${PROJECT_NAME}> --no-translations --verbose 0
        COMMENT "Running windeployqt..."
    )
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()