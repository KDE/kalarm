/*
 *  mainwindowbase.h  -  base class for main application windows
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002, 2006, 2007, 2015 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KXmlGuiWindow>

/**
 *  The MainWindowBase class is a base class for KAlarm's main window and message window.
 *  When the window is closed, it only allows the application to quit if there is no
 *  system tray icon displayed.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class MainWindowBase : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindowBase(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Window);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent*) override;
#else
    void enterEvent(QEvent*) override;
#endif
};

// vim: et sw=4:
