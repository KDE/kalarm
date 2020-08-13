/*
 *  autostart.h - autostart KAlarm when session restoration is complete
 *  Program:  kalarmautostart
 *  SPDX-FileCopyrightText: 2001, 2008, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AUTOSTART_H
#define AUTOSTART_H

#include <QCoreApplication>

class AutostartApp : public QCoreApplication
{
        Q_OBJECT
    public:
        AutostartApp(int& argc, char** argv);
        ~AutostartApp()  {}
        void setCommandLine(const QString& exe, const QStringList& args);

    private Q_SLOTS:
        void slotAutostart();

    private:
        QString     mExecutable;
        QStringList mArgs;
};

#endif // AUTOSTART_H

// vim: et sw=4:
