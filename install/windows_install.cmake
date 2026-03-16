set(TEMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/install_temp")
file(REMOVE_RECURSE ${TEMP_DIR})

# Копируем exe
file(COPY 
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.exe"
    DESTINATION "${TEMP_DIR}"
)

# windeployqt
message(STATUS "Запуск windeployqt...")
execute_process(
    COMMAND ${WINDEPLOYQT} 
        "${TEMP_DIR}/${PROJECT_NAME}.exe"
        --verbose 0
        --release
        --no-translations
        --no-network
        --no-opengl
        --no-3dcore
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

# ZIP архив
set(ZIP_NAME "${SETUP_DIR}/${PROJECT_NAME}_${PROJECT_VERSION}.zip")
message(STATUS "Создание портативной ZIP версии...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${ZIP_NAME}" --format=zip "."
        WORKING_DIRECTORY ${TEMP_DIR}
        RESULT_VARIABLE ZIP_RESULT
    )

if(ZIP_RESULT EQUAL 0)
    message(STATUS "Портативная версия создана: ${ZIP_NAME}")
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