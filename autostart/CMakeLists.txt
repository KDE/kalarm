include_directories( ../  )

########### next target ###############

set(kalarmautostart_SRCS 
	autostart.cpp 
)

kde4_add_app_icon(kalarmautostart_SRCS "../ox*-app-kalarm.png")

kde4_add_executable(kalarmautostart ${kalarmautostart_SRCS})

target_link_libraries(kalarmautostart ${KDE4_KDEUI_LIBS} )

install(TARGETS kalarmautostart  DESTINATION ${BIN_INSTALL_DIR} )
