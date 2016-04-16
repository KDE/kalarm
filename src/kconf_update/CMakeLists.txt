install(FILES kalarm.upd
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR}
)
install(PROGRAMS kalarm-version.pl
                 kalarm-1.2.1-general.pl
                 kalarm-1.9.5-defaults.pl
                 kalarm-2.0.2-general.pl
                 kalarm-2.1.5-general.pl
                 kalarm-15.08-kickoff.sh
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR}
)
