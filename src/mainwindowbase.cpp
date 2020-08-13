/*
 *  mainwindowbase.cpp  -  base class for main application windows
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002, 2003, 2007, 2015 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mainwindowbase.h"

#include "kalarmapp.h"


MainWindowBase::MainWindowBase(QWidget* parent, Qt::WindowFlags f)
    : KXmlGuiWindow(parent, f)
{
    setWindowModality(Qt::WindowModal);
}

/******************************************************************************
* Called when the mouse cursor enters the window.
* Activates this window if an Edit Alarm Dialog has activated itself.
* This is only required on Ubuntu's Unity desktop, which doesn't transfer
* keyboard focus properly between Edit Alarm Dialog windows and MessageWin
* windows.
*/
void MainWindowBase::enterEvent(QEvent* e)
{
    if (theApp()->needWindowFocusFix())
        QApplication::setActiveWindow(this);
    KXmlGuiWindow::enterEvent(e);
}

// vim: et sw=4:
