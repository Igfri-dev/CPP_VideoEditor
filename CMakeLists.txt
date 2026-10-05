cmake_minimum_required(VERSION 3.16)
project(CPP_VideoEditor LANGUAGES CXX)

# macOS sysroot handling to prevent CommandLineTools vs Xcode SDK mismatch
if(APPLE)
    if(EXISTS "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")
        set(CMAKE_OSX_SYSROOT "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk" CACHE PATH "OSX sysroot" FORCE)
    endif()
endif()

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure MSVC math defines (M_PI, etc.) are available on Windows
add_compile_definitions(_USE_MATH_DEFINES)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt 6
find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)

# -----------------------------------------------------------------------------
# Find FFmpeg (Multiplatform: macOS Homebrew, Linux PkgConfig, Windows vcpkg/FFMPEG_DIR)
# -----------------------------------------------------------------------------
find_package(PkgConfig QUIET)
set(FFMPEG_FOUND_VIA_PKGCONFIG FALSE)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG_PKG QUIET IMPORTED_TARGET libavformat libavcodec libswscale libswresample libavutil)
    if(FFMPEG_PKG_FOUND)
        set(FFMPEG_FOUND_VIA_PKGCONFIG TRUE)
        message(STATUS "FFmpeg libraries found via pkg-config")
        set(FFMPEG_INCLUDE_DIRS ${FFMPEG_PKG_INCLUDE_DIRS})
        set(FFMPEG_LIBRARIES PkgConfig::FFMPEG_PKG)
    endif()
endif()

if(NOT FFMPEG_FOUND_VIA_PKGCONFIG)
    set(FFMPEG_SEARCH_INCLUDE
        /opt/homebrew/include
        /usr/local/include
        /usr/include
        /usr/include/x86_64-linux-gnu
        "$ENV{FFMPEG_DIR}/include"
        "C:/ffmpeg/include"
    )
    set(FFMPEG_SEARCH_LIB
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        "$ENV{FFMPEG_DIR}/lib"
        "C:/ffmpeg/lib"
    )

    find_path(AVFORMAT_INCLUDE_DIR NAMES libavformat/avformat.h PATHS ${FFMPEG_SEARCH_INCLUDE})
    find_library(AVFORMAT_LIB NAMES avformat PATHS ${FFMPEG_SEARCH_LIB})
    find_library(AVCODEC_LIB NAMES avcodec PATHS ${FFMPEG_SEARCH_LIB})
    find_library(SWSCALE_LIB NAMES swscale PATHS ${FFMPEG_SEARCH_LIB})
    find_library(SWRESAMPLE_LIB NAMES swresample PATHS ${FFMPEG_SEARCH_LIB})
    find_library(AVUTIL_LIB NAMES avutil PATHS ${FFMPEG_SEARCH_LIB})

    set(FFMPEG_INCLUDE_DIRS ${AVFORMAT_INCLUDE_DIR})
    set(FFMPEG_LIBRARIES
        ${AVFORMAT_LIB}
        ${AVCODEC_LIB}
        ${SWSCALE_LIB}
        ${SWRESAMPLE_LIB}
        ${AVUTIL_LIB}
    )
endif()

add_executable(CPP_VideoEditor
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/previewwidget.h
    src/previewwidget.cpp
    src/timelinewidget.h
    src/timelinewidget.cpp
    src/core/clip.h
    src/core/clip.cpp
    src/core/colorcurve.h
    src/core/colorcurve.cpp
    src/core/motionpath.h
    src/core/motionpath.cpp
    src/core/track.h
    src/core/track.cpp
    src/core/marker.h
    src/core/projectserializer.h
    src/core/projectserializer.cpp
    src/core/timelinemodel.h
    src/core/timelinemodel.cpp
    src/medialibrary/mediaitem.h
    src/medialibrary/mediaitem.cpp
    src/medialibrary/medialibrarywidget.h
    src/medialibrary/medialibrarywidget.cpp
    src/engine/videoframedecoder.h
    src/engine/videoframedecoder.cpp
    src/engine/waveformgenerator.h
    src/engine/waveformgenerator.cpp
    src/engine/videocompositor.h
    src/engine/videocompositor.cpp
    src/engine/audioengine.h
    src/engine/audioengine.cpp
    src/engine/videoexporter.h
    src/engine/videoexporter.cpp
    src/engine/exportdialog.h
    src/engine/exportdialog.cpp
    src/inspector/inspectorwidget.h
    src/inspector/inspectorwidget.cpp
    src/inspector/curveeditorwidget.h
    src/inspector/curveeditorwidget.cpp
    src/snapping/snappingtypes.h
    src/snapping/canvassnappingengine.h
    src/snapping/canvassnappingengine.cpp
    src/snapping/timelinesnappingengine.h
    src/snapping/timelinesnappingengine.cpp
    resources/main.qrc
)

target_include_directories(CPP_VideoEditor PRIVATE
    src
    src/core
    src/medialibrary
    src/engine
    src/inspector
    src/snapping
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(CPP_VideoEditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    ${FFMPEG_LIBRARIES}
)

# Test executable
add_executable(test_editor
    tests/test_editor.cpp
    src/core/clip.h
    src/core/clip.cpp
    src/core/colorcurve.h
    src/core/colorcurve.cpp
    src/core/motionpath.h
    src/core/motionpath.cpp
    src/core/track.h
    src/core/track.cpp
    src/core/marker.h
    src/core/projectserializer.h
    src/core/projectserializer.cpp
    src/core/timelinemodel.h
    src/core/timelinemodel.cpp
    src/timelinewidget.h
    src/timelinewidget.cpp
    src/medialibrary/mediaitem.h
    src/medialibrary/mediaitem.cpp
    src/engine/videoframedecoder.h
    src/engine/videoframedecoder.cpp
    src/engine/waveformgenerator.h
    src/engine/waveformgenerator.cpp
    src/engine/videocompositor.h
    src/engine/videocompositor.cpp
    src/engine/videoexporter.h
    src/engine/videoexporter.cpp
    src/engine/audioengine.h
    src/engine/audioengine.cpp
    src/snapping/snappingtypes.h
    src/snapping/canvassnappingengine.h
    src/snapping/canvassnappingengine.cpp
    src/snapping/timelinesnappingengine.h
    src/snapping/timelinesnappingengine.cpp
    src/inspector/inspectorwidget.h
    src/inspector/inspectorwidget.cpp
    src/inspector/curveeditorwidget.h
    src/inspector/curveeditorwidget.cpp
)

target_include_directories(test_editor PRIVATE
    src
    src/core
    src/medialibrary
    src/engine
    src/snapping
    src/inspector
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(test_editor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    ${FFMPEG_LIBRARIES}
)
