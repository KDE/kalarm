ecm_install_icons(
    ICONS
    oxygen/16-actions-document-new-from-template.png
    oxygen/22-actions-kalarm.png
    oxygen/22-actions-document-new-from-template.png
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
