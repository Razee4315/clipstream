cmake_minimum_required(VERSION 3.21)

project(ClipStream VERSION 0.2.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg)

# --- Platform-specific implementations (swapped at build time) -----------------
# The headers in src/platform/ define the contract; the .cpp picked here is the
# implementation for the target OS. Windows is complete; others are stubs for now.
set(PLATFORM_SOURCES)
if(WIN32)
    enable_language(RC)
    list(APPEND PLATFORM_SOURCES
        src/platform/win/HotkeyManager_win.cpp
        src/platform/win/ForegroundApp_win.cpp
        src/platform/win/PasteSimulator_win.cpp
        src/platform/win/Autostart_win.cpp
        resources/clipstream.rc
    )
else()
    list(APPEND PLATFORM_SOURCES
        src/platform/stub/HotkeyManager_stub.cpp
        src/platform/stub/ForegroundApp_stub.cpp
        src/platform/stub/PasteSimulator_stub.cpp
        src/platform/stub/Autostart_stub.cpp
    )
endif()

set(SOURCES
    src/main.cpp
    src/theme.cpp
    src/theme.h
    src/AppController.cpp
    src/AppController.h
    # core
    src/core/Database.cpp
    src/core/Database.h
    src/core/ClipboardMonitor.cpp
    src/core/ClipboardMonitor.h
    src/core/ContentClassifier.cpp
    src/core/ContentClassifier.h
    src/core/MathEval.cpp
    src/core/MathEval.h
    src/core/ClipEntry.h
    # ui
    src/ui/OverlayWindow.cpp
    src/ui/OverlayWindow.h
    src/ui/HistoryModel.cpp
    src/ui/HistoryModel.h
    src/ui/EntryDelegate.cpp
    src/ui/EntryDelegate.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/IconFactory.cpp
    src/ui/IconFactory.h
    src/ui/RowActionsBar.cpp
    src/ui/RowActionsBar.h
    # Listed so AUTOMOC processes the Q_OBJECT header whose impl lives in a
    # differently-named platform .cpp (HotkeyManager_win.cpp / _stub.cpp).
    src/platform/HotkeyManager.h
    resources/app.qrc
    ${PLATFORM_SOURCES}
)

qt_add_executable(ClipStream WIN32 ${SOURCES})

target_include_directories(ClipStream PRIVATE
    src
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"  # lets windres find icon.ico (see guide)
)

target_link_libraries(ClipStream PRIVATE Qt6::Widgets Qt6::Sql Qt6::Svg)

if(WIN32)
    # APIs we call directly: RegisterHotKey/SendInput/GetForegroundWindow (user32),
    # OpenProcess/QueryFullProcessImageNameW (kernel32, auto-linked).
    target_link_libraries(ClipStream PRIVATE user32)
endif()
