# For local macOS development, install dependencies via Homebrew:
# brew install googletest
#
# To build and run the tests with CTest:
# cmake -DENABLE_TESTING=ON -S . -B <build_dir>
# cmake --build <build_dir> --target unit_tests
# ctest --test-dir <build_dir> -V

cmake_minimum_required(VERSION 3.21)

# Project configuration
project(CognitivePipelines VERSION 0.7.1 LANGUAGES CXX)

# Release build switch: control code signing identity for macOS
option(RELEASE_BUILD "Enable full codesigning for release" OFF)
if(RELEASE_BUILD)
    set(APPLE_SIGN_IDENTITY "Developer ID Application")
else()
    set(APPLE_SIGN_IDENTITY "-") # Use fast ad-hoc signing
endif()

# macOS packaging resources
if(APPLE)
    # Full path to the .icns and its base file name for CFBundleIconFile
    set(MACOSX_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/CognitivePipelines.icns)
    get_filename_component(MACOSX_ICON_NAME "${MACOSX_ICON_FILE}" NAME)
    set(MACOSX_ENTITLEMENTS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/CognitivePipelines.entitlements)
    set(MACOSX_INFO_PLIST_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in)
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
FetchContent_Declare(
    QtNodes
    GIT_REPOSITORY https://github.com/paceholder/nodeeditor.git
    GIT_TAG        3.0.12
)
# Explicitly build QtNodes as a shared library (required for proper resource loading)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build QtNodes as shared library" FORCE)
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
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Test Sql Pdf)
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
    # Application sources
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow.h
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
    ${SRC_DIR}/NodeGraphModel.cpp
    ${SRC_DIR}/NodeGraphModel.h
    ${SRC_DIR}/ToolNodeDelegate.cpp
    ${SRC_DIR}/ToolNodeDelegate.h
    ${SRC_DIR}/NodeInfoWidget.cpp
    ${SRC_DIR}/NodeInfoWidget.h
    ${SRC_DIR}/ExecutionEngine.cpp
    ${SRC_DIR}/ExecutionEngine.h
    ${SRC_DIR}/CredentialsEditorDialog.cpp
    ${SRC_DIR}/CredentialsEditorDialog.h
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
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Pdf
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
            # Core headers
            ${INCLUDE_DIR}/IToolConnector.h
            ${INCLUDE_DIR}/CommonDataTypes.h
            # Test sources
            tests/test_nodes.cpp
            tests/test_execution_engine.cpp
            tests/test_text_output_fanout.cpp
            tests/test_text_output_save.cpp
            tests/test_invalid_model_pipeline.cpp
            tests/test_process_connector.cpp
            tests/test_universal_llm.cpp
            tests/test_logging.cpp
            tests/test_image_node.cpp
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
            # Sources required by the unit tests
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
            ${SRC_DIR}/CredentialsEditorDialog.cpp
            ${SRC_DIR}/CredentialsEditorDialog.h
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
            $<$<BOOL:${WIN32}>:${WIN32_RESOURCE_FILE}>
    )
    target_include_directories(unit_tests PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(unit_tests PRIVATE
            GTest::gtest_main # Use vcpkg target
            cpr::cpr
            Qt6::Core
            Qt6::Widgets
            Qt6::Test
            Qt6::Concurrent
            Qt6::Sql
            Qt6::Pdf
            QtNodes::QtNodes
            Boost::boost
    )
    # Add unit tests to CTest
    include(GoogleTest)
    # Discover tests for the unit_tests target; working directory is project root (not used for credentials anymore)
    gtest_discover_tests(unit_tests 
        DISCOVERY_MODE=POST_BUILD 
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # Integration tests (Qt Test based, headless) - Definition looks okay
    add_executable(integration_tests
            # Core headers
            ${INCLUDE_DIR}/IToolConnector.h
            ${INCLUDE_DIR}/CommonDataTypes.h
            # Test sources and required implementations
            tests/test_integration.cpp
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
            ${SRC_DIR}/PdfToImageNode.cpp
            ${SRC_DIR}/PdfToImageNode.h
            ${SRC_DIR}/PdfToImagePropertiesWidget.cpp
            ${SRC_DIR}/PdfToImagePropertiesWidget.h
            ${SRC_DIR}/HumanInputNode.cpp
            ${SRC_DIR}/HumanInputNode.h
            ${SRC_DIR}/HumanInputPropertiesWidget.cpp
            ${SRC_DIR}/HumanInputPropertiesWidget.h
            ${SRC_DIR}/NodeGraphModel.cpp
            ${SRC_DIR}/NodeGraphModel.h
            ${SRC_DIR}/ToolNodeDelegate.cpp
            ${SRC_DIR}/ToolNodeDelegate.h
            ${SRC_DIR}/NodeInfoWidget.cpp
            ${SRC_DIR}/NodeInfoWidget.h
            ${SRC_DIR}/ExecutionEngine.cpp
            ${SRC_DIR}/ExecutionEngine.h
            ${SRC_DIR}/CredentialsEditorDialog.cpp
            ${SRC_DIR}/CredentialsEditorDialog.h
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
            QtNodes::QtNodes
            Boost::boost
            cpr::cpr
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
            foreach(tgt IN ITEMS CognitivePipelines unit_tests integration_tests)
                if(TARGET ${tgt})
                    # Deploy Qt frameworks into the app bundle so it runs outside the build machine
                    # Only run macdeployqt for Release/RelWithDebInfo builds to avoid duplicate framework warnings
                    # in Debug builds when using Homebrew Qt (system frameworks are used via rpath)
                    if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
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
                                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:${tgt}>" -always-overwrite -codesign=-
                                    COMMENT "Running macdeployqt on $<TARGET_BUNDLE_DIR:${tgt}>"
                                    VERBATIM)
                        else()
                            message(WARNING "macdeployqt not found; Qt frameworks may be missing from ${tgt}.app")
                        endif()
                    else()
                        message(STATUS "Skipping macdeployqt for ${tgt} in ${CMAKE_BUILD_TYPE} build (using system Qt frameworks)")
                    endif()

                    # Ad-hoc sign the resulting bundle so embedded frameworks have a valid cdhash
                    add_custom_command(TARGET ${tgt} POST_BUILD
                        COMMAND ${CODESIGN_EXECUTABLE} --force --deep --options runtime
                                --entitlements ${MACOSX_ENTITLEMENTS_FILE}
                                --sign "${APPLE_SIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:${tgt}>"
                        COMMENT "Ad-hoc codesigning ${tgt}.app with entitlements"
                        VERBATIM)
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
        add_custom_target(macos_deploy_all)
        if(TARGET CognitivePipelines)
            add_dependencies(macos_deploy_all CognitivePipelines)
            if(MACDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:CognitivePipelines>" -always-overwrite -codesign=-
                    COMMENT "macdeployqt CognitivePipelines.app"
                    VERBATIM)
            endif()
            add_custom_command(TARGET macos_deploy_all POST_BUILD
                COMMAND ${CODESIGN_EXECUTABLE} --force --deep --options runtime
                        --entitlements ${MACOSX_ENTITLEMENTS_FILE}
                        --sign - "$<TARGET_BUNDLE_DIR:CognitivePipelines>"
                COMMENT "Ad-hoc codesigning CognitivePipelines.app with entitlements"
                VERBATIM)
        endif()
        if(TARGET unit_tests)
            add_dependencies(macos_deploy_all unit_tests)
            if(MACDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:unit_tests>" -always-overwrite -codesign=-
                    COMMENT "macdeployqt unit_tests.app"
                    VERBATIM)
            endif()
            add_custom_command(TARGET macos_deploy_all POST_BUILD
                COMMAND ${CODESIGN_EXECUTABLE} --force --deep --options runtime
                        --entitlements ${MACOSX_ENTITLEMENTS_FILE}
                        --sign - "$<TARGET_BUNDLE_DIR:unit_tests>"
                COMMENT "Ad-hoc codesigning unit_tests.app with entitlements"
                VERBATIM)
        endif()
        if(TARGET integration_tests)
            add_dependencies(macos_deploy_all integration_tests)
            if(MACDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET macos_deploy_all POST_BUILD
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:integration_tests>" -always-overwrite -codesign=-
                    COMMENT "macdeployqt integration_tests.app"
                    VERBATIM)
            endif()
            add_custom_command(TARGET macos_deploy_all POST_BUILD
                COMMAND ${CODESIGN_EXECUTABLE} --force --deep --options runtime
                        --entitlements ${MACOSX_ENTITLEMENTS_FILE}
                        --sign - "$<TARGET_BUNDLE_DIR:integration_tests>"
                COMMENT "Ad-hoc codesigning integration_tests.app with entitlements"
                VERBATIM)
        endif()
    else()
        message(WARNING "codesign tool not found; macOS entitlements will not be applied by macos_deploy_all.")
    endif()
endif()

include(CPack)


# Live execution-state highlighting sources
target_sources(CognitivePipelines PRIVATE
    ${SRC_DIR}/ExecutionAwarePainters.cpp
    ${SRC_DIR}/ExecutionStateModel.cpp
)
