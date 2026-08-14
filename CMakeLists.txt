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

find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(MauiKit4 4.0 REQUIRED)
find_package(MauiKitFileBrowsing4 4.0 REQUIRED)

add_executable(xdg-desktop-portal-nx
    src/main.cpp
    src/portalrequest.cpp
    src/portalrequest.hpp
    src/filechooser.cpp
    src/filechooser.hpp
    src/appchooser.cpp
    src/appchooser.hpp
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
        MauiKit4
        MauiKit4::FileBrowsing)

target_compile_definitions(xdg-desktop-portal-nx
    PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING)

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


set(CPACK_PACKAGE_NAME "xdg-desktop-portal-nx")
set(CPACK_DEBIAN_PACKAGE_NAME "xdg-desktop-portal-nx")
set(CPACK_PACKAGE_VENDOR "Nitrux Latinoamericana S.C.")
set(CPACK_PACKAGE_CONTACT "hello@nxos.org")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MauiKit XDG Desktop Portal backend for Nitrux")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "uri_herrera@nxos.org")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "xdg-desktop-portal, xdg-desktop-portal-hyprland (>= 1.3.12), mauikit (>= 4.0.4), mauikit-filebrowsing (>= 4.0.4), qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qt6-wayland")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
