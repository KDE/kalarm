/*
 *  kalarmresource.h  -  Akonadi resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009 by David Jarvie <djarvie@kde.org>
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

#ifndef KALARMRESOURCE_H
#define KALARMRESOURCE_H

#include <icalresourcebase.h>

class KAlarmMimeTypeVisitor;
class KAEventData;

class KAlarmResource : public ICalResourceBase
{
        Q_OBJECT
    public:
        KAlarmResource(const QString& id);
        virtual ~KAlarmResource();

    protected Q_SLOTS:
        void doRetrieveItems(const Akonadi::Collection&);
        bool doRetrieveItem(const Akonadi::Item&, const QSet<QByteArray>& parts);

    protected:
        virtual void itemAdded(const Akonadi::Item&, const Akonadi::Collection&);
        virtual void itemChanged(const Akonadi::Item&, const QSet<QByteArray>& parts);

    private:
        static QString mimeType(const KAEventData*);

	KAlarmMimeTypeVisitor* mMimeVisitor;
};

#endif
