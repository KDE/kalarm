#
# SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
# SPDX-FileCopyrightText: 2019 Heiko Becker <heirecka@exherbo.org>
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
# SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
# SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
# FindLibmpv
# ----------
#
# Find the mpv media player client library.
#
# Defines the following variables:
#
# - Libmpv_FOUND
#     True if it finds the library and include directory
#
# - Libmpv_INCLUDE_DIRS
#     The libmpv include dirs for use with target_include_directories
#
# - Libmpvb_LIBRARIES
#     The libmpv libraries for use with target_link_libraries()
#
# - Libmpv_VERSION
#     The version of the found libmpv
#
#
# Defines the following imported target if 'Libmpv_FOUND' is true:
#
# - Libmpv::Libmpv
#

find_package(PkgConfig QUIET)

pkg_search_module(PC_MPV QUIET mpv)

find_path(Libmpv_INCLUDE_DIRS mpv/client.h
    HINTS "$ENV{LIBMPV_INCLUDE_PATH}" ${PC_MPV_INCLUDEDIR} ${PC_MPV_INCLUDE_DIRS}
)

find_library(Libmpv_LIBRARIES
    NAMES mpv
    HINTS ${PC_MPV_LIBDIR}
)

set(Libmpv_VERSION ${PC_MPV_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmpv
    FOUND_VAR
        Libmpv_FOUND
    REQUIRED_VARS
        Libmpv_LIBRARIES
        Libmpv_INCLUDE_DIRS
    VERSION_VAR
        Libmpv_VERSION
)

if(NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
    message(STATUS "Found Libmpv version: ${Libmpv_VERSION}")
endif (NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)

if (Libmpv_FOUND AND NOT TARGET Libmpv::Libmpv)
    add_library(Libmpv::Libmpv UNKNOWN IMPORTED)
    set_target_properties(Libmpv::Libmpv PROPERTIES
        IMPORTED_LOCATION "${Libmpv_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libmpv_INCLUDE_DIRS}"
    )
    # TODO: we should eventually use pkg_check_module to create a target
    if (ANDROID)
        target_link_libraries(Libmpv::Libmpv INTERFACE ${PC_MPV_LINK_LIBRARIES})
    endif()
endif()

if(NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
    message(STATUS "Found LibMPV include-dir path: ${Libmpv_INCLUDE_DIRS}")
    message(STATUS "Found LibMPV library path:${Libmpv_LIBRARIES}")
    message(STATUS "Found LibMPV version: ${Libmpv_VERSION}")
endif (NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)

mark_as_advanced(Libmpv_LIBRARIES Libmpv_INCLUDE_DIRS)

include(FeatureSummary)
set_package_properties(Libmpv PROPERTIES
    URL "https://mpv.io"
    DESCRIPTION "mpv media player client library"
)
