ecm_install_icons(
    ICONS
    16-actions-document-new-from-template.png
    16-actions-new-command-alarm.png
    22-actions-kalarm-disabled.png
    22-actions-kalarm.png
    22-actions-new-command-alarm.png
    16-actions-new-audio-alarm.png
    22-actions-document-new-from-template.png
    22-actions-kalarm-partdisabled.png
    22-actions-new-audio-alarm.png
    16-actions-show-today.svg
    22-actions-show-today.svg
    DESTINATION ${KDE_INSTALL_DATADIR}/kalarm/icons
    THEME oxygen)

install(FILES 16-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze/actions/16
        RENAME show-today.svg)
install(FILES 22-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze/actions/22
        RENAME show-today.svg)

install(FILES breeze-dark/16-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze-dark/actions/16
        RENAME show-today.svg)
install(FILES breeze-dark/22-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze-dark/actions/22
        RENAME show-today.svg)
