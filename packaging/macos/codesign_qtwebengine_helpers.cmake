if(NOT DEFINED APP_BUNDLE OR NOT DEFINED CODESIGN_EXECUTABLE OR NOT DEFINED ENTITLEMENTS OR NOT DEFINED SIGN_IDENTITY)
    message(FATAL_ERROR "APP_BUNDLE, CODESIGN_EXECUTABLE, ENTITLEMENTS, and SIGN_IDENTITY must be defined")
endif()

# Optional: MAIN_ENTITLEMENTS and IS_DEBUG_BUILD
if(NOT DEFINED MAIN_ENTITLEMENTS)
    set(MAIN_ENTITLEMENTS "${ENTITLEMENTS}")
endif()
if(NOT DEFINED IS_DEBUG_BUILD)
    set(IS_DEBUG_BUILD FALSE)
endif()

find_program(INSTALL_NAME_TOOL install_name_tool)
find_program(OTOOL otool)
find_program(XATTR xattr)

message(STATUS "APP_BUNDLE: ${APP_BUNDLE}")

if(XATTR)
    message(STATUS "Clearing extended attributes for ${APP_BUNDLE}")
    execute_process(COMMAND "${XATTR}" -cr "${APP_BUNDLE}")
endif()

# Clean up any stale codesign temporary files
file(GLOB_RECURSE _stale_cstemp "${APP_BUNDLE}/*.cstemp")
if(_stale_cstemp)
    message(STATUS "Removing stale codesign files: ${_stale_cstemp}")
    file(REMOVE ${_stale_cstemp})
endif()

set(_extra_args "")
if(NOT IS_DEBUG_BUILD)
    set(_extra_args "--options" "runtime")
endif()

# Function to sanitize a binary: remove absolute RPATHs and patch absolute LC_LOAD_DYLIB
function(sanitize_binary _bin)
    # Resolve symlinks to get the real path for otool/install_name_tool
    get_filename_component(_real_bin "${_bin}" REALPATH)
    if(NOT EXISTS "${_real_bin}")
        message(STATUS "Binary does not exist (skipping): ${_bin}")
        return()
    endif()
    
    if(INSTALL_NAME_TOOL AND OTOOL)
        message(STATUS "Sanitizing RPATHs for ${_real_bin}")
        execute_process(
            COMMAND "${OTOOL}" -l "${_real_bin}"
            OUTPUT_VARIABLE _otool_out
            RESULT_VARIABLE _otool_res
        )
        if(_otool_res EQUAL 0)
            # Extract paths from LC_RPATH commands
            string(REPLACE "\n" ";" _otool_lines "${_otool_out}")
            set(_next_is_path FALSE)
            foreach(_line IN LISTS _otool_lines)
                if(_line MATCHES "cmd LC_RPATH")
                    set(_next_is_path TRUE)
                elseif(_next_is_path AND _line MATCHES "path (.+) \\(offset [0-9]+\\)")
                    set(_rpath "${CMAKE_MATCH_1}")
                    set(_next_is_path FALSE)
                    # Check if it's an absolute path (doesn't start with @)
                    if(NOT _rpath MATCHES "^@")
                        message(STATUS "Removing absolute RPATH: ${_rpath} from ${_real_bin}")
                        execute_process(
                            COMMAND "${INSTALL_NAME_TOOL}" -delete_rpath "${_rpath}" "${_real_bin}"
                            RESULT_VARIABLE _del_res
                            ERROR_VARIABLE _del_err
                        )
                    endif()
                elseif(_next_is_path AND _line MATCHES "cmd ")
                    set(_next_is_path FALSE)
                endif()
            endforeach()
        endif()
        
        # Also sanitize any absolute LC_LOAD_DYLIB that point to /usr/local
        execute_process(
            COMMAND "${OTOOL}" -L "${_real_bin}"
            OUTPUT_VARIABLE _otool_L_out
        )
        string(REPLACE "\n" ";" _otool_L_lines "${_otool_L_out}")
        foreach(_line IN LISTS _otool_L_lines)
            if(_line MATCHES "^[ \t]+(/usr/local/[^ \t]+)")
                set(_old_path "${CMAKE_MATCH_1}")
                # We want to change it to @rpath if it's a known library we bundled
                get_filename_component(_lib_name "${_old_path}" NAME)
                
                # Check if it's a framework
                if(_old_path MATCHES "([^/]+)\\.framework")
                    set(_fw_name "${CMAKE_MATCH_1}")
                    set(_new_path "@rpath/${_fw_name}.framework/Versions/A/${_fw_name}")
                else()
                    set(_new_path "@rpath/${_lib_name}")
                endif()
                
                message(STATUS "Patching absolute dependency in ${_real_bin}: ${_old_path} -> ${_new_path}")
                execute_process(
                    COMMAND "${INSTALL_NAME_TOOL}" -change "${_old_path}" "${_new_path}" "${_real_bin}"
                    RESULT_VARIABLE _ch_res
                )
            endif()
        endforeach()
    endif()
endfunction()

# 0. Sanitize RPATHs on the helper
set(_helper_bin "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess")
if(NOT EXISTS "${_helper_bin}")
    set(_helper_bin "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess")
endif()
sanitize_binary("${_helper_bin}")

# 1. Sign Nested Frameworks and Dylibs inside the Helper App
file(GLOB _nested_frameworks "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks/*.framework")
file(GLOB _nested_dylibs "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks/*.dylib")

foreach(_item IN LISTS _nested_frameworks _nested_dylibs)
    if(EXISTS "${_item}")
        set(_target_to_sign "${_item}")
        if("${_item}" MATCHES "\\.framework$")
            get_filename_component(_fw_name "${_item}" NAME_WE)
            if(EXISTS "${_item}/Versions/A/${_fw_name}")
                sanitize_binary("${_item}/Versions/A/${_fw_name}")
            elseif(EXISTS "${_item}/${_fw_name}")
                sanitize_binary("${_item}/${_fw_name}")
            endif()
            set(_target_to_sign "${_item}")
        else()
            sanitize_binary("${_item}")
        endif()
        
        execute_process(
            COMMAND "${CODESIGN_EXECUTABLE}" --force ${_extra_args} --sign "${SIGN_IDENTITY}" "${_target_to_sign}"
            RESULT_VARIABLE _codesign_result
            ERROR_VARIABLE _codesign_stderr
        )
        if(NOT _codesign_result EQUAL 0)
            message(WARNING "codesign failed for nested item ${_item}: ${_codesign_stderr}")
        else()
            message(STATUS "codesigned nested item ${_item}")
        endif()
    endif()
endforeach()

# 2. Sign Helper App (with entitlements)
set(_helper_app "${APP_BUNDLE}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app")
if(EXISTS "${_helper_app}")
    execute_process(
        COMMAND "${CODESIGN_EXECUTABLE}" --force ${_extra_args} --entitlements "${ENTITLEMENTS}" --sign "${SIGN_IDENTITY}" "${_helper_app}"
        RESULT_VARIABLE _codesign_result
        ERROR_VARIABLE _codesign_stderr
    )
    if(NOT _codesign_result EQUAL 0)
        message(FATAL_ERROR "codesign failed for app ${_helper_app}: ${_codesign_stderr}")
    else()
        message(STATUS "codesigned app ${_helper_app}")
    endif()
endif()

# 4. Sanitize and sign all other frameworks, dylibs, and plugins in the main bundle to ensure total isolation
file(GLOB _main_frameworks "${APP_BUNDLE}/Contents/Frameworks/*.framework")
file(GLOB _main_dylibs "${APP_BUNDLE}/Contents/Frameworks/*.dylib")
file(GLOB_RECURSE _main_plugins "${APP_BUNDLE}/Contents/PlugIns/*.dylib")
file(GLOB _main_execs "${APP_BUNDLE}/Contents/MacOS/*")

set(_items_to_sign ${_main_frameworks} ${_main_dylibs} ${_main_plugins} ${_main_execs})
if(_items_to_sign)
    list(REMOVE_DUPLICATES _items_to_sign)
endif()

foreach(_item IN LISTS _items_to_sign)
    if(EXISTS "${_item}")
        if(IS_DIRECTORY "${_item}" AND NOT "${_item}" MATCHES "\\.framework$")
            continue()
        endif()

        set(_target_to_sign "${_item}")
        if("${_item}" MATCHES "\\.framework$")
            get_filename_component(_fw_name "${_item}" NAME_WE)
            if(EXISTS "${_item}/Versions/A/${_fw_name}")
                sanitize_binary("${_item}/Versions/A/${_fw_name}")
            elseif(EXISTS "${_item}/${_fw_name}")
                sanitize_binary("${_item}/${_fw_name}")
            endif()
            # For frameworks, we sign the .framework bundle directory
            set(_target_to_sign "${_item}")
        else()
            # For dylibs and executables, sanitize the file itself
            sanitize_binary("${_item}")
        endif()

        execute_process(
            COMMAND "${CODESIGN_EXECUTABLE}" --force ${_extra_args} --sign "${SIGN_IDENTITY}" "${_target_to_sign}"
            RESULT_VARIABLE _codesign_result
            ERROR_VARIABLE _codesign_stderr
        )
        if(NOT _codesign_result EQUAL 0)
            message(WARNING "codesign failed for item ${_item}: ${_codesign_stderr}")
        else()
            message(STATUS "codesigned item ${_item}")
        endif()
    endif()
endforeach()

# 5. Sign the main App Bundle itself
message(STATUS "Final codesigning for main bundle: ${APP_BUNDLE}")
execute_process(
    COMMAND "${CODESIGN_EXECUTABLE}" --force ${_extra_args} 
            --preserve-metadata=entitlements 
            --entitlements "${MAIN_ENTITLEMENTS}" 
            --sign "${SIGN_IDENTITY}" "${APP_BUNDLE}"
    RESULT_VARIABLE _codesign_result
    ERROR_VARIABLE _codesign_stderr
)
if(NOT _codesign_result EQUAL 0)
    message(FATAL_ERROR "Final codesign failed for bundle ${APP_BUNDLE}: ${_codesign_stderr}")
else()
    message(STATUS "Final codesign successful for bundle ${APP_BUNDLE}")
endif()
