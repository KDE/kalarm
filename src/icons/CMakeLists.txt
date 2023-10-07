install(FILES breeze/16-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze/actions/16
        RENAME show-today.svg)
install(FILES breeze/22-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze/actions/22
        RENAME show-today.svg)

install(FILES breeze-dark/16-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze-dark/actions/16
        RENAME show-today.svg)
install(FILES breeze-dark/22-actions-show-today.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/breeze-dark/actions/22
        RENAME show-today.svg)

ecm_install_icons(
    ICONS
    oxygen/16-actions-document-new-from-template.png
    oxygen/16-actions-new-command-alarm.png
    oxygen/22-actions-kalarm-disabled.png
    oxygen/22-actions-kalarm.png
    oxygen/22-actions-new-command-alarm.png
    oxygen/16-actions-new-audio-alarm.png
    oxygen/22-actions-document-new-from-template.png
    oxygen/22-actions-kalarm-partdisabled.png
    oxygen/22-actions-new-audio-alarm.png
    breeze/16-actions-show-today.svg
    breeze/22-actions-show-today.svg
    DESTINATION ${KDE_INSTALL_DATADIR}/kalarm/icons
    THEME oxygen)

ecm_install_icons(
    ICONS
    hicolor/128-apps-kalarm.png
    hicolor/16-apps-kalarm.png
    hicolor/22-apps-kalarm.png
    hicolor/32-apps-kalarm.png
    hicolor/48-apps-kalarm.png
    hicolor/64-apps-kalarm.png
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor)
