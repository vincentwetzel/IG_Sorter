cmake_minimum_required(VERSION 3.16)
project(IG_Sorter_Qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent)

qt_standard_project_setup()

# Source files
set(SOURCES
    src/main.cpp
    src/core/FileNameParser.cpp
    src/core/FileGrouper.cpp
    src/core/DatabaseManager.cpp
    src/core/DirectoryCleanup.cpp
    src/core/SorterEngine.cpp
    src/utils/ConfigManager.cpp
    src/utils/FileUtils.cpp
    src/utils/ThemeManager.cpp
    src/utils/LogManager.cpp
    src/ui/MainWindow.cpp
    src/ui/MenuScreen.cpp
    src/ui/CleanupScreen.cpp
    src/ui/SortingScreen.cpp
    src/ui/ReportScreen.cpp
    src/ui/SettingsDialog.cpp
    src/ui/ImagePreviewGrid.cpp
    src/ui/ImageThumbnail.cpp
    src/ui/SortPanel.cpp
    src/ui/ThumbnailWithLabel.cpp
    src/ui/FlowLayout.cpp
    src/ui/AddPersonDialog.cpp
)

set(HEADERS
    src/core/FileNameParser.h
    src/core/FileGrouper.h
    src/core/DatabaseManager.h
    src/core/DirectoryCleanup.h
    src/core/SorterEngine.h
    src/core/Types.h
    src/utils/ConfigManager.h
    src/utils/FileUtils.h
    src/utils/ThemeManager.h
    src/utils/LogManager.h
    src/ui/MainWindow.h
    src/ui/MenuScreen.h
    src/ui/CleanupScreen.h
    src/ui/SortingScreen.h
    src/ui/ReportScreen.h
    src/ui/SettingsDialog.h
    src/ui/ImagePreviewGrid.h
    src/ui/ImageThumbnail.h
    src/ui/SortPanel.h
    src/ui/ThumbnailWithLabel.h
    src/ui/FlowLayout.h
    src/ui/AddPersonDialog.h
)

# Resource file
set(RESOURCES resources/resources.qrc)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Install
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
