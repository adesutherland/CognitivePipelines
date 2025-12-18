if(NOT DEFINED APP_BUNDLE OR NOT DEFINED CODESIGN_EXECUTABLE OR NOT DEFINED ENTITLEMENTS OR NOT DEFINED SIGN_IDENTITY)
    message(FATAL_ERROR "APP_BUNDLE, CODESIGN_EXECUTABLE, ENTITLEMENTS, and SIGN_IDENTITY must be defined")
endif()

set(_qtwebengine_items
    "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"
    "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/QtWebEngineCore"
    "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineWidgets.framework/Versions/A/QtWebEngineWidgets"
)

foreach(_item IN LISTS _qtwebengine_items)
    if(EXISTS "${_item}")
        execute_process(
            COMMAND "${CODESIGN_EXECUTABLE}" --force --options runtime --entitlements "${ENTITLEMENTS}" --sign "${SIGN_IDENTITY}" "${_item}"
            RESULT_VARIABLE _codesign_result
            OUTPUT_VARIABLE _codesign_stdout
            ERROR_VARIABLE _codesign_stderr
        )
        if(NOT _codesign_result EQUAL 0)
            message(FATAL_ERROR "codesign failed for ${_item}: ${_codesign_stderr}")
        else()
            message(STATUS "codesigned ${_item}")
        endif()
    else()
        message(STATUS "Skipping codesign; path not found: ${_item}")
    endif()
endforeach()
