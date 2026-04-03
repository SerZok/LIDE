cmake_minimum_required(VERSION 3.19)
set (PROJECT_NAME LIDE)
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/version.h"
    @ONLY
)

project(${PROJECT_NAME} 
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Lisp IDE"
    LANGUAGES CXX
)

if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:\\Qt\\6.10.2\\msvc2022_64" CACHE PATH "Qt installation path")
endif()

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
    "src/ui/dialogs/*.ui"
    "src/ui/*.ui"
    "src/*.ui"
)

file(GLOB_RECURSE PROJECT_RESOURCES
    "src/*.qrc"
)

# Генерация стилей
function(generate_theme theme_name)
    set(COLORS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/styles/colors/${theme_name}.colors")
    message(STATUS "Генерация стиля [${theme_name}] используя цвета из: ${COLORS_FILE}")

    file(STRINGS ${COLORS_FILE} COLOR_LINES)

    foreach(line ${COLOR_LINES})
        string(STRIP "${line}" line)
        string(REGEX REPLACE "\r$" "" line "${line}")
        
        if(NOT line MATCHES "^#" AND NOT line STREQUAL "")
            string(REGEX MATCH "^([^=]+)=(.*)$" match "${line}")
            if(CMAKE_MATCH_1)
                set(${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
            else()
                message(WARNING "Could not parse line: '${line}'")
            endif()
        endif()
    endforeach()

    # Генерируем CSS файл в бинарную директорию
   configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/styles/template.css.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/styles/${theme_name}.css"
        @ONLY
    )
endfunction()

# Генерируем темы
generate_theme("dark")
generate_theme("light")

set(RESOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources.rc
)

# Создание exe'шника
qt_add_executable(${PROJECT_NAME} 
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_UIS}
    ${PROJECT_RESOURCES}
    ${RESOURCE_FILES}
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
        COMMAND ${WINDEPLOYQT} $<TARGET_FILE:${PROJECT_NAME}> 
        --no-translations
        --no-translations
        --no-network
        --no-opengl
        --no-quick
        --no-compiler-runtime
        --no-system-d3d-compiler
        --no-system-dxc-compiler
        --verbose 0
        COMMENT "Running windeployqt..."
    )

    # Копирование sbcl
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SBCL" 
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sbcl"
    COMMENT "Copying SBCL to output directory..."
)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

#================================[ РАЗВЁРТЫВАНИЕ ]================================
if(WIN32)

    install(CODE "
        set(PROJECT_NAME \"${PROJECT_NAME}\")
        set(PROJECT_VERSION \"${PROJECT_VERSION}\")
        set(PROJECT_VERSION_MAJOR \"${PROJECT_VERSION_MAJOR}\")
        set(PROJECT_VERSION_MINOR \"${PROJECT_VERSION_MINOR}\")
        set(PROJECT_VERSION_PATCH \"${PROJECT_VERSION_PATCH}\")
        set(WINDEPLOYQT \"${WINDEPLOYQT}\")
        set(SETUP_DIR \"${CMAKE_CURRENT_SOURCE_DIR}/setups\")
        set(ICON_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/src/images/logo.ico\")
        set(ROOT_PATH \"${CMAKE_CURRENT_SOURCE_DIR}\")

        include(\"${CMAKE_CURRENT_SOURCE_DIR}/install/windows_install.cmake\")
    ")

endif()
