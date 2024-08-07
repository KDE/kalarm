# CMake module to search for LIBVLC (VLC library)
#
# Copyright (C) 2024, David Jarvie <djarvie@kde.org>
# Copyright (C) 2011-2018, Harald Sitter <sitter@kde.org>
# Copyright (C) 2010, Rohit Yadav <rohityadav89@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# If it's found it sets LIBVLC_FOUND to TRUE
# and following variables are set:
#    LIBVLC_INCLUDE_DIR
#    LIBVLC_LIBRARY
#    LIBVLC_VERSION

# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

if (NOT WIN32)
    find_package(PkgConfig)
    pkg_check_modules(PC_LIBVLC libvlc)
    set(LIBVLC_DEFINITIONS ${PC_LIBVLC_CFLAGS_OTHER})
endif (NOT WIN32)

#Put here path to custom location
#example: /home/user/vlc/include etc..
find_path(LIBVLC_INCLUDE_DIR vlc/vlc.h
HINTS "$ENV{LIBVLC_INCLUDE_PATH}" ${PC_LIBVLC_INCLUDEDIR} ${PC_LIBVLC_INCLUDE_DIRS}
PATHS
    "$ENV{LIB_DIR}/include"
    "$ENV{LIB_DIR}/include/vlc"
    "/usr/include"
    "/usr/include/vlc"
    "/usr/local/include"
    "/usr/local/include/vlc"
    #mingw
    c:/msys/local/include
)
find_path(LIBVLC_INCLUDE_DIR PATHS "${CMAKE_INCLUDE_PATH}/vlc" NAMES vlc.h 
        HINTS ${PC_LIBVLC_INCLUDEDIR} ${PC_LIBVLC_INCLUDE_DIRS})

#Put here path to custom location
#example: /home/user/vlc/lib etc..
find_library(LIBVLC_LIBRARY NAMES vlc libvlc
HINTS "$ENV{LIBVLC_LIBRARY_PATH}" ${PC_LIBVLC_LIBDIR} ${PC_LIBVLC_LIBRARY_DIRS}
PATHS
    "$ENV{LIB_DIR}/lib"
    #mingw
    c:/msys/local/lib
)
find_library(LIBVLC_LIBRARY NAMES vlc libvlc)
find_library(LIBVLCCORE_LIBRARY NAMES vlccore libvlccore
HINTS "$ENV{LIBVLC_LIBRARY_PATH}" ${PC_LIBVLC_LIBDIR} ${PC_LIBVLC_LIBRARY_DIRS}
PATHS
    "$ENV{LIB_DIR}/lib"
    #mingw
    c:/msys/local/lib
)
find_library(LIBVLCCORE_LIBRARY NAMES vlccore libvlccore)

set(LIBVLC_VERSION ${PC_LIBVLC_VERSION})
if (NOT LIBVLC_VERSION)
    file(READ "${LIBVLC_INCLUDE_DIR}/vlc/libvlc_version.h" _libvlc_version_h)

    string(REGEX MATCH "# define LIBVLC_VERSION_MAJOR +\\(([0-9])\\)" _dummy "${_libvlc_version_h}")
    set(_version_major "${CMAKE_MATCH_1}")

    string(REGEX MATCH "# define LIBVLC_VERSION_MINOR +\\(([0-9])\\)" _dummy "${_libvlc_version_h}")
    set(_version_minor "${CMAKE_MATCH_1}")

    string(REGEX MATCH "# define LIBVLC_VERSION_REVISION +\\(([0-9])\\)" _dummy "${_libvlc_version_h}")
    set(_version_revision "${CMAKE_MATCH_1}")

    # Optionally, one could also parse LIBVLC_VERSION_EXTRA, but it does not
    # seem to be used by libvlc.pc.

    set(LIBVLC_VERSION "${_version_major}.${_version_minor}.${_version_revision}")
endif (NOT LIBVLC_VERSION)

if (LIBVLC_INCLUDE_DIR AND LIBVLC_LIBRARY AND LIBVLCCORE_LIBRARY)
set(LIBVLC_FOUND TRUE)
endif (LIBVLC_INCLUDE_DIR AND LIBVLC_LIBRARY AND LIBVLCCORE_LIBRARY)

if(NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
    message(STATUS "Found LibVLC include-dir path: ${LIBVLC_INCLUDE_DIR}")
    message(STATUS "Found LibVLC library path:${LIBVLC_LIBRARY}")
    message(STATUS "Found LibVLCcore library path:${LIBVLCCORE_LIBRARY}")
    message(STATUS "Found LibVLC version: ${LIBVLC_VERSION}")
endif (NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)

if(LIBVLC_FOUND AND NOT TARGET LibVLC::LibVLC)
    add_library(LibVLC::LibVLC UNKNOWN IMPORTED)
    set_target_properties(LibVLC::LibVLC PROPERTIES
        IMPORTED_LOCATION "${LIBVLC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBVLC_INCLUDE_DIR}"
    )
endif()

if(LIBVLC_FOUND AND NOT TARGET LibVLC::Core)
    add_library(LibVLC::Core UNKNOWN IMPORTED)
    set_target_properties(LibVLC::Core PROPERTIES
        IMPORTED_LOCATION "${LIBVLCCORE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBVLC_INCLUDE_DIR}"
    )
endif()
