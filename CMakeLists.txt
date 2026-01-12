# For local macOS development, install dependencies via Homebrew:
# brew install googletest
#
# To build and run the tests with CTest:
# cmake -DENABLE_TESTING=OFF -S . -B <build_dir> (to disable)
# cmake --build <build_dir> --target unit_tests
# ctest --test-dir <build_dir> -V

cmake_minimum_required(VERSION 3.21)

# Project configuration
project(CognitivePipelines VERSION 0.12.12 LANGUAGES C CXX)

# Release build switch: control code signing identity for macOS
option(RELEASE_BUILD "Enable full codesigning for release" OFF)
# Auto-enable RELEASE_BUILD for standard Release configurations if not explicitly overridden
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
if(CMAKE_BUILD_TYPE_UPPER MATCHES "RELEASE|RELWITHDEBINFO|MINSIZEREL")
    if(NOT DEFINED RELEASE_BUILD_SET_BY_USER)
        set(RELEASE_BUILD ON CACHE BOOL "Enable full codesigning for release" FORCE)
        set(RELEASE_BUILD_SET_BY_USER TRUE CACHE INTERNAL "Flag to track if user explicitly set RELEASE_BUILD")
    endif()
endif()
# Allow user to manually toggle it; if they do -DRELEASE_BUILD=OFF/ON, it should persist.
# CMake's option() handles the cache, but since we might have forced it above, 
# we need to be careful. Actually, the requirement says "force RELEASE_BUILD to ON if...".
set(MACOS_SIGNING_IDENTITY "" CACHE STRING "macOS Code Signing Identity")
if(DEFINED ENV{MACOS_SIGNING_IDENTITY})
    set(MACOS_SIGNING_IDENTITY "$ENV{MACOS_SIGNING_IDENTITY}" CACHE STRING "macOS Code Signing Identity" FORCE)
endif()

if(RELEASE_BUILD)
    if(NOT MACOS_SIGNING_IDENTITY)
        set(APPLE_SIGN_IDENTITY "-")
        message(STATUS "macOS signing: No identity provided. Defaulting to Ad-Hoc signing (-)")
    else()
        set(APPLE_SIGN_IDENTITY "${MACOS_SIGNING_IDENTITY}")
        message(STATUS "macOS signing: Using provided identity: ${APPLE_SIGN_IDENTITY}")
    endif()
else()
    set(APPLE_SIGN_IDENTITY "-") # Use fast ad-hoc signing
    message(STATUS "macOS signing: Not a release build. Using Ad-Hoc signing (-)")
endif()

# macOS packaging resources
if(APPLE)
    # Full path to the .icns and its base file name for CFBundleIconFile
    set(MACOSX_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/CognitivePipelines.icns)
    get_filename_component(MACOSX_ICON_NAME "${MACOSX_ICON_FILE}" NAME)
    set(MACOSX_ENTITLEMENTS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/hardened_runtime.entitlements)
    set(MACOSX_QTWEBENGINE_PROCESS_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/QtWebEngineProcess.entitlements)
    set(MACOSX_INFO_PLIST_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in)
    set(MACOSX_QTWEBENGINE_CODESIGNER ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/codesign_qtwebengine_helpers.cmake)
endif()

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
message(STATUS "Fetching QtNodes...")
FetchContent_Declare(
    QtNodes
    GIT_REPOSITORY https://github.com/paceholder/nodeeditor.git
    GIT_TAG        3.0.12
)

# Manually Define the Target from local 3rdparty sources
add_library(quickjs STATIC
    src/3rdparty/quickjs/quickjs.c
    src/3rdparty/quickjs/libregexp.c
    src/3rdparty/quickjs/libunicode.c
    src/3rdparty/quickjs/cutils.c
    src/3rdparty/quickjs/dtoa.c
    src/3rdparty/quickjs/quickjs-libc.c
)

# Critical Definitions
target_compile_definitions(quickjs PUBLIC
    _GNU_SOURCE
    CONFIG_VERSION="2024-01-13"
    CONFIG_BIGNUM
)

# Include Paths
target_include_directories(quickjs PUBLIC src/3rdparty/quickjs)

# Enforce C11 for QuickJS (required for atomics on Windows/MSVC)
set_target_properties(quickjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_definitions(quickjs PRIVATE 
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    target_compile_options(quickjs PRIVATE /experimental:c11atomics)
endif()

# Explicitly build QtNodes as a shared library (required for proper resource loading)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build QtNodes as shared library" FORCE)
message(STATUS "Making QtNodes available...")
FetchContent_MakeAvailable(QtNodes)
message(STATUS "FetchContent complete.")

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt's automatic processing (safe even if not used yet)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Dependencies resolved from the system (no FetchContent)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Test Sql Pdf WebChannel Positioning WebEngineWidgets DBus)
find_package(Boost 1.70 REQUIRED)
find_package(cpr REQUIRED)
find_package(GTest REQUIRED)
# Explicitly find transitive dependencies as good practice
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)


# Windows resource file for application icon
if(WIN32)
    set(WIN32_RESOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/app.rc)
endif()

# Sources
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_executable(CognitivePipelines
    # Core headers
    ${INCLUDE_DIR}/IToolConnector.h
    ${INCLUDE_DIR}/CommonDataTypes.h
    ${INCLUDE_DIR}/IScriptHost.h
    # Application sources
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow.h
    ${SRC_DIR}/Logger.cpp
    ${SRC_DIR}/Logger.h
    ${SRC_DIR}/about_dialog.cpp
    ${SRC_DIR}/about_dialog.h
    ${SRC_DIR}/UserInputDialog.cpp
    ${SRC_DIR}/UserInputDialog.h
    ${SRC_DIR}/PythonScriptConnector.cpp
    ${SRC_DIR}/PythonScriptConnector.h
    ${SRC_DIR}/UniversalLLMPropertiesWidget.cpp
    ${SRC_DIR}/UniversalLLMPropertiesWidget.h
    ${SRC_DIR}/UniversalLLMNode.cpp
    ${SRC_DIR}/UniversalLLMNode.h
    ${SRC_DIR}/ImageGenPropertiesWidget.cpp
    ${SRC_DIR}/ImageGenPropertiesWidget.h
    ${SRC_DIR}/ImageGenNode.cpp
    ${SRC_DIR}/ImageGenNode.h
    ${SRC_DIR}/backends/ILLMBackend.h
    ${SRC_DIR}/backends/OpenAIBackend.cpp
    ${SRC_DIR}/backends/OpenAIBackend.h
    ${SRC_DIR}/backends/GoogleBackend.cpp
    ${SRC_DIR}/backends/GoogleBackend.h
    ${SRC_DIR}/backends/AnthropicBackend.cpp
    ${SRC_DIR}/backends/AnthropicBackend.h
    ${SRC_DIR}/ExecutionScriptHost.cpp
    ${SRC_DIR}/ExecutionScriptHost.h
    ${SRC_DIR}/ModelCaps.cpp
    ${SRC_DIR}/ModelCaps.h
    ${SRC_DIR}/ModelCapsRegistry.cpp
    ${SRC_DIR}/ModelCapsRegistry.h
    ${SRC_DIR}/core/LLMProviderRegistry.cpp
    ${SRC_DIR}/core/LLMProviderRegistry.h
    ${SRC_DIR}/core/DocumentLoader.cpp
    ${SRC_DIR}/core/DocumentLoader.h
    ${SRC_DIR}/core/TextChunker.cpp
    ${SRC_DIR}/core/TextChunker.h
    ${SRC_DIR}/core/RagUtils.cpp
    ${SRC_DIR}/core/RagUtils.h
    ${SRC_DIR}/RagQueryNode.cpp
    ${SRC_DIR}/RagQueryNode.h
    ${SRC_DIR}/RagQueryPropertiesWidget.cpp
    ${SRC_DIR}/RagQueryPropertiesWidget.h
    ${SRC_DIR}/chunkingstrategies/ChunkerStrategy.h
    ${SRC_DIR}/chunkingstrategies/MarkdownChunker.cpp
    ${SRC_DIR}/chunkingstrategies/MarkdownChunker.h
    ${SRC_DIR}/chunkingstrategies/StandardCodeChunker.cpp
    ${SRC_DIR}/chunkingstrategies/StandardCodeChunker.h
    ${SRC_DIR}/PromptBuilderNode.cpp
    ${SRC_DIR}/PromptBuilderNode.h
    ${SRC_DIR}/PromptBuilderPropertiesWidget.cpp
    ${SRC_DIR}/PromptBuilderPropertiesWidget.h
    ${SRC_DIR}/TextInputNode.cpp
    ${SRC_DIR}/TextInputNode.h
    ${SRC_DIR}/TextInputPropertiesWidget.cpp
    ${SRC_DIR}/TextInputPropertiesWidget.h
    ${SRC_DIR}/TextOutputNode.cpp
    ${SRC_DIR}/TextOutputNode.h
    ${SRC_DIR}/TextOutputPropertiesWidget.cpp
    ${SRC_DIR}/TextOutputPropertiesWidget.h
    ${SRC_DIR}/ImageNode.cpp
    ${SRC_DIR}/ImageNode.h
    ${SRC_DIR}/ImagePropertiesWidget.cpp
    ${SRC_DIR}/ImagePropertiesWidget.h
    ${SRC_DIR}/ImagePopupDialog.cpp
    ${SRC_DIR}/ImagePopupDialog.h
    ${SRC_DIR}/logging_categories.cpp
    ${SRC_DIR}/MermaidNode.cpp
    ${SRC_DIR}/MermaidNode.h
    ${SRC_DIR}/MermaidPropertiesWidget.cpp
    ${SRC_DIR}/MermaidPropertiesWidget.h
    ${SRC_DIR}/PdfToImageNode.cpp
    ${SRC_DIR}/PdfToImageNode.h
    ${SRC_DIR}/PdfToImagePropertiesWidget.cpp
    ${SRC_DIR}/PdfToImagePropertiesWidget.h
    ${SRC_DIR}/HumanInputNode.cpp
    ${SRC_DIR}/HumanInputNode.h
    ${SRC_DIR}/HumanInputPropertiesWidget.cpp
    ${SRC_DIR}/HumanInputPropertiesWidget.h
    ${SRC_DIR}/ProcessConnector.cpp
    ${SRC_DIR}/ProcessConnector.h
    ${SRC_DIR}/ProcessConnectorPropertiesWidget.cpp
    ${SRC_DIR}/ProcessConnectorPropertiesWidget.h
    ${SRC_DIR}/PythonScriptConnectorPropertiesWidget.cpp
    ${SRC_DIR}/PythonScriptConnectorPropertiesWidget.h
    ${SRC_DIR}/DatabaseConnector.cpp
    ${SRC_DIR}/DatabaseConnector.h
    ${SRC_DIR}/DatabaseConnectorPropertiesWidget.cpp
    ${SRC_DIR}/DatabaseConnectorPropertiesWidget.h
    ${SRC_DIR}/QuickJSRuntime.cpp
    ${SRC_DIR}/QuickJSRuntime.h
    ${SRC_DIR}/ScriptDatabaseBridge.cpp
    ${SRC_DIR}/ScriptDatabaseBridge.h
    ${SRC_DIR}/RagIndexerNode.cpp
    ${SRC_DIR}/RagIndexerNode.h
    ${SRC_DIR}/RagIndexerPropertiesWidget.cpp
    ${SRC_DIR}/RagIndexerPropertiesWidget.h
    ${SRC_DIR}/ConditionalRouterNode.cpp
    ${SRC_DIR}/ConditionalRouterNode.h
    ${SRC_DIR}/ConditionalRouterPropertiesWidget.cpp
    ${SRC_DIR}/ConditionalRouterPropertiesWidget.h
    ${SRC_DIR}/LoopNode.cpp
    ${SRC_DIR}/LoopNode.h
    ${SRC_DIR}/LoopPropertiesWidget.cpp
    ${SRC_DIR}/LoopPropertiesWidget.h
    ${SRC_DIR}/LoopUntilNode.cpp
    ${SRC_DIR}/LoopUntilNode.h
    ${SRC_DIR}/LoopUntilPropertiesWidget.cpp
    ${SRC_DIR}/LoopUntilPropertiesWidget.h
    ${SRC_DIR}/UniversalScriptConnector.cpp
    ${SRC_DIR}/UniversalScriptConnector.h
    ${SRC_DIR}/ScriptPropertiesWidget.cpp
    ${SRC_DIR}/ScriptPropertiesWidget.h
    ${SRC_DIR}/NodeGraphModel.cpp
    ${SRC_DIR}/NodeGraphModel.h
    ${SRC_DIR}/ToolNodeDelegate.cpp
    ${SRC_DIR}/ToolNodeDelegate.h
    ${SRC_DIR}/NodeInfoWidget.cpp
    ${SRC_DIR}/NodeInfoWidget.h
    ${SRC_DIR}/ExecutionEngine.cpp
    ${SRC_DIR}/ExecutionEngine.h
    ${SRC_DIR}/MermaidRenderService.cpp
    ${SRC_DIR}/MermaidRenderService.h
    ${SRC_DIR}/CredentialsDialog.cpp
    ${SRC_DIR}/CredentialsDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${qtnodes_SOURCE_DIR}/resources/resources.qrc
    $<$<BOOL:${WIN32}>:${WIN32_RESOURCE_FILE}>
)


# Link libraries
# Add public headers directory to include path
target_include_directories(CognitivePipelines PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SRC_DIR}
    ${SRC_DIR}/backends
    ${SRC_DIR}/core
    ${SRC_DIR}/chunkingstrategies
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(CognitivePipelines PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Pdf
    Qt6::DBus
    QtNodes::QtNodes
    Boost::boost
    cpr::cpr
    quickjs
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

# --- Testing setup (optional, on by default) ---
option(ENABLE_TESTING "Build unit/integration tests" ON)

if(ENABLE_TESTING)
    message(STATUS "ENABLE_TESTING=ON: building test suite")
    include(CTest)
    enable_testing()

    # Unit Tests (GTest)
    add_executable(unit_tests
            # Core headers
            ${INCLUDE_DIR}/IToolConnector.h
            ${INCLUDE_DIR}/CommonDataTypes.h
            ${INCLUDE_DIR}/string_utils.h
            # Test sources
            tests/test_app_init.cpp
            src/Logger.cpp
            src/Logger.h
            tests/test_main.cpp
            tests/test_nodes.cpp
            tests/test_text_output_fanout.cpp
            tests/test_execution_engine.cpp
            tests/test_text_output_save.cpp
            tests/test_invalid_model_pipeline.cpp
            tests/test_process_connector.cpp
            tests/test_universal_llm.cpp
            tests/test_logging.cpp
            tests/test_image_node.cpp
            tests/test_mermaid_node.cpp
            tests/test_mermaid_repro.cpp
            tests/test_pdf_node.cpp
            tests/test_rag_foundation.cpp
            tests/test_document_loader.cpp
            tests/test_text_chunker.cpp
            tests/test_chunker_viz.cpp
            tests/test_rag_indexer.cpp
            tests/test_rag.cpp
            tests/test_rag_integration.cpp
            tests/test_sql_edge_cases.cpp
            tests/test_ziggy_repro.cpp
            tests/test_conditional_router.cpp
            tests/test_router_execution.cpp
            tests/test_loop_node.cpp
            tests/test_loop_integration.cpp
            tests/test_execution_control.cpp
            tests/test_loop_until.cpp
            tests/test_model_caps.cpp
            tests/test_universal_caps.cpp
            tests/test_dynamic_discovery.cpp
            tests/test_model_selection.cpp
            tests/test_backend_discovery.cpp
            tests/test_string_utils.cpp
            tests/test_anthropic_foundation.cpp
            tests/test_anthropic_chat.cpp
            tests/test_execution_host.cpp
            tests/test_quickjs_backend.cpp
            tests/ScriptDatabaseBridgeTest.cpp
            tests/ScriptNodeIntegrationTest.cpp
            # Sources required by the unit tests
            ${SRC_DIR}/ExecutionScriptHost.cpp
            ${SRC_DIR}/ExecutionScriptHost.h
            ${SRC_DIR}/logging_categories.cpp
            ${SRC_DIR}/ExecutionEngine.cpp
            ${SRC_DIR}/ExecutionEngine.h
            ${SRC_DIR}/NodeGraphModel.cpp
            ${SRC_DIR}/NodeGraphModel.h
            ${SRC_DIR}/ToolNodeDelegate.cpp
            ${SRC_DIR}/ToolNodeDelegate.h
            ${SRC_DIR}/NodeInfoWidget.cpp
            ${SRC_DIR}/NodeInfoWidget.h
            ${SRC_DIR}/PromptBuilderNode.cpp
            ${SRC_DIR}/PromptBuilderNode.h
            ${SRC_DIR}/PromptBuilderPropertiesWidget.cpp
            ${SRC_DIR}/PromptBuilderPropertiesWidget.h
            ${SRC_DIR}/TextInputNode.cpp
            ${SRC_DIR}/TextInputNode.h
            ${SRC_DIR}/TextInputPropertiesWidget.cpp
            ${SRC_DIR}/TextInputPropertiesWidget.h
            ${SRC_DIR}/TextOutputNode.cpp
            ${SRC_DIR}/TextOutputNode.h
            ${SRC_DIR}/TextOutputPropertiesWidget.cpp
            ${SRC_DIR}/TextOutputPropertiesWidget.h
            ${SRC_DIR}/ImageNode.cpp
            ${SRC_DIR}/ImageNode.h
            ${SRC_DIR}/ImagePropertiesWidget.cpp
            ${SRC_DIR}/ImagePropertiesWidget.h
            ${SRC_DIR}/ImagePopupDialog.cpp
            ${SRC_DIR}/ImagePopupDialog.h
            ${SRC_DIR}/MermaidNode.cpp
            ${SRC_DIR}/MermaidNode.h
            ${SRC_DIR}/MermaidPropertiesWidget.cpp
            ${SRC_DIR}/MermaidPropertiesWidget.h
            ${SRC_DIR}/MermaidRenderService.cpp
            ${SRC_DIR}/MermaidRenderService.h
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
            ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
            ${SRC_DIR}/PdfToImageNode.cpp
            ${SRC_DIR}/PdfToImageNode.h
            ${SRC_DIR}/PdfToImagePropertiesWidget.cpp
            ${SRC_DIR}/PdfToImagePropertiesWidget.h
            ${SRC_DIR}/HumanInputNode.cpp
            ${SRC_DIR}/HumanInputNode.h
            ${SRC_DIR}/HumanInputPropertiesWidget.cpp
            ${SRC_DIR}/HumanInputPropertiesWidget.h
            ${SRC_DIR}/mainwindow.cpp
            ${SRC_DIR}/mainwindow.h
            ${SRC_DIR}/UserInputDialog.cpp
            ${SRC_DIR}/UserInputDialog.h
            ${SRC_DIR}/about_dialog.cpp
            ${SRC_DIR}/about_dialog.h
            ${SRC_DIR}/CredentialsDialog.cpp
            ${SRC_DIR}/CredentialsDialog.h
            ${SRC_DIR}/ProcessConnector.cpp
            ${SRC_DIR}/ProcessConnector.h
            ${SRC_DIR}/ProcessConnectorPropertiesWidget.cpp
            ${SRC_DIR}/ProcessConnectorPropertiesWidget.h
            ${SRC_DIR}/PythonScriptConnector.cpp
            ${SRC_DIR}/PythonScriptConnector.h
            ${SRC_DIR}/PythonScriptConnectorPropertiesWidget.cpp
            ${SRC_DIR}/PythonScriptConnectorPropertiesWidget.h
            ${SRC_DIR}/DatabaseConnector.cpp
            ${SRC_DIR}/DatabaseConnector.h
            ${SRC_DIR}/DatabaseConnectorPropertiesWidget.cpp
            ${SRC_DIR}/DatabaseConnectorPropertiesWidget.h
            ${SRC_DIR}/QuickJSRuntime.cpp
            ${SRC_DIR}/QuickJSRuntime.h
            ${SRC_DIR}/ScriptDatabaseBridge.cpp
            ${SRC_DIR}/ScriptDatabaseBridge.h
            ${SRC_DIR}/UniversalScriptConnector.cpp
            ${SRC_DIR}/UniversalScriptConnector.h
            ${SRC_DIR}/ScriptPropertiesWidget.cpp
            ${SRC_DIR}/ScriptPropertiesWidget.h
            ${SRC_DIR}/RagIndexerNode.cpp
            ${SRC_DIR}/RagIndexerNode.h
            ${SRC_DIR}/RagIndexerPropertiesWidget.cpp
            ${SRC_DIR}/RagIndexerPropertiesWidget.h
            ${SRC_DIR}/ConditionalRouterNode.cpp
            ${SRC_DIR}/ConditionalRouterNode.h
            ${SRC_DIR}/ConditionalRouterPropertiesWidget.cpp
            ${SRC_DIR}/ConditionalRouterPropertiesWidget.h
            ${SRC_DIR}/LoopNode.cpp
            ${SRC_DIR}/LoopNode.h
            ${SRC_DIR}/LoopPropertiesWidget.cpp
            ${SRC_DIR}/LoopPropertiesWidget.h
            ${SRC_DIR}/LoopUntilNode.cpp
            ${SRC_DIR}/LoopUntilNode.h
            ${SRC_DIR}/LoopUntilPropertiesWidget.cpp
            ${SRC_DIR}/LoopUntilPropertiesWidget.h
            ${SRC_DIR}/ExecutionAwarePainters.cpp
            ${SRC_DIR}/ExecutionStateModel.cpp
            ${SRC_DIR}/UniversalLLMNode.cpp
            ${SRC_DIR}/UniversalLLMNode.h
            ${SRC_DIR}/UniversalLLMPropertiesWidget.cpp
            ${SRC_DIR}/UniversalLLMPropertiesWidget.h
            ${SRC_DIR}/ModelCaps.cpp
            ${SRC_DIR}/ModelCaps.h
            ${SRC_DIR}/ModelCapsRegistry.cpp
            ${SRC_DIR}/ModelCapsRegistry.h
            ${SRC_DIR}/ImageGenNode.cpp
            ${SRC_DIR}/ImageGenNode.h
            ${SRC_DIR}/ImageGenPropertiesWidget.cpp
            ${SRC_DIR}/ImageGenPropertiesWidget.h
            ${SRC_DIR}/core/LLMProviderRegistry.cpp
            ${SRC_DIR}/core/LLMProviderRegistry.h
            ${SRC_DIR}/core/DocumentLoader.cpp
            ${SRC_DIR}/core/DocumentLoader.h
            ${SRC_DIR}/core/TextChunker.cpp
            ${SRC_DIR}/core/TextChunker.h
            ${SRC_DIR}/core/RagUtils.cpp
            ${SRC_DIR}/core/RagUtils.h
            ${SRC_DIR}/RagQueryNode.cpp
            ${SRC_DIR}/RagQueryNode.h
            ${SRC_DIR}/RagQueryPropertiesWidget.cpp
            ${SRC_DIR}/RagQueryPropertiesWidget.h
            ${SRC_DIR}/chunkingstrategies/ChunkerStrategy.h
            ${SRC_DIR}/chunkingstrategies/MarkdownChunker.cpp
            ${SRC_DIR}/chunkingstrategies/MarkdownChunker.h
            ${SRC_DIR}/chunkingstrategies/StandardCodeChunker.cpp
            ${SRC_DIR}/chunkingstrategies/StandardCodeChunker.h
            ${SRC_DIR}/backends/ILLMBackend.h
            ${SRC_DIR}/backends/OpenAIBackend.cpp
            ${SRC_DIR}/backends/OpenAIBackend.h
            ${SRC_DIR}/backends/GoogleBackend.cpp
            ${SRC_DIR}/backends/GoogleBackend.h
            ${SRC_DIR}/backends/AnthropicBackend.cpp
            ${SRC_DIR}/backends/AnthropicBackend.h
            $<$<BOOL:${WIN32}>:${WIN32_RESOURCE_FILE}>
    )
    target_include_directories(unit_tests PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(unit_tests PRIVATE
            GTest::gtest # Use vcpkg target
            cpr::cpr
            Qt6::Core
            Qt6::Widgets
            Qt6::Test
            Qt6::Concurrent
            Qt6::Sql
            Qt6::Pdf
            Qt6::WebEngineWidgets
            Qt6::DBus
            QtNodes::QtNodes
            Boost::boost
            quickjs
    )
    # Add unit tests to CTest
    include(GoogleTest)
    # Integration tests (Qt Test based, headless) - Definition looks okay
    add_executable(integration_tests
            # Core headers
            ${INCLUDE_DIR}/IToolConnector.h
            ${INCLUDE_DIR}/CommonDataTypes.h
            # Test sources and required implementations
            tests/test_integration.cpp
            tests/test_matrix.cpp
            tests/integration/MermaidRenderTest.cpp
            ${SRC_DIR}/Logger.cpp
            ${SRC_DIR}/Logger.h
            ${SRC_DIR}/logging_categories.cpp
            ${SRC_DIR}/mainwindow.cpp
            ${SRC_DIR}/mainwindow.h
            ${SRC_DIR}/about_dialog.cpp
            ${SRC_DIR}/about_dialog.h
            ${SRC_DIR}/UserInputDialog.cpp
            ${SRC_DIR}/UserInputDialog.h
            ${SRC_DIR}/PythonScriptConnector.cpp
            ${SRC_DIR}/PythonScriptConnector.h
            ${SRC_DIR}/PythonScriptConnectorPropertiesWidget.cpp
            ${SRC_DIR}/PythonScriptConnectorPropertiesWidget.h
            ${SRC_DIR}/UniversalLLMNode.cpp
            ${SRC_DIR}/UniversalLLMNode.h
            ${SRC_DIR}/UniversalLLMPropertiesWidget.cpp
            ${SRC_DIR}/UniversalLLMPropertiesWidget.h
            ${SRC_DIR}/ModelCaps.cpp
            ${SRC_DIR}/ModelCaps.h
            ${SRC_DIR}/ModelCapsRegistry.cpp
            ${SRC_DIR}/ModelCapsRegistry.h
            ${SRC_DIR}/ImageGenNode.cpp
            ${SRC_DIR}/ImageGenNode.h
            ${SRC_DIR}/ImageGenPropertiesWidget.cpp
            ${SRC_DIR}/ImageGenPropertiesWidget.h
            ${SRC_DIR}/core/LLMProviderRegistry.cpp
            ${SRC_DIR}/core/LLMProviderRegistry.h
            ${SRC_DIR}/backends/ILLMBackend.h
            ${SRC_DIR}/backends/OpenAIBackend.cpp
            ${SRC_DIR}/backends/OpenAIBackend.h
            ${SRC_DIR}/backends/GoogleBackend.cpp
            ${SRC_DIR}/backends/GoogleBackend.h
            ${SRC_DIR}/backends/AnthropicBackend.cpp
            ${SRC_DIR}/backends/AnthropicBackend.h
            ${SRC_DIR}/ExecutionScriptHost.cpp
            ${SRC_DIR}/ExecutionScriptHost.h
            ${SRC_DIR}/core/DocumentLoader.cpp
            ${SRC_DIR}/core/DocumentLoader.h
            ${SRC_DIR}/core/TextChunker.cpp
            ${SRC_DIR}/core/TextChunker.h
            ${SRC_DIR}/core/RagUtils.cpp
            ${SRC_DIR}/core/RagUtils.h
            ${SRC_DIR}/chunkingstrategies/ChunkerStrategy.h
            ${SRC_DIR}/chunkingstrategies/MarkdownChunker.cpp
            ${SRC_DIR}/chunkingstrategies/MarkdownChunker.h
            ${SRC_DIR}/chunkingstrategies/StandardCodeChunker.cpp
            ${SRC_DIR}/chunkingstrategies/StandardCodeChunker.h
            ${SRC_DIR}/PromptBuilderNode.cpp
            ${SRC_DIR}/PromptBuilderNode.h
            ${SRC_DIR}/PromptBuilderPropertiesWidget.cpp
            ${SRC_DIR}/PromptBuilderPropertiesWidget.h
            ${SRC_DIR}/TextInputNode.cpp
            ${SRC_DIR}/TextInputNode.h
            ${SRC_DIR}/TextInputPropertiesWidget.cpp
            ${SRC_DIR}/TextInputPropertiesWidget.h
            ${SRC_DIR}/TextOutputNode.cpp
            ${SRC_DIR}/TextOutputNode.h
            ${SRC_DIR}/TextOutputPropertiesWidget.cpp
            ${SRC_DIR}/TextOutputPropertiesWidget.h
            ${SRC_DIR}/ImageNode.cpp
            ${SRC_DIR}/ImageNode.h
            ${SRC_DIR}/ImagePropertiesWidget.cpp
            ${SRC_DIR}/ImagePropertiesWidget.h
            ${SRC_DIR}/ImagePopupDialog.cpp
            ${SRC_DIR}/ImagePopupDialog.h
            ${SRC_DIR}/MermaidNode.cpp
            ${SRC_DIR}/MermaidNode.h
            ${SRC_DIR}/MermaidPropertiesWidget.cpp
            ${SRC_DIR}/MermaidPropertiesWidget.h
            ${SRC_DIR}/MermaidRenderService.cpp
            ${SRC_DIR}/MermaidRenderService.h
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
            ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
            ${SRC_DIR}/PdfToImageNode.cpp
            ${SRC_DIR}/PdfToImageNode.h
            ${SRC_DIR}/PdfToImagePropertiesWidget.cpp
            ${SRC_DIR}/PdfToImagePropertiesWidget.h
            ${SRC_DIR}/HumanInputNode.cpp
            ${SRC_DIR}/HumanInputNode.h
            ${SRC_DIR}/HumanInputPropertiesWidget.cpp
            ${SRC_DIR}/HumanInputPropertiesWidget.h
            ${SRC_DIR}/QuickJSRuntime.cpp
            ${SRC_DIR}/QuickJSRuntime.h
            ${SRC_DIR}/ScriptDatabaseBridge.cpp
            ${SRC_DIR}/ScriptDatabaseBridge.h
            ${SRC_DIR}/UniversalScriptConnector.cpp
            ${SRC_DIR}/UniversalScriptConnector.h
            ${SRC_DIR}/ScriptPropertiesWidget.cpp
            ${SRC_DIR}/ScriptPropertiesWidget.h
            ${SRC_DIR}/NodeGraphModel.cpp
            ${SRC_DIR}/NodeGraphModel.h
            ${SRC_DIR}/ToolNodeDelegate.cpp
            ${SRC_DIR}/ToolNodeDelegate.h
            ${SRC_DIR}/NodeInfoWidget.cpp
            ${SRC_DIR}/NodeInfoWidget.h
            ${SRC_DIR}/ExecutionEngine.cpp
            ${SRC_DIR}/ExecutionEngine.h
            ${SRC_DIR}/CredentialsDialog.cpp
            ${SRC_DIR}/CredentialsDialog.h
            ${SRC_DIR}/ProcessConnector.cpp
            ${SRC_DIR}/ProcessConnector.h
            ${SRC_DIR}/ProcessConnectorPropertiesWidget.cpp
            ${SRC_DIR}/ProcessConnectorPropertiesWidget.h
            ${SRC_DIR}/DatabaseConnector.cpp
            ${SRC_DIR}/DatabaseConnector.h
            ${SRC_DIR}/DatabaseConnectorPropertiesWidget.cpp
            ${SRC_DIR}/DatabaseConnectorPropertiesWidget.h
            ${SRC_DIR}/RagIndexerNode.cpp
            ${SRC_DIR}/RagIndexerNode.h
            ${SRC_DIR}/RagIndexerPropertiesWidget.cpp
            ${SRC_DIR}/RagIndexerPropertiesWidget.h
            ${SRC_DIR}/RagQueryNode.cpp
            ${SRC_DIR}/RagQueryNode.h
            ${SRC_DIR}/RagQueryPropertiesWidget.cpp
            ${SRC_DIR}/RagQueryPropertiesWidget.h
            ${SRC_DIR}/ConditionalRouterNode.cpp
            ${SRC_DIR}/ConditionalRouterNode.h
            ${SRC_DIR}/ConditionalRouterPropertiesWidget.cpp
            ${SRC_DIR}/ConditionalRouterPropertiesWidget.h
            ${SRC_DIR}/LoopNode.cpp
            ${SRC_DIR}/LoopNode.h
            ${SRC_DIR}/LoopPropertiesWidget.cpp
            ${SRC_DIR}/LoopPropertiesWidget.h
            ${SRC_DIR}/LoopUntilNode.cpp
            ${SRC_DIR}/LoopUntilNode.h
            ${SRC_DIR}/LoopUntilPropertiesWidget.cpp
            ${SRC_DIR}/LoopUntilPropertiesWidget.h
            ${SRC_DIR}/ExecutionAwarePainters.cpp
            ${SRC_DIR}/ExecutionStateModel.cpp
            $<$<BOOL:${WIN32}>:${WIN32_RESOURCE_FILE}>
    )

    target_include_directories(integration_tests PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(integration_tests PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::Concurrent
            Qt6::Sql
            Qt6::Pdf
            Qt6::WebEngineWidgets
            Qt6::DBus
            QtNodes::QtNodes
            Boost::boost
            cpr::cpr
            quickjs
    )

    # Force integration_tests to be a CONSOLE app on Windows
    # This enables stdout/stderr logging in CI
    if(WIN32)
        set_target_properties(integration_tests PROPERTIES
                WIN32_EXECUTABLE OFF
        )
    endif()

    # Add integration tests to CTest (can have the same name as executable)
    add_test(NAME integration_tests COMMAND integration_tests) # Explicitly add to CTest

endif()

# macOS: convert executables to .app bundles, apply icon and entitlements
if(APPLE)
    if(TARGET CognitivePipelines)
        # Ensure the icon is explicitly packaged and Info.plist is under our control
        target_sources(CognitivePipelines PRIVATE ${MACOSX_ICON_FILE})
        set_source_files_properties(${MACOSX_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(CognitivePipelines PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_ICON_FILE ${MACOSX_ICON_NAME}
            MACOSX_BUNDLE_INFO_PLIST ${MACOSX_INFO_PLIST_TEMPLATE}
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${MACOSX_ENTITLEMENTS_FILE}
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_SIGN_IDENTITY}"
            MACOSX_BUNDLE_GUI_IDENTIFIER com.cognitivepipelines.app
            MACOSX_BUNDLE_BUNDLE_NAME CognitivePipelines
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        )
    endif()
    if(TARGET unit_tests)
        target_sources(unit_tests PRIVATE ${MACOSX_ICON_FILE})
        set_source_files_properties(${MACOSX_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(unit_tests PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_ICON_FILE ${MACOSX_ICON_NAME}
            MACOSX_BUNDLE_INFO_PLIST ${MACOSX_INFO_PLIST_TEMPLATE}
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${MACOSX_ENTITLEMENTS_FILE}
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_SIGN_IDENTITY}"
            MACOSX_BUNDLE_GUI_IDENTIFIER com.cognitivepipelines.unit-tests
            MACOSX_BUNDLE_BUNDLE_NAME unit_tests
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        )
    endif()
    if(TARGET integration_tests)
        target_sources(integration_tests PRIVATE ${MACOSX_ICON_FILE})
        set_source_files_properties(${MACOSX_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(integration_tests PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_ICON_FILE ${MACOSX_ICON_NAME}
            MACOSX_BUNDLE_INFO_PLIST ${MACOSX_INFO_PLIST_TEMPLATE}
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${MACOSX_ENTITLEMENTS_FILE}
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_SIGN_IDENTITY}"
            MACOSX_BUNDLE_GUI_IDENTIFIER com.cognitivepipelines.integration-tests
            MACOSX_BUNDLE_BUNDLE_NAME integration_tests
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        )
    endif()

    # Ensure test bundles can locate Homebrew/VCPKG cpr at runtime without copying
    if(TARGET unit_tests)
        set_target_properties(unit_tests PROPERTIES
            BUILD_RPATH "$<TARGET_FILE_DIR:cpr::cpr>"
            INSTALL_RPATH "$<TARGET_FILE_DIR:cpr::cpr>"
        )
    endif()
    if(TARGET integration_tests)
        set_target_properties(integration_tests PROPERTIES
            BUILD_RPATH "$<TARGET_FILE_DIR:cpr::cpr>"
            INSTALL_RPATH "$<TARGET_FILE_DIR:cpr::cpr>"
        )
    endif()

    # For non-Xcode generators, explicitly codesign bundles with entitlements so
    # macOS App Sandbox permissions (file dialogs, networking) are effective.
    if(NOT XCODE)
        find_program(CODESIGN_EXECUTABLE codesign)
        if(CODESIGN_EXECUTABLE)
            set(IS_DEBUG_BUILD FALSE)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(IS_DEBUG_BUILD TRUE)
            endif()

            if(IS_DEBUG_BUILD)
                get_target_property(_qt_qmake_location Qt6::qmake IMPORTED_LOCATION)
                if(NOT QT_INSTALL_LIBEXECS AND _qt_qmake_location)
                    execute_process(
                        COMMAND "${_qt_qmake_location}" -query QT_INSTALL_LIBEXECS
                        OUTPUT_VARIABLE QT_INSTALL_LIBEXECS
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
                endif()

                if(NOT QT_INSTALL_LIBEXECS)
                    find_program(QMAKE_EXECUTABLE NAMES qmake6 qmake-qt6 qmake
                        HINTS
                            $ENV{HOMEBREW_PREFIX}/opt/qt/bin
                            /usr/local/opt/qt/bin
                            /opt/homebrew/opt/qt/bin
                            /usr/local/opt/qt6/bin
                            /opt/homebrew/opt/qt6/bin
                            /usr/local/opt/qt@6/bin
                            /opt/homebrew/opt/qt@6/bin)
                    if(QMAKE_EXECUTABLE)
                        execute_process(
                            COMMAND "${QMAKE_EXECUTABLE}" -query QT_INSTALL_LIBEXECS
                            OUTPUT_VARIABLE QT_INSTALL_LIBEXECS
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                    endif()
                endif()

                if(NOT QT_PLUGINS_DIR AND _qt_qmake_location)
                    execute_process(
                        COMMAND "${_qt_qmake_location}" -query QT_INSTALL_PLUGINS
                        OUTPUT_VARIABLE QT_PLUGINS_DIR
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
                endif()

                if(NOT QT_PLUGINS_DIR AND QMAKE_EXECUTABLE)
                    execute_process(
                        COMMAND "${QMAKE_EXECUTABLE}" -query QT_INSTALL_PLUGINS
                        OUTPUT_VARIABLE QT_PLUGINS_DIR
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
                endif()

                if(NOT QT_PLUGINS_DIR)
                    find_program(QTPATHS_EXECUTABLE NAMES qtpaths6 qtpaths-qt6 qtpaths)
                    if(QTPATHS_EXECUTABLE)
                        execute_process(
                            COMMAND "${QTPATHS_EXECUTABLE}" --plugin-dir
                            OUTPUT_VARIABLE QT_PLUGINS_DIR
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                    endif()
                endif()

                set(_qt_libexec_candidates "")
                if(QT_INSTALL_LIBEXECS)
                    list(APPEND _qt_libexec_candidates "${QT_INSTALL_LIBEXECS}")
                endif()
                if(DEFINED Qt6_DIR)
                    get_filename_component(_qt_cmake_dir "${Qt6_DIR}" DIRECTORY)
                    get_filename_component(_qt_prefix_lib "${_qt_cmake_dir}" DIRECTORY)
                    get_filename_component(_qt_prefix "${_qt_prefix_lib}" DIRECTORY)
                    list(APPEND _qt_libexec_candidates "${_qt_prefix}/libexec")
                endif()
                get_target_property(_qt_webenginecore_location Qt6::WebEngineCore LOCATION)
                get_target_property(_qt_webenginewidgets_location Qt6::WebEngineWidgets LOCATION)
                if(_qt_webenginecore_location)
                    get_filename_component(_qt_webenginecore_framework_dir "${_qt_webenginecore_location}" DIRECTORY)
                    list(APPEND _qt_libexec_candidates "${_qt_webenginecore_framework_dir}/Helpers")
                endif()
                list(APPEND _qt_libexec_candidates
                    "$ENV{HOMEBREW_PREFIX}/opt/qt/libexec"
                    "/usr/local/opt/qt/libexec"
                    "/opt/homebrew/opt/qt/libexec"
                    "/usr/local/libexec"
                    "/opt/homebrew/libexec"
                    "/usr/local/share/qt/libexec")

                foreach(_libexec_path IN LISTS _qt_libexec_candidates)
                    if(_libexec_path AND EXISTS "${_libexec_path}/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess")
                        set(QT_INSTALL_LIBEXECS "${_libexec_path}")
                        break()
                    endif()
                endforeach()
            endif()

            foreach(tgt IN ITEMS CognitivePipelines unit_tests integration_tests)
                if(TARGET ${tgt})
                    # Deploy Qt frameworks into the app bundle so it runs outside the build machine
                    # Only run macdeployqt for Release/RelWithDebInfo builds to avoid duplicate framework warnings
                    # in Debug builds when using Homebrew Qt (system frameworks are used via rpath)
                    if(IS_DEBUG_BUILD)
                        message(STATUS "Fast Debug: skipping macdeployqt for ${tgt}; using system Qt frameworks")
                    elseif(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
                        find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt
                                        HINTS
                                            $ENV{HOMEBREW_PREFIX}/opt/qt/bin
                                            /usr/local/opt/qt/bin
                                            /opt/homebrew/opt/qt/bin
                                            /usr/local/opt/qt6/bin
                                            /opt/homebrew/opt/qt6/bin
                                            /usr/local/opt/qt@6/bin
                                            /opt/homebrew/opt/qt@6/bin)
                        if(MACDEPLOYQT_EXECUTABLE)
                            add_custom_command(TARGET ${tgt} POST_BUILD
                                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:${tgt}>" "-executable=$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess"
                                    COMMENT "Running macdeployqt on $<TARGET_BUNDLE_DIR:${tgt}> (including helper process)"
                                    VERBATIM)

                            # Bundling and patching frameworks for QtWebEngineProcess helper
                            add_custom_command(TARGET ${tgt} POST_BUILD
                                # Create the Frameworks directory in the Helper app
                                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks"
                                # Copy all frameworks and dylibs from Main App bundle to Helper App bundle (use rsync to preserve symlinks)
                                # We exclude QtWebEngineCore.framework to avoid recursion.
                                COMMAND /usr/bin/rsync -a --delete --exclude "QtWebEngineCore.framework" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks/"
                                COMMENT "Bundling all frameworks for QtWebEngineProcess in ${tgt}.app"
                                VERBATIM)
                        else()
                            message(WARNING "macdeployqt not found; Qt frameworks may be missing from ${tgt}.app")
                        endif()
                    else()
                        message(STATUS "Skipping macdeployqt for ${tgt} in ${CMAKE_BUILD_TYPE} build (using system Qt frameworks)")
                    endif()

                    if(IS_DEBUG_BUILD)
                        # Ensure stale bundled Qt frameworks/plugins from prior deploys are removed to avoid
                        # double-loading system Qt alongside bundled copies.
                        add_custom_command(TARGET ${tgt} POST_BUILD
                            COMMAND /bin/sh -c "bundle=\"$1\"; for fw in \"$bundle\"/Contents/Frameworks/Qt*.framework; do case \"$fw\" in *QtWebEngineCore.framework|*QtWebEngineWidgets.framework) :;; *) rm -rf \"$fw\";; esac; done; rm -rf \"$bundle\"/Contents/PlugIns" _ "$<TARGET_BUNDLE_DIR:${tgt}>"
                            COMMENT "Fast Debug: removing bundled Qt frameworks/plugins (keeping QtWebEngineCore/QtWebEngineWidgets) to prefer system Qt"
                            VERBATIM)

                        if(_qt_webenginecore_location)
                            get_filename_component(_qt_webenginecore_framework_dir "${_qt_webenginecore_location}" DIRECTORY)
                            get_filename_component(_qt_webenginecore_framework_versions "${_qt_webenginecore_framework_dir}" DIRECTORY)
                            get_filename_component(_qt_webenginecore_framework_root "${_qt_webenginecore_framework_versions}" DIRECTORY)

                            add_custom_command(TARGET ${tgt} POST_BUILD
                                COMMAND /usr/bin/rsync -a --delete "${_qt_webenginecore_framework_root}/" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework/"
                                COMMENT "Fast Debug: ensuring QtWebEngineCore.framework is present in ${tgt}.app"
                                VERBATIM)
                        endif()

                        if(_qt_webenginewidgets_location)
                            get_filename_component(_qt_webenginewidgets_framework_dir "${_qt_webenginewidgets_location}" DIRECTORY)
                            get_filename_component(_qt_webenginewidgets_framework_versions "${_qt_webenginewidgets_framework_dir}" DIRECTORY)
                            get_filename_component(_qt_webenginewidgets_framework_root "${_qt_webenginewidgets_framework_versions}" DIRECTORY)

                            add_custom_command(TARGET ${tgt} POST_BUILD
                                COMMAND /usr/bin/rsync -a --delete "${_qt_webenginewidgets_framework_root}/" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineWidgets.framework/"
                                COMMENT "Fast Debug: ensuring QtWebEngineWidgets.framework is present in ${tgt}.app"
                                VERBATIM)
                        endif()

                        if(QT_INSTALL_LIBEXECS AND EXISTS "${QT_INSTALL_LIBEXECS}/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess")
                            set(_qtwebengine_helper_src_app "${QT_INSTALL_LIBEXECS}/QtWebEngineProcess.app")
                            set(_qtwebengine_helper_dst_app "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework/Helpers/QtWebEngineProcess.app")
                            set(_qtwebengine_helper_dst_bin "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess")

                            add_custom_command(TARGET ${tgt} POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E make_directory "${_qtwebengine_helper_dst_app}"
                                COMMAND ${CMAKE_COMMAND} -E copy_directory "${_qtwebengine_helper_src_app}" "${_qtwebengine_helper_dst_app}"
                                COMMAND ${CODESIGN_EXECUTABLE} --force --options runtime
                                        --entitlements "${MACOSX_QTWEBENGINE_PROCESS_ENTITLEMENTS}"
                                        --sign "-" "${_qtwebengine_helper_dst_bin}"
                                COMMAND ${CODESIGN_EXECUTABLE} --force --options runtime --sign "-" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineCore.framework"
                                COMMAND ${CODESIGN_EXECUTABLE} --force --options runtime --sign "-" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Frameworks/QtWebEngineWidgets.framework"
                                COMMENT "Fast Debug: copying and signing QtWebEngineProcess for ${tgt}.app"
                                VERBATIM)
                        else()
                            message(WARNING "Fast Debug: QtWebEngineProcess not found at ${QT_INSTALL_LIBEXECS}/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess; skipping copy for ${tgt}")
                        endif()

                        set(_qt_platform_plugin_candidates "")
                        if(QT_PLUGINS_DIR)
                            list(APPEND _qt_platform_plugin_candidates "${QT_PLUGINS_DIR}/platforms/libqcocoa.dylib")
                        endif()
                        list(APPEND _qt_platform_plugin_candidates
                            "$ENV{HOMEBREW_PREFIX}/share/qt/plugins/platforms/libqcocoa.dylib"
                            "/usr/local/share/qt/plugins/platforms/libqcocoa.dylib"
                            "/opt/homebrew/share/qt/plugins/platforms/libqcocoa.dylib")

                        set(_qt_platform_plugin_src "")
                        foreach(_candidate IN LISTS _qt_platform_plugin_candidates)
                            if(_candidate AND EXISTS "${_candidate}")
                                set(_qt_platform_plugin_src "${_candidate}")
                                break()
                            endif()
                        endforeach()

                        if(_qt_platform_plugin_src)
                            set(_qt_platform_plugin_dst_dir "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/PlugIns/platforms")
                            set(_qt_platform_plugin_dst "${_qt_platform_plugin_dst_dir}/libqcocoa.dylib")

                            add_custom_command(TARGET ${tgt} POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E make_directory "${_qt_platform_plugin_dst_dir}"
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qt_platform_plugin_src}" "${_qt_platform_plugin_dst}"
                                COMMAND ${CODESIGN_EXECUTABLE} --force --sign - "${_qt_platform_plugin_dst}"
                                COMMENT "Fast Debug: copying cocoa platform plugin for ${tgt}.app"
                                VERBATIM)
                        else()
                            message(WARNING "Fast Debug: Qt cocoa platform plugin not found; cocoa plugin will rely on system lookup")
                        endif()

                        set(_qt_sqlite_plugin_candidates "")
                        if(QT_PLUGINS_DIR)
                            list(APPEND _qt_sqlite_plugin_candidates "${QT_PLUGINS_DIR}/sqldrivers/libqsqlite.dylib")
                        endif()
                        list(APPEND _qt_sqlite_plugin_candidates
                            "$ENV{HOMEBREW_PREFIX}/share/qt/plugins/sqldrivers/libqsqlite.dylib"
                            "/usr/local/share/qt/plugins/sqldrivers/libqsqlite.dylib"
                            "/opt/homebrew/share/qt/plugins/sqldrivers/libqsqlite.dylib")

                        set(_qt_sqlite_plugin_src "")
                        foreach(_candidate IN LISTS _qt_sqlite_plugin_candidates)
                            if(_candidate AND EXISTS "${_candidate}")
                                set(_qt_sqlite_plugin_src "${_candidate}")
                                break()
                            endif()
                        endforeach()

                        if(_qt_sqlite_plugin_src)
                            set(_qt_sqlite_plugin_dst_dir "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/PlugIns/sqldrivers")
                            set(_qt_sqlite_plugin_dst "${_qt_sqlite_plugin_dst_dir}/libqsqlite.dylib")

                            add_custom_command(TARGET ${tgt} POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E make_directory "${_qt_sqlite_plugin_dst_dir}"
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qt_sqlite_plugin_src}" "${_qt_sqlite_plugin_dst}"
                                COMMAND ${CODESIGN_EXECUTABLE} --force --sign - "${_qt_sqlite_plugin_dst}"
                                COMMENT "Fast Debug: copying sqlite driver plugin for ${tgt}.app"
                                VERBATIM)
                        else()
                            message(WARNING "Fast Debug: Qt sqlite driver plugin not found; database features may be unavailable in ${tgt}.app")
                        endif()
                    else()
                        add_custom_command(TARGET ${tgt} POST_BUILD
                            COMMAND ${CMAKE_COMMAND}
                                -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:${tgt}>
                                -DCODESIGN_EXECUTABLE=${CODESIGN_EXECUTABLE}
                                -DENTITLEMENTS=${MACOSX_QTWEBENGINE_PROCESS_ENTITLEMENTS}
                                -DMAIN_ENTITLEMENTS=${MACOSX_ENTITLEMENTS_FILE}
                                -DIS_DEBUG_BUILD=${IS_DEBUG_BUILD}
                                -DSIGN_IDENTITY=${APPLE_SIGN_IDENTITY}
                                -P ${MACOSX_QTWEBENGINE_CODESIGNER}
                            COMMENT "Codesigning and patching ${tgt}.app"
                            VERBATIM)
                    endif()
                endif()
            endforeach()
        else()
            message(WARNING "codesign tool not found; macOS entitlements will not be applied to app bundles.")
        endif()
    endif()
endif()


# Convenience target to (re)deploy and codesign app bundles even when up-to-date
# Useful when dyld errors persist due to stale frameworks/signatures.
if(APPLE AND NOT XCODE)
    find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt
        HINTS
            $ENV{HOMEBREW_PREFIX}/opt/qt/bin
            /usr/local/opt/qt/bin
            /opt/homebrew/opt/qt/bin
            /usr/local/opt/qt6/bin
            /opt/homebrew/opt/qt6/bin
            /usr/local/opt/qt@6/bin
            /opt/homebrew/opt/qt@6/bin)
    find_program(CODESIGN_EXECUTABLE codesign)
    if(CODESIGN_EXECUTABLE)
        if(NOT DEFINED IS_DEBUG_BUILD)
            set(IS_DEBUG_BUILD FALSE)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(IS_DEBUG_BUILD TRUE)
            endif()
        endif()
        add_custom_target(macos_deploy_all)
        if(TARGET CognitivePipelines)
            add_dependencies(macos_deploy_all CognitivePipelines)

            if(MACDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:CognitivePipelines>" "-executable=$<TARGET_BUNDLE_DIR:CognitivePipelines>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess"
                    COMMENT "macdeployqt CognitivePipelines.app (Targeted via macos_deploy_all, including helper process)"
                    VERBATIM)

                # Bundling and patching frameworks for QtWebEngineProcess helper
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    # Create the Frameworks directory in the Helper app
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:CognitivePipelines>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks"
                    # Copy all frameworks and dylibs from Main App bundle to Helper App bundle (use rsync to preserve symlinks)
                    # We exclude QtWebEngineCore.framework to avoid recursion.
                    COMMAND /usr/bin/rsync -a --delete --exclude "QtWebEngineCore.framework" "$<TARGET_BUNDLE_DIR:CognitivePipelines>/Contents/Frameworks/" "$<TARGET_BUNDLE_DIR:CognitivePipelines>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks/"
                    COMMENT "Bundling all frameworks for QtWebEngineProcess in CognitivePipelines.app"
                    VERBATIM)
            endif()
            add_custom_command(TARGET macos_deploy_all POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:CognitivePipelines>
                    -DCODESIGN_EXECUTABLE=${CODESIGN_EXECUTABLE}
                    -DENTITLEMENTS=${MACOSX_QTWEBENGINE_PROCESS_ENTITLEMENTS}
                    -DMAIN_ENTITLEMENTS=${MACOSX_ENTITLEMENTS_FILE}
                    -DIS_DEBUG_BUILD=${IS_DEBUG_BUILD}
                    -DSIGN_IDENTITY=${APPLE_SIGN_IDENTITY}
                    -P ${MACOSX_QTWEBENGINE_CODESIGNER}
                COMMENT "Codesigning and patching CognitivePipelines.app"
                VERBATIM)
        endif()
        if(TARGET unit_tests)
            add_dependencies(macos_deploy_all unit_tests)
            if(MACDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:unit_tests>" "-executable=$<TARGET_BUNDLE_DIR:unit_tests>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess"
                    COMMENT "macdeployqt unit_tests.app (including helper process)"
                    VERBATIM)

                # Bundling and patching frameworks for QtWebEngineProcess helper
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    # Create the Frameworks directory in the Helper app
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:unit_tests>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks"
                    # Copy all frameworks and dylibs from Main App bundle to Helper App bundle (use rsync to preserve symlinks)
                    # We exclude QtWebEngineCore.framework to avoid recursion.
                    COMMAND /usr/bin/rsync -a --delete --exclude "QtWebEngineCore.framework" "$<TARGET_BUNDLE_DIR:unit_tests>/Contents/Frameworks/" "$<TARGET_BUNDLE_DIR:unit_tests>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks/"
                    COMMENT "Bundling all frameworks for QtWebEngineProcess in unit_tests.app"
                    VERBATIM)
            endif()
            add_custom_command(TARGET macos_deploy_all POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:unit_tests>
                    -DCODESIGN_EXECUTABLE=${CODESIGN_EXECUTABLE}
                    -DENTITLEMENTS=${MACOSX_QTWEBENGINE_PROCESS_ENTITLEMENTS}
                    -DMAIN_ENTITLEMENTS=${MACOSX_ENTITLEMENTS_FILE}
                    -DIS_DEBUG_BUILD=${IS_DEBUG_BUILD}
                    -DSIGN_IDENTITY=${APPLE_SIGN_IDENTITY}
                    -P ${MACOSX_QTWEBENGINE_CODESIGNER}
                COMMENT "Codesigning and patching unit_tests.app"
                VERBATIM)
        endif()
        if(TARGET integration_tests)
            add_dependencies(macos_deploy_all integration_tests)
            if(MACDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:integration_tests>" "-executable=$<TARGET_BUNDLE_DIR:integration_tests>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess"
                    COMMENT "macdeployqt integration_tests.app (including helper process)"
                    VERBATIM)

                # Bundling and patching frameworks for QtWebEngineProcess helper
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    # Create the Frameworks directory in the Helper app
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:integration_tests>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks"
                    # Copy all frameworks and dylibs from Main App bundle to Helper App bundle (use rsync to preserve symlinks)
                    # We exclude QtWebEngineCore.framework to avoid recursion.
                    COMMAND /usr/bin/rsync -a --delete --exclude "QtWebEngineCore.framework" "$<TARGET_BUNDLE_DIR:integration_tests>/Contents/Frameworks/" "$<TARGET_BUNDLE_DIR:integration_tests>/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks/"
                    COMMENT "Bundling all frameworks for QtWebEngineProcess in integration_tests.app"
                    VERBATIM)
            endif()
            add_custom_command(TARGET macos_deploy_all POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:integration_tests>
                    -DCODESIGN_EXECUTABLE=${CODESIGN_EXECUTABLE}
                    -DENTITLEMENTS=${MACOSX_QTWEBENGINE_PROCESS_ENTITLEMENTS}
                    -DMAIN_ENTITLEMENTS=${MACOSX_ENTITLEMENTS_FILE}
                    -DIS_DEBUG_BUILD=${IS_DEBUG_BUILD}
                    -DSIGN_IDENTITY=${APPLE_SIGN_IDENTITY}
                    -P ${MACOSX_QTWEBENGINE_CODESIGNER}
                COMMENT "Codesigning and patching integration_tests.app"
                VERBATIM)
        endif()
    else()
        message(WARNING "codesign tool not found; macOS entitlements will not be applied by macos_deploy_all.")
    endif()
endif()

# Discover tests for the unit_tests target; working directory is project root (not used for credentials anymore)
# We do this at the very end to ensure it runs AFTER all macOS POST_BUILD cleanup/codesign steps
if(TARGET unit_tests)
    gtest_discover_tests(unit_tests
        DISCOVERY_MODE POST_BUILD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DISCOVERY_TIMEOUT 300
    )
endif()

include(CPack)


# Live execution-state highlighting sources
target_sources(CognitivePipelines PRIVATE
    ${SRC_DIR}/ExecutionAwarePainters.cpp
    ${SRC_DIR}/ExecutionStateModel.cpp
)

# Windows Post-Build Signing
if(WIN32 AND WINDOWS_SIGNING_ENABLED)
    add_custom_command(TARGET CognitivePipelines POST_BUILD
        COMMAND "${SIGNTOOL_EXECUTABLE}" sign /f "${WINDOWS_PFX_FILE}" /p "${WINDOWS_PFX_PASSWORD}" 
                /tr http://timestamp.digicert.com /td sha256 /fd sha256 
                "$<TARGET_FILE:CognitivePipelines>"
        COMMENT "Signing CognitivePipelines.exe with signtool.exe"
        VERBATIM
    )
endif()
