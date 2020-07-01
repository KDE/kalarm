/*
 *  akonadi_serializer_kalarm.h  -  Akonadi resource serializer for KAlarm
 *  SPDX-FileCopyrightText: 2009-2014 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef AKONADI_SERIALIZER_KALARM_H
#define AKONADI_SERIALIZER_KALARM_H

#include "kaeventformatter.h"

#include <AkonadiCore/itemserializerplugin.h>
#include <AkonadiCore/differencesalgorithminterface.h>
#include <AkonadiCore/gidextractorinterface.h>
#include <KCalendarCore/ICalFormat>

#include <QObject>

namespace Akonadi {
class Item;
class AbstractDifferencesReporter;
}

class SerializerPluginKAlarm : public QObject, public Akonadi::ItemSerializerPlugin, public Akonadi::DifferencesAlgorithmInterface, public Akonadi::GidExtractorInterface
{
    Q_OBJECT
    Q_INTERFACES(Akonadi::ItemSerializerPlugin)
    Q_INTERFACES(Akonadi::DifferencesAlgorithmInterface)
    Q_INTERFACES(Akonadi::GidExtractorInterface)
    Q_PLUGIN_METADATA(IID "org.kde.akonadi.SerializerPluginKAlarm")
public:
    bool deserialize(Akonadi::Item &item, const QByteArray &label, QIODevice &data, int version) override;
    void serialize(const Akonadi::Item &item, const QByteArray &label, QIODevice &data, int &version) override;
    void compare(Akonadi::AbstractDifferencesReporter *, const Akonadi::Item &left, const Akonadi::Item &right) override;
    QString extractGid(const Akonadi::Item &item) const override;

private:
    void reportDifference(Akonadi::AbstractDifferencesReporter *, KAEventFormatter::Parameter);

    KCalendarCore::ICalFormat mFormat;
    KAEventFormatter mValueL;
    KAEventFormatter mValueR;
    QString mRegistered;
};

#endif // AKONADI_SERIALIZER_KALARM_H
