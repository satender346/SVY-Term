cmake_minimum_required(VERSION 3.21)
project(SVYTerm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

find_package(PkgConfig REQUIRED)
pkg_check_modules(VTERM REQUIRED IMPORTED_TARGET vterm)

option(SVYTERM_ENABLE_LIBSSH "Enable libssh backend" ON)

set(SVYTERM_HAS_LIBSSH 0)
if(SVYTERM_ENABLE_LIBSSH)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSSH QUIET IMPORTED_TARGET libssh)
        if(LIBSSH_FOUND)
            set(SVYTERM_HAS_LIBSSH 1)
            message(STATUS "libssh found: SSH/SFTP native backend enabled")
        else()
            message(WARNING "libssh not found: building without native SSH/SFTP backend")
        endif()
    else()
        message(WARNING "pkg-config not found: building without native SSH/SFTP backend")
    endif()
endif()

add_executable(SVY-Term
    src/app/main.cpp
    src/app/Application.cpp
    src/app/Application.h
    src/core/SessionTypes.h
    src/core/SessionManager.cpp
    src/core/SessionManager.h
    src/config/AppConfig.cpp
    src/config/AppConfig.h
    src/protocols/SftpClient.cpp
    src/protocols/SftpClient.h
    src/protocols/CredentialCache.cpp
    src/protocols/CredentialCache.h
    src/protocols/CredentialManager.cpp
    src/protocols/CredentialManager.h
    src/tunnels/TunnelManager.cpp
    src/tunnels/TunnelManager.h
    src/terminal/TerminalSession.h
    src/terminal/PtySession.cpp
    src/terminal/PtySession.h
    src/terminal/TerminalView.cpp
    src/terminal/TerminalView.h
    src/terminal/TerminalPane.cpp
    src/terminal/TerminalPane.h
    src/terminal/TerminalWidget.cpp
    src/terminal/TerminalWidget.h
    src/terminal/SshTerminalWidget.cpp
    src/terminal/SshTerminalWidget.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/SessionDialog.cpp
    src/ui/SessionDialog.h
)

target_include_directories(SVY-Term PRIVATE src)

target_compile_definitions(SVY-Term PRIVATE SVYTERM_HAS_LIBSSH=${SVYTERM_HAS_LIBSSH})
target_compile_definitions(SVY-Term PRIVATE SVYTERM_VERSION="${PROJECT_VERSION}")

target_link_libraries(SVY-Term PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    PkgConfig::VTERM
)

if(NOT APPLE)
    target_link_libraries(SVY-Term PRIVATE util)
endif()

if(SVYTERM_HAS_LIBSSH)
    target_link_libraries(SVY-Term PRIVATE PkgConfig::LIBSSH)
endif()

if(APPLE)
    set_target_properties(SVY-Term PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "SVY-Term"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.svy.svyterm"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
    target_link_libraries(SVY-Term PRIVATE "-framework Security")
endif()

install(TARGETS SVY-Term
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
