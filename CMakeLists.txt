# For local macOS development, install dependencies via Homebrew:
# brew install googletest
#
# To build and run the tests with CTest:
# cmake -DENABLE_TESTING=ON -S . -B <build_dir>
# cmake --build <build_dir> --target unit_tests
# ctest --test-dir <build_dir> -V

cmake_minimum_required(VERSION 3.21)

# Project configuration
project(CognitivePipelines VERSION 0.1.1 LANGUAGES CXX)

# --- Standardize Output Directories ---
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# For multi-config generators like MSVC, append the config type
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
endforeach()
# --------------------------------------

# FetchContent for external dependencies
include(FetchContent)

# Add QtNodes (paceholder/nodeeditor) as a dependency
FetchContent_Declare(
    QtNodes
    GIT_REPOSITORY https://github.com/paceholder/nodeeditor.git
    GIT_TAG        3.0.12
)
FetchContent_MakeAvailable(QtNodes)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt's automatic processing (safe even if not used yet)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Dependencies resolved from the system (no FetchContent)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Test)
find_package(Boost 1.70 REQUIRED)
find_package(cpr REQUIRED)
find_package(GTest REQUIRED)
# Explicitly find transitive dependencies as good practice
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)



# Sources
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(CognitivePipelines
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow.h
    ${SRC_DIR}/about_dialog.cpp
    ${SRC_DIR}/about_dialog.h
    ${SRC_DIR}/PythonScriptConnector.cpp
    ${SRC_DIR}/PythonScriptConnector.h
    ${SRC_DIR}/llm_api_client.cpp
    ${SRC_DIR}/llm_api_client.h
    ${SRC_DIR}/LLMConnector.cpp
    ${SRC_DIR}/LLMConnector.h
    ${SRC_DIR}/LLMConnectorPropertiesWidget.cpp
    ${SRC_DIR}/LLMConnectorPropertiesWidget.h
    ${SRC_DIR}/PromptBuilderNode.cpp
    ${SRC_DIR}/PromptBuilderNode.h
    ${SRC_DIR}/PromptBuilderPropertiesWidget.cpp
    ${SRC_DIR}/PromptBuilderPropertiesWidget.h
    ${SRC_DIR}/TextInputNode.cpp
    ${SRC_DIR}/TextInputNode.h
    ${SRC_DIR}/TextInputPropertiesWidget.cpp
    ${SRC_DIR}/TextInputPropertiesWidget.h
    ${SRC_DIR}/NodeGraphModel.cpp
    ${SRC_DIR}/NodeGraphModel.h
    ${SRC_DIR}/ToolNodeDelegate.cpp
    ${SRC_DIR}/ToolNodeDelegate.h
    ${SRC_DIR}/ExecutionEngine.cpp
    ${SRC_DIR}/ExecutionEngine.h
)


# Link libraries
# Add public headers directory to include path
target_include_directories(CognitivePipelines PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SRC_DIR}
)

target_link_libraries(CognitivePipelines PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    QtNodes::QtNodes
    Boost::boost
    cpr::cpr
)

# Define build information macros for use in C++
# Capture short git commit hash (fallback to "unknown" if not available)
set(APP_VERSION "${PROJECT_VERSION}")
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

# Pass definitions to the target as string literals
# Quotes are embedded so they become proper C string literals
string(REPLACE "\"" "\\\"" APP_VERSION_ESCAPED "${APP_VERSION}")
string(REPLACE "\"" "\\\"" GIT_HASH_ESCAPED "${GIT_COMMIT_HASH}")

target_compile_definitions(CognitivePipelines PRIVATE
    APP_VERSION="${APP_VERSION_ESCAPED}"
    GIT_COMMIT_HASH="${GIT_HASH_ESCAPED}"
)

# On macOS and Windows, optionally create a GUI app without console if needed
if(WIN32)
    # Uncomment to build a WIN32 subsystem GUI app:
    set_target_properties(CognitivePipelines PROPERTIES WIN32_EXECUTABLE YES)
elseif(APPLE)
    # Uncomment to build a macOS app bundle:
    set_target_properties(CognitivePipelines PROPERTIES MACOSX_BUNDLE YES)
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS CognitivePipelines
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

# CPack setup for simple cross-platform installers
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "CognitivePipelines")
set(CPACK_PACKAGE_VENDOR "CognitivePipelines Authors")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "support@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CognitivePipelines application")

# Generator suggestions (users can choose others)
set(CPACK_GENERATOR "ZIP;TGZ")
if(WIN32)
    list(APPEND CPACK_GENERATOR NSIS)
elseif(APPLE)
    list(APPEND CPACK_GENERATOR DragNDrop)
endif()

# --- Testing setup (optional, off by default) ---
option(ENABLE_TESTING "Build unit/integration tests" OFF)

if(ENABLE_TESTING)
    message(STATUS "ENABLE_TESTING=ON: building test suite")
    include(CTest)
    enable_testing()

    # Unit Tests (GTest)
    add_executable(unit_tests
            tests/test_llm_api_client.cpp
            tests/test_nodes.cpp
            tests/test_execution_engine.cpp
            # Sources required by the unit tests
            ${SRC_DIR}/llm_api_client.cpp
            ${SRC_DIR}/llm_api_client.h
            ${SRC_DIR}/ExecutionEngine.cpp
            ${SRC_DIR}/ExecutionEngine.h
            ${SRC_DIR}/NodeGraphModel.cpp
            ${SRC_DIR}/NodeGraphModel.h
            ${SRC_DIR}/ToolNodeDelegate.cpp
            ${SRC_DIR}/ToolNodeDelegate.h
            ${SRC_DIR}/PromptBuilderNode.cpp
            ${SRC_DIR}/PromptBuilderNode.h
            ${SRC_DIR}/PromptBuilderPropertiesWidget.cpp
            ${SRC_DIR}/PromptBuilderPropertiesWidget.h
            ${SRC_DIR}/TextInputNode.cpp
            ${SRC_DIR}/TextInputNode.h
            ${SRC_DIR}/TextInputPropertiesWidget.cpp
            ${SRC_DIR}/TextInputPropertiesWidget.h
            ${SRC_DIR}/LLMConnector.cpp
            ${SRC_DIR}/LLMConnector.h
            ${SRC_DIR}/LLMConnectorPropertiesWidget.cpp
            ${SRC_DIR}/LLMConnectorPropertiesWidget.h
    )
    target_include_directories(unit_tests PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(unit_tests PRIVATE
            GTest::gtest_main # Use vcpkg target
            cpr::cpr
            Qt6::Core
            Qt6::Widgets
            Qt6::Concurrent
            QtNodes::QtNodes
            Boost::boost
    )
    # Add unit tests to CTest
    include(GoogleTest)
    gtest_discover_tests(unit_tests) # Discover tests FOR the unit_tests target

    # Integration tests (Qt Test based, headless) - Definition looks okay
    add_executable(integration_tests
            tests/test_integration.cpp
            ${SRC_DIR}/mainwindow.cpp
            ${SRC_DIR}/mainwindow.h
            ${SRC_DIR}/about_dialog.cpp
            ${SRC_DIR}/about_dialog.h
            ${SRC_DIR}/PythonScriptConnector.cpp
            ${SRC_DIR}/PythonScriptConnector.h
            ${SRC_DIR}/llm_api_client.cpp
            ${SRC_DIR}/llm_api_client.h
            ${SRC_DIR}/LLMConnector.cpp
            ${SRC_DIR}/LLMConnector.h
            ${SRC_DIR}/LLMConnectorPropertiesWidget.cpp
            ${SRC_DIR}/LLMConnectorPropertiesWidget.h
            ${SRC_DIR}/PromptBuilderNode.cpp
            ${SRC_DIR}/PromptBuilderNode.h
            ${SRC_DIR}/PromptBuilderPropertiesWidget.cpp
            ${SRC_DIR}/PromptBuilderPropertiesWidget.h
            ${SRC_DIR}/TextInputNode.cpp
            ${SRC_DIR}/TextInputNode.h
            ${SRC_DIR}/TextInputPropertiesWidget.cpp
            ${SRC_DIR}/TextInputPropertiesWidget.h
            ${SRC_DIR}/NodeGraphModel.cpp
            ${SRC_DIR}/NodeGraphModel.h
            ${SRC_DIR}/ToolNodeDelegate.cpp
            ${SRC_DIR}/ToolNodeDelegate.h
            ${SRC_DIR}/ExecutionEngine.cpp
            ${SRC_DIR}/ExecutionEngine.h
    )

    target_include_directories(integration_tests PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(integration_tests PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::Concurrent
            QtNodes::QtNodes
            Boost::boost
            cpr::cpr
    )
    # Add integration tests to CTest (can have the same name as executable)
    add_test(NAME integration_tests COMMAND integration_tests) # Explicitly add to CTest
endif()

include(CPack)
