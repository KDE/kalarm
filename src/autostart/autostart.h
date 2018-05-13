/*
 *  autostart.h - autostart KAlarm when session restoration is complete
 *  Program:  kalarmautostart
 *  Copyright © 2001,2008,2018 by David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
