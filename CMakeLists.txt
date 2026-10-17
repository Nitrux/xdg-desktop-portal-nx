# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.24)

project(xdg-desktop-portal-nx
    VERSION 1.0.0
    DESCRIPTION "Nitrux XDG Desktop Portal backend"
    LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(ECM 6.13 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})

option(NX_PORTAL_ENABLE_DEBUG_OUTPUT
    "Build the portal backend with debug and informational logging" OFF)

set(QT_NO_PRIVATE_MODULE_WARNING ON CACHE BOOL
    "MauiKit uses Qt's private WaylandClient API" FORCE)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(MauiKit4 4.0.5 REQUIRED)
find_package(MauiKitFileBrowsing4 4.0.5 REQUIRED)

add_executable(xdg-desktop-portal-nx
    src/main.cpp
    src/portalrequest.cpp
    src/portalrequest.hpp
    src/filechooser.cpp
    src/filechooser.hpp
    src/appchooser.cpp
    src/appchooser.hpp
    src/openuri.cpp
    src/openuri.hpp
    src/settings.cpp
    src/settings.hpp)

qt_add_resources(xdg-desktop-portal-nx nx_qml
    PREFIX "/nx/qml"
    FILES
        src/qml/FileChooserDialog.qml
        src/qml/PlacesSideBar.qml
        src/qml/AppChooserDialog.qml)

target_link_libraries(xdg-desktop-portal-nx
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        KF6::I18n
        KF6::I18nQml
        MauiKit4
        MauiKit4::FileBrowsing)

add_executable(xdg-desktop-portal-nx-test tests/main.cpp)

qt_add_resources(xdg-desktop-portal-nx-test nx_test_qml
    PREFIX "/nx/test"
    FILES tests/TestWindow.qml)

target_link_libraries(xdg-desktop-portal-nx-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        KF6::I18n
        MauiKit4)

target_compile_definitions(xdg-desktop-portal-nx
    PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING)

if(NOT NX_PORTAL_ENABLE_DEBUG_OUTPUT)
    target_compile_definitions(xdg-desktop-portal-nx PRIVATE
        QT_NO_DEBUG_OUTPUT
        QT_NO_INFO_OUTPUT)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(xdg-desktop-portal-nx PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    target_compile_options(xdg-desktop-portal-nx PRIVATE
        -march=x86-64-v3 -mtune=generic)
endif()

install(TARGETS xdg-desktop-portal-nx
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(FILES data/nx.portal
    DESTINATION "${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals")
install(FILES data/org.freedesktop.impl.portal.desktop.nx.service
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services")
install(FILES data/portals.conf
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/xdg-desktop-portal")

if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE
        @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()
