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

set(SYS_PLUGINS_DIR "<unset>" CACHE PATH "Path to system plugins directory, or empty to disable")
if(SYS_PLUGINS_DIR STREQUAL "<unset>")
    if(WITH_PORTABLE)
        set(SYS_PLUGINS_DIR "")
    else()
        set(SYS_PLUGINS_DIR "${CMAKE_INSTALL_PREFIX}/${LETOS_INSTALL_PLUGINDIR}")
        message(WARNING "SYS_PLUGINS_DIR set to \"${SYS_PLUGINS_DIR}\". This cannot be changed by `--prefix` when executing `cmake --install .`. If it is not correct, change it now!")
    endif()
endif()

add_subdirectory(core)
if(WITH_GUI)
    add_subdirectory(gui)
    add_subdirectory(letos)
endif()

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
