# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: none

# This builds KAlarm's audio plugins, which provide audio backends.

set(CMAKE_VERBOSE_MAKEFILE ON)
set(audioplugin_SRCS
    audioplugin.cpp
    audioplayer.cpp
    audioplugin.h
    audioplayer.h
)

ecm_qt_declare_logging_category(audioplugin_SRCS
                                HEADER audioplugin_debug.h
				IDENTIFIER AUDIOPLUGIN_LOG
                                CATEGORY_NAME org.kde.pim.kalarm.audioplugin
                                DEFAULT_SEVERITY Warning
                                DESCRIPTION "kalarm (audio plugin)"
				EXPORT AUDIOPLUGIN
                               )

if(ENABLE_LIBVLC)
    kcoreaddons_add_plugin(audioplugin_vlc
        SOURCES
            ${audioplugin_SRCS}
            audioplayer_vlc.cpp
            audioplugin_vlc.cpp
            audioplayer_vlc.h
            audioplugin_vlc.h
        INSTALL_NAMESPACE "pim6/kalarm"
    )

    set_target_properties(audioplugin_vlc
        PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/kalarm")
    target_link_libraries(audioplugin_vlc
        kalarmplugin
	KF6::CoreAddons
        KF6::I18n
        LibVLC::LibVLC
    )
    target_include_directories(audioplugin_vlc
        PUBLIC "$<BUILD_INTERFACE:${kalarm_SOURCE_DIR}/src/audioplugin;${kalarm_BINARY_DIR}/src/audioplugin>")
    generate_export_header(audioplugin_vlc BASE_NAME audioplugin_vlc)
endif()

if(ENABLE_LIBMPV)
    kcoreaddons_add_plugin(audioplugin_mpv
        SOURCES
            ${audioplugin_SRCS}
            audioplayer_mpv.cpp
            audioplugin_mpv.cpp
            audioplayer_mpv.h
            audioplugin_mpv.h
        INSTALL_NAMESPACE "pim6/kalarm"
    )

    set_target_properties(audioplugin_mpv
        PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/kalarm")
    target_link_libraries(audioplugin_mpv
        kalarmplugin
	KF6::CoreAddons
        KF6::I18n
        Libmpv::Libmpv
    )
    target_include_directories(audioplugin_mpv
        PUBLIC "$<BUILD_INTERFACE:${kalarm_SOURCE_DIR}/src/audioplugin;${kalarm_BINARY_DIR}/src/audioplugin>")
    generate_export_header(audioplugin_mpv BASE_NAME audioplugin_mpv)
endif()
