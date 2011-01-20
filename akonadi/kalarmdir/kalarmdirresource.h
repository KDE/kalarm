/*
 *  kalarmdirresource.h  -  Akonadi directory resource for KAlarm
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

#ifndef KALARMDIRRESOURCE_H
#define KALARMDIRRESOURCE_H

#include "kacalendar.h"
#include "kaevent.h"
#include <akonadi/resourcebase.h>

namespace Akonadi_KAlarm_Dir_Resource { class Settings; }

class KAlarmDirResource : public Akonadi::ResourceBase, public Akonadi::AgentBase::Observer
{
        Q_OBJECT
    public:
        KAlarmDirResource(const QString& id);
        ~KAlarmDirResource();

    public Q_SLOTS:
        virtual void configure(WId windowId);
        virtual void aboutToQuit();

    protected Q_SLOTS:
        void retrieveCollections();
        void retrieveItems(const Akonadi::Collection&);
        bool retrieveItem(const Akonadi::Item&, const QSet<QByteArray>& parts);

    protected:
        virtual void collectionChanged(const Akonadi::Collection&);
        virtual void itemAdded(const Akonadi::Item&, const Akonadi::Collection&);
        virtual void itemChanged(const Akonadi::Item&, const QSet<QByteArray>& parts);
        virtual void itemRemoved(const Akonadi::Item&);

    private Q_SLOTS:
        void settingsChanged();

    private:
        bool loadFiles();
        QString directoryName() const;
        QString directoryFileName(const QString& file) const;
        void initializeDirectory() const;
        bool cancelIfReadOnly();
        bool writeToFile(const KAEvent&);
        void setCompatibility(bool writeAttr = true);

        QMap<QString, KAEvent> mEvents;    // cached alarms, indexed by ID
        Akonadi_KAlarm_Dir_Resource::Settings* mSettings;
        KAlarm::CalEvent::Types  mAlarmTypes;
        KAlarm::Calendar::Compat mCompatibility;
};

#endif

// vim: et sw=4:
