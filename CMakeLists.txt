cmake_minimum_required(VERSION 3.16)
project(EASy68KLinux VERSION 5.16.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings as errors required by project policy
add_compile_options(-Wall -Wextra -Werror -fPIC)
set(LEGACY_CLANG_SUPPRESSIONS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(LEGACY_CLANG_SUPPRESSIONS
        -Wno-unknown-warning-option
        -Wno-unused-but-set-variable
        -Wno-unused-const-variable
        -Wno-parentheses
        -Wno-dangling-else
        -Wno-char-subscripts
        -Wno-missing-braces
        -Wno-writable-strings
        -Wno-missing-field-initializers
    )
endif()

# -----------------------------------------------------------------------------
# 1. Standalone Assembler (easy68k-asm)
# -----------------------------------------------------------------------------
set(ASM_SOURCES
    src/assembler/assemble.cpp
    src/assembler/binfile.cpp
    src/assembler/build.cpp
    src/assembler/codegen.cpp
    src/assembler/directiv.cpp
    src/assembler/error.cpp
    src/assembler/eval.cpp
    src/assembler/globals.cpp
    src/assembler/instlook.cpp
    src/assembler/insttabl.cpp
    src/assembler/listing.cpp
    src/assembler/macro.cpp
    src/assembler/movem.cpp
    src/assembler/object.cpp
    src/assembler/opparse.cpp
    src/assembler/structured.cpp
    src/assembler/symbol.cpp
    src/assembler/main.cpp
)

add_executable(easy68k-asm ${ASM_SOURCES})
target_include_directories(easy68k-asm PRIVATE src/assembler)
target_compile_options(easy68k-asm PRIVATE ${LEGACY_CLANG_SUPPRESSIONS})

# -----------------------------------------------------------------------------
# 2. Simulator Core Library & Standalone CLI Simulator (easy68k-sim)
# -----------------------------------------------------------------------------
set(SIM_CORE_SOURCES
    src/simulator/bpoint.cpp
    src/simulator/bpointexpr.cpp
    src/simulator/code1.cpp
    src/simulator/code2.cpp
    src/simulator/code3.cpp
    src/simulator/code4.cpp
    src/simulator/code5.cpp
    src/simulator/code6.cpp
    src/simulator/code7.cpp
    src/simulator/code8.cpp
    src/simulator/code9.cpp
    src/simulator/run.cpp
    src/simulator/scan.cpp
    src/simulator/simio.cpp
    src/simulator/simmessage.cpp
    src/simulator/simops1.cpp
    src/simulator/simops2.cpp
    src/simulator/startsim.cpp
    src/simulator/strutils.cpp
    src/simulator/utils.cpp
    src/simulator/sim_globals.cpp
    src/simulator/sim_periph.cpp
    src/simulator/disassembler.cpp
)

add_library(easy68k_sim_core STATIC ${SIM_CORE_SOURCES})
target_include_directories(easy68k_sim_core PUBLIC src/simulator)
target_compile_options(easy68k_sim_core PRIVATE ${LEGACY_CLANG_SUPPRESSIONS})

add_executable(easy68k-sim src/simulator/main.cpp)
target_link_libraries(easy68k-sim PRIVATE easy68k_sim_core)
target_compile_options(easy68k-sim PRIVATE ${LEGACY_CLANG_SUPPRESSIONS})

# -----------------------------------------------------------------------------
# 3. EASyBIN Utility (easybin)
# -----------------------------------------------------------------------------
add_executable(easybin
    src/easybin/fileio.cpp
    src/easybin/main.cpp
)
target_include_directories(easybin PRIVATE src/easybin)
target_compile_options(easybin PRIVATE ${LEGACY_CLANG_SUPPRESSIONS})

# -----------------------------------------------------------------------------
# 4. Qt6 Native GUI IDE & Simulator (easy68k-gui)
# -----------------------------------------------------------------------------
find_package(Qt6 COMPONENTS Core Gui Widgets Network Test QUIET)
find_package(Qt6 COMPONENTS Multimedia QUIET)

if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(GUI_CORE_SOURCES
        src/gui/mainwindow.cpp
        src/gui/codeeditor.cpp
        src/gui/syntaxhighlighter.cpp
        src/gui/findreplacebar.cpp
        src/gui/breakpointdialog.cpp
        src/gui/listingwidget.cpp
        src/gui/registersview.cpp
        src/gui/memoryview.cpp
        src/gui/stackview.cpp
        src/gui/terminalwidget.cpp
        src/gui/hardwarewidget.cpp
        src/gui/easybinwidget.cpp
        src/gui/helpwidget.cpp
        src/gui/simulatormanager.cpp
        src/gui/variablesview.cpp
        src/gui/updater.cpp
        src/easybin/fileio.cpp
    )

    add_library(easy68k_gui_core STATIC ${GUI_CORE_SOURCES})
    target_compile_definitions(easy68k_gui_core PUBLIC EASY68K_VERSION_STRING="${PROJECT_VERSION}")
    target_include_directories(easy68k_gui_core PUBLIC src/gui src/simulator src/easybin)
    target_link_libraries(easy68k_gui_core PUBLIC easy68k_sim_core Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Network)
    if(TARGET Qt6::Multimedia)
        target_compile_definitions(easy68k_gui_core PUBLIC EASY68K_HAS_MULTIMEDIA=1)
        target_link_libraries(easy68k_gui_core PUBLIC Qt6::Multimedia)
    endif()

    add_executable(easy68k-gui src/gui/main.cpp)
    target_link_libraries(easy68k-gui PRIVATE easy68k_gui_core)

    if(TARGET Qt6::Test)
        add_executable(test_gui tests/test_gui.cpp)
        target_link_libraries(test_gui PRIVATE easy68k_gui_core Qt6::Test)
    endif()
else()
    message(WARNING "Qt6 not found. easy68k-gui target will not be built.")
endif()

# -----------------------------------------------------------------------------
# Testing
# -----------------------------------------------------------------------------
enable_testing()
add_test(NAME EASy68KTestSuite
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

if(TARGET test_gui)
    add_test(NAME EASy68KGuiTest
        COMMAND test_gui
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# -----------------------------------------------------------------------------
# Installation Rules
# -----------------------------------------------------------------------------
install(TARGETS easy68k-asm easy68k-sim easybin
    RUNTIME DESTINATION bin
)

if(TARGET easy68k-gui)
    install(TARGETS easy68k-gui RUNTIME DESTINATION bin)
    install(FILES desktop/easy68k.desktop DESTINATION share/applications)
    install(FILES desktop/easy68k.appdata.xml DESTINATION share/metainfo RENAME io.github.sebo1411.easy68k.appdata.xml)
    install(FILES desktop/easy68k.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()

install(DIRECTORY examples/ DESTINATION share/easy68k/examples FILES_MATCHING PATTERN "*.x68")
install(DIRECTORY docs/help/ DESTINATION share/doc/easy68k/help)

# -----------------------------------------------------------------------------
# Packaging (CPack)
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "easy68k-linux")
set(CPACK_PACKAGE_VENDOR "EASy68K Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Motorola 68000 Assembler, Simulator & IDE for Linux")
set(CPACK_PACKAGE_VERSION_MAJOR 5)
set(CPACK_PACKAGE_VERSION_MINOR 16)
set(CPACK_PACKAGE_VERSION_PATCH 1)
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Debian package generator settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "EASy68K Linux Maintainers <https://github.com/Sebo1411/EASy68KLinux>")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6, libqt6gui6, libqt6core6, libqt6network6")
if(TARGET Qt6::Multimedia)
    string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", libqt6multimedia6")
endif()

# RPM package generator settings
set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_AUTOREQPROV OFF)
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase-gui")
if(TARGET Qt6::Multimedia)
    string(APPEND CPACK_RPM_PACKAGE_REQUIRES ", qt6-qtmultimedia")
endif()

set(CPACK_GENERATOR "TGZ;DEB")
include(CPack)
