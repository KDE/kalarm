/*
 *  kalarmresource.h  -  Akonadi resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009,2010 by David Jarvie <djarvie@kde.org>
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

#include <icalresourcebase.h>

class KAlarmMimeTypeVisitor;
class KAEvent;

class KAlarmResource : public ICalResourceBase
{
        Q_OBJECT
    public:
        KAlarmResource(const QString& id);
        virtual ~KAlarmResource();
        static QString mimeType(const KAEvent*);

    protected Q_SLOTS:
        void doRetrieveItems(const Akonadi::Collection&);
        bool doRetrieveItem(const Akonadi::Item&, const QSet<QByteArray>& parts);

    protected:
        virtual void itemAdded(const Akonadi::Item&, const Akonadi::Collection&);
        virtual void itemChanged(const Akonadi::Item&, const QSet<QByteArray>& parts);

    private:
        KAlarmMimeTypeVisitor* mMimeVisitor;
};

#endif

// vim: et sw=4:
