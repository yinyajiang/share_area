cmake_minimum_required(VERSION 3.20)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")
endif()

project(ShareArea VERSION 1.0.0 LANGUAGES CXX)

# Allow overriding the app version from command line (e.g., CI)
set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/script/setup.iss.in"
    "${CMAKE_BINARY_DIR}/setup.iss"
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt path
if(NOT DEFINED CMAKE_PREFIX_PATH)
    if(APPLE)
        # 设置多个 Qt 路径，CMake 会依次尝试
        set(CMAKE_PREFIX_PATH
            "/Users/yajiang/Qt6.10/6.10.3/macos"
            "/Users/echo/Qt/6.10.2/macos"
        )
    elseif(WIN32)
        set(CMAKE_PREFIX_PATH "D:/Qt6.10/6.10.1/msvc2022_64")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Svg LinguistTools)

set(SOURCES
    src/main.cpp
    src/core/app_settings.cpp
    src/core/shared_file.cpp
    src/core/auto_start.cpp
    src/core/single_instance_guard.cpp
    src/network/peer_discovery.cpp
    src/network/file_transfer_manager.cpp
    src/ui/main_window.cpp
    src/ui/breathing_dot.cpp
    src/ui/drop_area_widget.cpp
    src/ui/file_list_widget.cpp
    src/ui/setup_dialog.cpp
    src/ui/system_tray.cpp
    src/ui/app_icon.cpp
    src/ui/log_window.cpp
)

# macOS 原生毛玻璃支持
if(APPLE)
    list(APPEND SOURCES src/ui/macos_blur.mm)
endif()

set(HEADERS
    src/constants.h
    src/core/app_settings.h
    src/core/auto_start.h
    src/core/shared_file.h
    src/core/single_instance_guard.h
    src/network/peer_discovery.h
    src/network/file_transfer_manager.h
    src/ui/main_window.h
    src/ui/breathing_dot.h
    src/ui/drop_area_widget.h
    src/ui/file_list_widget.h
    src/ui/setup_dialog.h
    src/ui/system_tray.h
    src/ui/app_icon.h
    src/ui/log_window.h
)

# Translation files
set(TS_FILES
    translations/share_area_zh_CN.ts
    translations/share_area_en.ts
)

qt_add_executable(ShareArea
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

# Handle translations
qt_create_translation(QM_FILES ${SOURCES} ${HEADERS} ${TS_FILES})
target_sources(ShareArea PRIVATE ${QM_FILES})

# Copy QM files next to the executable for runtime loading
add_custom_command(TARGET ShareArea POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:ShareArea>/translations
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${QM_FILES}
        $<TARGET_FILE_DIR:ShareArea>/translations/
)

target_include_directories(ShareArea PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

target_link_libraries(ShareArea PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
)

# macOS bundle
if(APPLE)
    find_library(APPKIT_LIBRARY AppKit)
    target_link_libraries(ShareArea PRIVATE ${APPKIT_LIBRARY})
    set_target_properties(ShareArea PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.sharearea.app
        MACOSX_BUNDLE_BUNDLE_NAME ShareArea
        MACOSX_BUNDLE_BUNDLE_VERSION ${APP_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION}
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
    )

    # Copy icon into app bundle's Resources
    set_source_files_properties(resources/icons/icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(ShareArea PRIVATE resources/icons/icon.icns)
endif()

# Windows
if(WIN32)
    set_target_properties(ShareArea PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
