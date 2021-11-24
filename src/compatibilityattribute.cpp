/*
 *  compatibilityattribute.cpp  -  Akonadi attribute holding Collection compatibility
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2011-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "compatibilityattribute.h"

#include "kalarmcal_debug.h"

namespace KAlarmCal
{

class Q_DECL_HIDDEN CompatibilityAttribute::Private
{
public:
  Private() = default;
  bool operator==(KAlarmCal::CompatibilityAttribute::Private other) const {
    return mCompatibility == other.mCompatibility && mVersion == other.mVersion;
    }

    KACalendar::Compat mCompatibility{KACalendar::Incompatible};  // calendar compatibility with current KAlarm format
    int                mVersion{KACalendar::IncompatibleFormat};  // KAlarm calendar format version
};

CompatibilityAttribute::CompatibilityAttribute()
    : d(new Private)
{
}

CompatibilityAttribute::CompatibilityAttribute(const CompatibilityAttribute &rhs)
    : Akonadi::Attribute(rhs),
      d(new Private(*rhs.d))
{
}

CompatibilityAttribute::~CompatibilityAttribute()
{
    delete d;
}

CompatibilityAttribute &CompatibilityAttribute::operator=(const CompatibilityAttribute &other)
{
    if (&other != this) {
        Attribute::operator=(other);
        *d = *other.d;
    }
    return *this;
}

bool CompatibilityAttribute::operator==(const CompatibilityAttribute &other) const
{
    return *d == *other.d;
}

CompatibilityAttribute *CompatibilityAttribute::clone() const
{
    return new CompatibilityAttribute(*this);
}

KACalendar::Compat CompatibilityAttribute::compatibility() const
{
    return d->mCompatibility;
}

void CompatibilityAttribute::setCompatibility(KACalendar::Compat c)
{
    d->mCompatibility = c;
}

int CompatibilityAttribute::version() const
{
    return d->mVersion;
}

void CompatibilityAttribute::setVersion(int v)
{
    d->mVersion = v;
}

QByteArray CompatibilityAttribute::type() const
{
    return name();
}

QByteArray CompatibilityAttribute::name()
{
    static const QByteArray attType("KAlarmCompatibility");
    return attType;
}

QByteArray CompatibilityAttribute::serialized() const
{
    const QByteArray v = QByteArray::number(d->mCompatibility) + ' '
                         + QByteArray::number(d->mVersion);
    qCDebug(KALARMCAL_LOG) << v;
    return v;
}

void CompatibilityAttribute::deserialize(const QByteArray &data)
{
    qCDebug(KALARMCAL_LOG) << data;

    // Set default values
    d->mCompatibility = KACalendar::Incompatible;
    d->mVersion       = KACalendar::IncompatibleFormat;

    bool ok;
    const QList<QByteArray> items = data.simplified().split(' ');
    const int count = items.count();
    int index = 0;
    if (count > index) {
        // 0: calendar format compatibility
        const int c = items[index++].toInt(&ok);
        const KACalendar::Compat AllCompat(KACalendar::Current | KACalendar::Converted | KACalendar::Convertible | KACalendar::Incompatible | KACalendar::Unknown);
        if (!ok  || (c & static_cast<int>(AllCompat)) != c) {
            qCritical() << "Invalid compatibility:" << c;
            return;
        }
        d->mCompatibility = static_cast<KACalendar::Compat>(c);
    }
    if (count > index) {
        // 1: KAlarm calendar version number
        const int c = items[index++].toInt(&ok);
        if (!ok) {
            qCritical() << "Invalid version:" << c;
            return;
        }
        d->mVersion = c;
    }
}

} // namespace KAlarmCal

// vim: et sw=4:
