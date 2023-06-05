ecm_install_icons(
    ICONS
    16-actions-show-today.svg
    22-actions-show-today.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME breeze)

ecm_install_icons(
    ICONS
    dark/16-actions-show-today.svg
    dark/22-actions-show-today.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME breeze-dark)

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
    16-actions-show-today.svg
    22-actions-show-today.svg
    DESTINATION ${KDE_INSTALL_DATADIR}/kalarm/icons
    THEME oxygen)

ecm_install_icons(
    ICONS
    128-apps-kalarm.png
    16-apps-kalarm.png
    22-apps-kalarm.png
    32-apps-kalarm.png
    48-apps-kalarm.png
    64-apps-kalarm.png
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor)
