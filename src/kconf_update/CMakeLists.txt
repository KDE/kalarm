install(FILES kalarm.upd
       	DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR}
)
add_executable(kalarm-3.10.0-run_mode "kalarm-3.10.0-run_mode.cpp")
target_link_libraries(kalarm-3.10.0-run_mode
    KF6::ConfigCore
    KF6::XmlGui
)

install(
    TARGETS
        kalarm-3.10.0-run_mode
    DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin
)
