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

#include "kaeventformatter.h"

#include <akonadi/itemserializerplugin.h>
#include <akonadi/differencesalgorithminterface.h>
#include <kcalcore/icalformat.h>

#include <QtCore/QObject>

class KAEvent;

namespace Akonadi
{

class AbstractDifferencesReporter;

class SerializerPluginKAlarm : public QObject,
                               public Akonadi::ItemSerializerPlugin,
                               public Akonadi::DifferencesAlgorithmInterface
{
        Q_OBJECT
        Q_INTERFACES(Akonadi::ItemSerializerPlugin)
        Q_INTERFACES(Akonadi::DifferencesAlgorithmInterface)

    public:
        bool deserialize(Akonadi::Item& item, const QByteArray& label, QIODevice& data, int version);
        void serialize(const Akonadi::Item& item, const QByteArray& label, QIODevice& data, int& version);
        void compare(Akonadi::AbstractDifferencesReporter*, const Akonadi::Item& left, const Akonadi::Item& right);

    private:
        void reportDifference(Akonadi::AbstractDifferencesReporter*, KAEventFormatter::Parameter);

        KCalCore::ICalFormat mFormat;
        KAEventFormatter mValueL;
        KAEventFormatter mValueR;
};

}

#endif // AKONADI_SERIALIZER_KALARM_H

// vim: et sw=4:
