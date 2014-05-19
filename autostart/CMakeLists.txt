include_directories( ../  )

########### next target ###############

set(kalarmautostart_SRCS 
	autostart.cpp 
)

kde4_add_app_icon(kalarmautostart_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../appicons/ox*-app-kalarm.png")

add_executable(kalarmautostart ${kalarmautostart_SRCS})

target_link_libraries(kalarmautostart  KF5::KDELibs4Support)

install(TARGETS kalarmautostart  ${INSTALL_TARGETS_DEFAULT_ARGS} )
