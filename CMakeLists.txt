cmake_minimum_required(VERSION 3.16)
project(unitray-settings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(unitray-settings
    gui/main.cpp
    gui/app_icon.h
    gui/autostart.h
    gui/autostart.cpp
    gui/about_dialog.h
    gui/about_dialog.cpp
    gui/settings_window.h
    gui/settings_window.cpp
)

target_link_libraries(unitray-settings PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS unitray-settings DESTINATION bin)
