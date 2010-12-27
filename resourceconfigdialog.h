/*
 *  resourceconfigdialog.h  -  KAlarm resource configuration dialog
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <djarvie@kde.org>
 *  Based on configdialog.cpp in kdelibs/kresources,
 *  Copyright (c) 2002 Tobias Koenig <tokoe@kde.org>
 *  Copyright (c) 2002 Jan-Pascal van Best <janpascal@vanbest.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef RESOURCECONFIGDIALOG_H
#define RESOURCECONFIGDIALOG_H

#include <kdialog.h>

class KLineEdit;
class QCheckBox;

namespace KRES {
    class ConfigWidget;
}
class AlarmResource;

class ResourceConfigDialog : public KDialog
{
        Q_OBJECT
    public:
        // Resource=0: create new resource
        ResourceConfigDialog(QWidget* parent, AlarmResource* resource);

        void setInEditMode(bool value);

    protected Q_SLOTS:
        void accept();
        void setReadOnly(bool value);
        void slotNameChanged(const QString& text);

    private:
        KRES::ConfigWidget* mConfigWidget;
        AlarmResource*      mResource;
        KLineEdit*          mName;
        QCheckBox*          mReadOnly;
};

#endif

// vim: et sw=4:
