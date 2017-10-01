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

#ifndef KALARMCAL_UTILS_H_
#define KALARMCAL_UTILS_H_

#include <KDateTime>
#include <QDateTime>
#include <QTimeZone>

#include "kalarmcal_export.h"

namespace KAlarmCal {

/** Get the KDateTime::Spec for a QDateTime */
KALARMCAL_EXPORT KDateTime::Spec kTimeSpec(const QDateTime &dt);

/** Convert a QTimeZone to a KDateTime::Spec */
KALARMCAL_EXPORT KDateTime::Spec zoneToSpec(const QTimeZone &zone);

/** Convert a QTimeZone to a KDateTime::Spec */
KALARMCAL_EXPORT QTimeZone specToZone(const KDateTime::Spec &spec);

/** Convert KDateTime to QDateTime, correctly preserving timespec */
KALARMCAL_EXPORT QDateTime k2q(const KDateTime &kdt);

/** Convert QDateTime to KDateTime, correctly preserving timespec */
KALARMCAL_EXPORT KDateTime q2k(const QDateTime &qdt, bool isAllDay = false);

}

#endif
