include_directories( ../  )

########### next target ###############

set(kalarmautostart_SRCS 
	autostart.cpp 
)

kde4_add_app_icon(kalarmautostart_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../appicons/ox*-app-kalarm.png")

kde4_add_executable(kalarmautostart ${kalarmautostart_SRCS})

target_link_libraries(kalarmautostart ${KDE4_KDEUI_LIBS} )

install(TARGETS kalarmautostart  ${INSTALL_TARGETS_DEFAULT_ARGS} )
