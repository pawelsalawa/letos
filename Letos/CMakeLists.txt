cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(version)

letos_find_version()
project(Letos VERSION "${LETOS_VERSION_STRING}")

add_custom_target(MiscFiles)
target_sources(MiscFiles PRIVATE
	../ChangeLog.md
	../README.md
)

if(WIN32 OR APPLE)
    set(WITH_PORTABLE_DEFAULT ON)
endif()

option(WITH_GUI "Build GUI application" ON)

option(WITH_PORTABLE "Create portable bundle" ${WITH_PORTABLE_DEFAULT})
option(WITH_UPDATER "Enable update manager" ${WITH_PORTABLE})
option(WITH_SUPPORT_REMINDERS "Enable occasional support reminders" ON)
option(WITH_SHARED_RES "Install resources as separate files (distro packaging only)" OFF)

set(QRC_EXTRA_ICONSETS "" CACHE STRING "Additional icon sets as .qrc files, separated by semicolons")

if(NOT WITH_CLI AND NOT WITH_GUI)
    message(FATAL_ERROR "At least one of WITH_CLI or WITH_GUI must be enabled")
endif()

include(letos_sqlite3)
include(CTest)
include(common)

if(WITH_PORTABLE)
    # For portable builds, always use the install plugin dir from dirs.cmake.
    set(SYS_PLUGINS_DIR "${LETOS_INSTALL_PLUGINDIR}" CACHE PATH "Path to system plugins directory")
else()
    # For non-portable builds: if the user did not provide SYS_PLUGINS_DIR on the cmake command line,
    # set it to the install plugin dir and warn. If the user provided it, preserve and show it.
    if(NOT DEFINED SYS_PLUGINS_DIR)
        set(SYS_PLUGINS_DIR "${LETOS_INSTALL_PLUGINDIR}" CACHE PATH "Path to system plugins directory")
        message(STATUS "SYS_PLUGINS_DIR: ${SYS_PLUGINS_DIR}")
        message(WARNING "The SYS_PLUGINS_DIR cannot be changed by `--prefix` when executing `cmake --install .`. If it is not correct, change it now!")
    else()
        # Ensure the cache entry exists with the user-provided value (no warning).
        set(SYS_PLUGINS_DIR "${SYS_PLUGINS_DIR}" CACHE PATH "Path to system plugins directory")
    endif()
    message(STATUS "SYS_PLUGINS_DIR: ${SYS_PLUGINS_DIR}")
endif()

add_subdirectory(core)
if(WITH_GUI)
    add_subdirectory(gui)
    add_subdirectory(letos)
endif()

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
