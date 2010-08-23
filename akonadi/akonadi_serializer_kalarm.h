/*
 *  akonadi_serializer_kalarm.h  -  Akonadi resource serializer for KAlarm
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

#ifndef AKONADI_SERIALIZER_KALARM_H
#define AKONADI_SERIALIZER_KALARM_H

#include <QtCore/QObject>

#include <akonadi/itemserializerplugin.h>
#include <kcalcore/icalformat.h>

class KAEvent;

class SerializerPluginKAlarm : public QObject, public Akonadi::ItemSerializerPlugin
{
        Q_OBJECT
        Q_INTERFACES(Akonadi::ItemSerializerPlugin)

    public:
        bool deserialize(Akonadi::Item& item, const QByteArray& label, QIODevice& data, int version);
        void serialize(const Akonadi::Item& item, const QByteArray& label, QIODevice& data, int& version);

    private:
        KCalCore::ICalFormat mFormat;
};

#endif // AKONADI_SERIALIZER_KALARM_H

// vim: et sw=4:
