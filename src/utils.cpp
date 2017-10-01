/*
  This file is part of the kalarmcal library.

  Copyright (c) 2017  Daniel Vrátil <dvratil@kde.org>
  Copyright (c) 2017  David Jarvie <djarvie@kde.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "utils.h"

#include <KTimeZone>
#include <KSystemTimeZones>
#include <QDebug>

KDateTime::Spec KAlarmCal::kTimeSpec(const QDateTime &dt)
{
    if (dt.isValid()) {
        switch (dt.timeSpec()) {
            case Qt::LocalTime:
                return KDateTime::LocalZone;
            case Qt::UTC:
                return KDateTime::UTC;
            case Qt::OffsetFromUTC:
                return KDateTime::Spec(KDateTime::OffsetFromUTC, dt.offsetFromUtc());
            case Qt::TimeZone:
                return KSystemTimeZones::zone(QString::fromLatin1(dt.timeZone().id()));
            default:
                break;
        }
    }
    return KDateTime::Invalid;
}

KDateTime::Spec KAlarmCal::zoneToSpec(const QTimeZone& zone)
{
    if (!zone.isValid())
        return KDateTime::Invalid;
    if (zone == QTimeZone::utc())
        return KDateTime::UTC;
    if (zone == QTimeZone::systemTimeZone())
        return KDateTime::LocalZone;
    if (zone.id().startsWith("UTC")) {
        return KDateTime::Spec(KDateTime::OffsetFromUTC, zone.offsetFromUtc(QDateTime::currentDateTimeUtc()));
    } else {
        return KSystemTimeZones::zone(QString::fromLatin1(zone.id()));
    }
}

namespace {

QTimeZone resolveCustomTZ(const KTimeZone &ktz)
{
    // First, let's try Microsoft
    const auto msIana = QTimeZone::windowsIdToDefaultIanaId(ktz.name().toUtf8());
    if (!msIana.isEmpty()) {
        return QTimeZone(msIana);
    }

    int standardUtcOffset = 0;
    bool matched = false;
    const auto phases = ktz.phases();
    for (const auto &phase : phases) {
        if (!phase.isDst()) {
            standardUtcOffset = phase.utcOffset();
            matched = true;
            break;
        }
    }
    if (!matched) {
        standardUtcOffset = ktz.currentOffset(Qt::UTC);
    }

    const auto candidates = QTimeZone::availableTimeZoneIds(standardUtcOffset);
    QMap<int, QTimeZone> matchedCandidates;
    for (const auto &tzid : candidates) {
        const QTimeZone candidate(tzid);
        // This would be a fallback
        if (candidate.hasTransitions() != ktz.hasTransitions()) {
            matchedCandidates.insert(0, candidate);
            continue;
        }

        // Without transitions, we can't do any more precise matching, so just
        // accept this candidate and be done with it
        if (!candidate.hasTransitions() && !ktz.hasTransitions()) {
            return candidate;
        }

        // Calculate how many transitions this candidate shares with the ktz.
        // The candidate with the most matching transitions will win.
        const auto transitions = ktz.transitions(QDateTime(), QDateTime::currentDateTimeUtc());
        int matchedTransitions = 0;
        for (auto it = transitions.rbegin(), end = transitions.rend(); it != end; ++it) {
            const auto &transition = *it;
            const QTimeZone::OffsetDataList candidateTransitions = candidate.transitions(transition.time(), transition.time());
            if (candidateTransitions.isEmpty()) {
                continue;
            }
            const auto candidateTransition = candidateTransitions[0];
            const auto abvs = transition.phase().abbreviations();
            for (const auto &abv : abvs) {
                if (candidateTransition.abbreviation == QString::fromUtf8(abv)) {
                    ++matchedTransitions;
                    break;
                }
            }
        }
        matchedCandidates.insert(matchedTransitions, candidate); 
    }

    if (!matchedCandidates.isEmpty()) {
        return matchedCandidates.value(matchedCandidates.lastKey());
    }

    return {};
}

}

QTimeZone KAlarmCal::specToZone(const KDateTime::Spec &spec)
{
    switch (spec.type()) {
        case KDateTime::Invalid:
            return QTimeZone();
        case KDateTime::LocalZone:
        case KDateTime::ClockTime:
            return QTimeZone::systemTimeZone();
        case KDateTime::UTC:
            return QTimeZone::utc();
        default: {
            auto tz = QTimeZone(spec.timeZone().name().toUtf8());
            if (!tz.isValid()) {
                tz = resolveCustomTZ(spec.timeZone());
                qDebug() << "Resolved" << spec.timeZone().name() << "to" << tz.id();
            }
            return tz;
        }
    }

    return QTimeZone::systemTimeZone();
}

QDateTime KAlarmCal::k2q(const KDateTime &kdt)
{
    if (kdt.isValid()) {
        switch (kdt.timeType()) {
            case KDateTime::LocalZone:
            case KDateTime::ClockTime:
                return QDateTime(kdt.date(), kdt.time(), Qt::LocalTime);
            case KDateTime::UTC:
                return QDateTime(kdt.date(), kdt.time(), Qt::UTC);
            case KDateTime::OffsetFromUTC:
                return QDateTime(kdt.date(), kdt.time(), Qt::OffsetFromUTC, kdt.timeSpec().utcOffset());
            case KDateTime::TimeZone: {
                auto tz = QTimeZone(kdt.timeZone().name().toUtf8());
                if (!tz.isValid()) {
                    tz = resolveCustomTZ(kdt.timeZone());
                    qDebug() << "Resolved" << kdt.timeZone().name() << "to" << tz.id();
                }
                return QDateTime(kdt.date(), kdt.time(), tz);
            }
            case KDateTime::Invalid:
            default:
                break;
        }
    }
    return QDateTime();
}

KDateTime KAlarmCal::q2k(const QDateTime &qdt, bool allDay)
{
    if (qdt.isValid()) {
        KDateTime kdt(qdt.date(), qdt.time(), kTimeSpec(qdt));
        kdt.setDateOnly(allDay && qdt.time() == QTime(0, 0, 0));
        return kdt;
    }
    return KDateTime();
}
