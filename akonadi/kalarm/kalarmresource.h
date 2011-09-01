/*
 *  kalarmresource.h  -  Akonadi resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef KALARMRESOURCE_H
#define KALARMRESOURCE_H

#include "kacalendar.h"
#include <icalresourcebase.h>

class KJob;
class KAEvent;
class AlarmTypeRadioWidget;

class KAlarmResource : public ICalResourceBase
{
        Q_OBJECT
    public:
        KAlarmResource(const QString& id);
        virtual ~KAlarmResource();

    protected:
        /**
         * Customize the configuration dialog before it is displayed.
         */
        virtual void customizeConfigDialog(Akonadi::SingleFileResourceConfigDialog<Akonadi_KAlarm_Resource::Settings>*);
        virtual void configDialogAcceptedActions(Akonadi::SingleFileResourceConfigDialog<Akonadi_KAlarm_Resource::Settings>*);

        virtual void doRetrieveItems(const Akonadi::Collection&);
        virtual bool doRetrieveItem(const Akonadi::Item&, const QSet<QByteArray>& parts);
        virtual bool readFromFile(const QString& fileName);
        virtual bool writeToFile(const QString& fileName);
        virtual void itemAdded(const Akonadi::Item&, const Akonadi::Collection&);
        virtual void itemChanged(const Akonadi::Item&, const QSet<QByteArray>& parts);

    private Q_SLOTS:
        void settingsChanged();

    private:
        AlarmTypeRadioWidget*    mTypeSelector;
        KAlarm::Calendar::Compat mCompatibility;
        int                      mVersion;   // calendar format version
};

#endif

// vim: et sw=4:
