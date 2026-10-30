cmake_minimum_required(VERSION 3.21)
project(WaylandAutoclicker VERSION 2.0.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets DBus)

include(GNUInstallDirs)
include(CTest)

add_subdirectory(tests)

set(SOURCES
    src/main.cpp
    src/TimingPolicy.cpp
    src/ClickController.cpp
    src/PortalManager.cpp
    src/Settings.cpp
    src/TrayController.cpp
    src/MainWindow.cpp
    src/DebugDialog.cpp
)

set(HEADERS
    src/InputInjector.hpp
    src/TimingPolicy.hpp
    src/ClickController.hpp
    src/PortalManager.hpp
    src/Settings.hpp
    src/TrayController.hpp
    src/MainWindow.hpp
    src/DebugDialog.hpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
    add_executable(wayland-autoclicker ${SOURCES} ${HEADERS})

    target_include_directories(wayland-autoclicker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_link_libraries(wayland-autoclicker PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
    )

    target_compile_options(wayland-autoclicker PRIVATE
        -Wall -Wextra
        -fstack-protector-strong
        $<$<NOT:$<CONFIG:Debug>>:-O2 -D_FORTIFY_SOURCE=2>
        -Wformat -Wformat-security
    )

    target_link_options(wayland-autoclicker PRIVATE
        -Wl,-z,relro,-z,now
        -Wl,-z,noexecstack
    )

    install(TARGETS wayland-autoclicker
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES data/fr.perso.Autoclicker.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES data/icons/48x48/fr.perso.Autoclicker.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
    )
endif()
