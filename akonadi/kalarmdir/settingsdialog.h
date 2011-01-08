/*
 *  settingsdialog.h  -  Akonadi KAlarm directory resource configuration dialog
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef KALARMDIR_SETTINGSDIALOG_H
#define KALARMDIR_SETTINGSDIALOG_H

#include "ui_settingsdialog.h"

#include <KDE/KDialog>

class KConfigDialogManager;

namespace Akonadi_KAlarm_Dir_Resource
{

class Settings;

class SettingsDialog : public KDialog
{
        Q_OBJECT
    public:
        explicit SettingsDialog(WId windowId, Settings*);

    private Q_SLOTS:
        void save();
        void validate();

    private:
        Ui::SettingsDialog ui;
        KConfigDialogManager* mManager;
        Akonadi_KAlarm_Dir_Resource::Settings* mSettings;
};

}

#endif // KALARMDIR_SETTINGSDIALOG_H
