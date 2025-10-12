cmake_minimum_required(VERSION 3.21)

# Project configuration
project(CognitivePipelines VERSION 0.1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt's automatic processing (safe even if not used yet)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
# Require Boost headers (Boost.Process is header-only in this usage)
find_package(Boost 1.70 REQUIRED COMPONENTS headers)

# Fetch cpr via FetchContent
include(FetchContent)
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
)
# Ensure transitive dependencies are made available
FetchContent_MakeAvailable(cpr)



# Sources
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(CognitivePipelines
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow.h
    ${SRC_DIR}/IToolConnector.h
    ${SRC_DIR}/PythonScriptConnector.cpp
    ${SRC_DIR}/PythonScriptConnector.h
)


# Link libraries
target_link_libraries(CognitivePipelines PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    cpr::cpr
)

# Boost headers (required)
# Link against Boost::headers target provided by find_package(Boost)
target_link_libraries(CognitivePipelines PRIVATE Boost::headers)

# On macOS and Windows, optionally create a GUI app without console if needed
if(WIN32)
    # Uncomment to build a WIN32 subsystem GUI app:
    # set_target_properties(CognitivePipelines PROPERTIES WIN32_EXECUTABLE YES)
elseif(APPLE)
    # Uncomment to build a macOS app bundle:
    # set_target_properties(CognitivePipelines PROPERTIES MACOSX_BUNDLE YES)
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

include(CPack)
