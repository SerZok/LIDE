set(TEMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/install_temp")
set(APP_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin") 
file(REMOVE_RECURSE ${TEMP_DIR})

# Копируем exe
file(COPY 
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.exe"
    DESTINATION "${TEMP_DIR}"
)

# Переводы
file(MAKE_DIRECTORY "${TEMP_DIR}/translations")
foreach(lang ru_RU en_US)
    set(src "${APP_BIN_DIR}/translations/LIDE_${lang}.qm")
    if(EXISTS "${src}")
        file(COPY "${src}" DESTINATION "${TEMP_DIR}/translations/")
        message(STATUS "  + LIDE_${lang}.qm")
    endif()
endforeach()

set(QT_TRANSLATIONS "")
if(DEFINED ENV{QTDIR})
    set(QT_TRANSLATIONS "$ENV{QTDIR}/translations")
elseif(CMAKE_PREFIX_PATH)
    foreach(prefix ${CMAKE_PREFIX_PATH})
        if(EXISTS "${prefix}/translations/qt_ru.qm")
            set(QT_TRANSLATIONS "${prefix}/translations")
            break()
        endif()
    endforeach()
endif()

if(QT_TRANSLATIONS AND EXISTS "${QT_TRANSLATIONS}")
    foreach(lang ru en)
        set(src "${QT_TRANSLATIONS}/qt_${lang}.qm")
        if(EXISTS "${src}")
            file(COPY "${src}" DESTINATION "${TEMP_DIR}/translations/")
            message(STATUS "  + qt_${lang}.qm")
        endif()
    endforeach()
else()
    message(WARNING "  ! Не найдены системные переводы Qt. Проверьте QTDIR")
endif()

# windeployqt
message(STATUS "Запуск windeployqt...")
execute_process(
    COMMAND ${WINDEPLOYQT} 
        "${TEMP_DIR}/${PROJECT_NAME}.exe"
        --verbose 0
        --release
        --no-network
        --no-opengl
        --no-opengl-sw
        --no-3dcore
        --no-translations
        --no-compiler-runtime
        --no-system-d3d-compiler
        --no-system-dxc-compiler
        --dir "${TEMP_DIR}"
    WORKING_DIRECTORY ${TEMP_DIR}
    RESULT_VARIABLE DEPLOY_RESULT
)

if(NOT DEPLOY_RESULT EQUAL 0)
    message(WARNING "windeployqt завершился с ошибкой")
endif()

if(EXISTS "${ROOT_PATH}/libs/SBCL")
    message(STATUS "Копирование SBCL...")
    file(COPY "${ROOT_PATH}/libs/SBCL"  DESTINATION "${TEMP_DIR}"
    )
endif()

if(EXISTS "${ROOT_PATH}/src/docs")
    message(STATUS "Копирование Manual...")
    file(COPY "${ROOT_PATH}/src/docs"  DESTINATION "${TEMP_DIR}"
    )
endif()

# ZIP архив
set(ZIP_NAME "${SETUP_DIR}/${PROJECT_NAME}_${PROJECT_VERSION}.zip")
message(STATUS "Создание портативной ZIP версии...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${ZIP_NAME}" --format=zip "."
        WORKING_DIRECTORY ${TEMP_DIR}
        RESULT_VARIABLE ZIP_RESULT
    )

if(ZIP_RESULT EQUAL 0)
    message(STATUS "Портативная ZIP версия создана: ${ZIP_NAME}")
else()
    message(WARNING "Не удалось создать ZIP архив")
endif()

# Inno Setup (Установщик)
set(INNO_PATH "")
foreach(PATH 
    "C:/Program Files (x86)/Inno Setup 6/ISCC.exe"
    "C:/Program Files/Inno Setup 6/ISCC.exe"
    "C:/Program Files (x86)/Inno Setup 5/ISCC.exe"
    "C:/Program Files/Inno Setup 5/ISCC.exe"
)
    if(EXISTS "${PATH}")
        set(INNO_PATH "${PATH}")
        break()
    endif()
endforeach()

if(NOT INNO_PATH)
    message(FATAL_ERROR "Inno Setup не найден! Сайт: https://jrsoftware.org/isdl.php#stable")
endif()

# Запускаем Inno Setup
message(STATUS "Запуск Inno Setup...")

execute_process(
    COMMAND "${INNO_PATH}" 
        "/q"
        "${ROOT_PATH}/install/windows_install.iss"
        "/dMyAppName=${PROJECT_NAME}"
        "/dMyAppVersion=${PROJECT_VERSION}"
        "/dSourceDir=${TEMP_DIR}"
        "/dOutputDir=${SETUP_DIR}"
        "/dIconPath=${ICON_PATH}"
        RESULT_VARIABLE INNO_RESULT
)

if(INNO_RESULT EQUAL 0)
    message(STATUS "Установщик создан: ${ROOT_PATH}/setups/${PROJECT_NAME}_Setup_${PROJECT_VERSION}.exe")
    file(REMOVE_RECURSE ${TEMP_DIR})
else()
    message(FATAL_ERROR "Ошибка создания установщика: ${INNO_ERROR}")
endif()