/*
   This file is part of kalarmcal library, which provides access to KAlarm
   calendar data.

   SPDX-FileCopyrightText: 2005-2022 David Jarvie <djarvie@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kadatetimetest.h"
#include "kadatetime.h"
#include <cstdlib>
using KAlarmCal::KADateTime;


#include <QTest>
#include <QTimeZone>
#include <QLocale>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

//clazy:excludeall=non-pod-global-static

//TODO: test new methods

QTEST_GUILESS_MAIN(KADateTimeTest)


namespace KAlarmCal
{
extern KALARMCAL_EXPORT int KADateTime_utcCacheHit;
extern KALARMCAL_EXPORT int KADateTime_zoneCacheHit;
}

////////////////////////////////////////////////////////////////////////
// KADateTime::Spec constructors and basic property information methods,
// and the static convenience instances/methods.
////////////////////////////////////////////////////////////////////////

void KADateTimeTest::specConstructors()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");

    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":Europe/London");
    ::tzset();

    // Ensure that local time is different from UTC and different from 'london'
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Default constructor
    KADateTime::Spec invalid;
    QVERIFY(!invalid.isValid());
    QCOMPARE(invalid.type(), KADateTime::Invalid);
    QVERIFY(!invalid.isLocalZone());
    QVERIFY(!invalid.isUtc());
    QVERIFY(!invalid.isOffsetFromUtc());
    QCOMPARE(invalid.utcOffset(), 0);
    QVERIFY(!invalid.timeZone().isValid());

    // Time zone
    KADateTime::Spec tz(london);
    QVERIFY(tz.isValid());
    QCOMPARE(tz.type(), KADateTime::TimeZone);
    QVERIFY(!tz.isUtc());
    QVERIFY(!tz.isOffsetFromUtc());
    QVERIFY(!tz.isLocalZone());
    QCOMPARE(tz.utcOffset(), 0);
    QCOMPARE(tz.timeZone(), london);

    KADateTime::Spec tzLocal(losAngeles);
    QVERIFY(tzLocal.isValid());
    QCOMPARE(tzLocal.type(), KADateTime::TimeZone);
    QVERIFY(!tzLocal.isUtc());
    QVERIFY(!tzLocal.isOffsetFromUtc());
    QVERIFY(!tzLocal.isLocalZone());
    QCOMPARE(tzLocal.utcOffset(), 0);
    QCOMPARE(tzLocal.timeZone(), losAngeles);

    // ... copy constructor
    KADateTime::Spec tzCopy(tz);
    QVERIFY(tzCopy.isValid());
    QCOMPARE(tzCopy.type(), KADateTime::TimeZone);
    QVERIFY(!tzCopy.isUtc());
    QVERIFY(!tzCopy.isOffsetFromUtc());
    QVERIFY(!tzCopy.isLocalZone());
    QCOMPARE(tzCopy.utcOffset(), 0);
    QCOMPARE(tzCopy.timeZone(), london);

    // Local time zone
    KADateTime::Spec local(KADateTime::LocalZone);
    QVERIFY(local.isValid());
    QCOMPARE(local.type(), KADateTime::LocalZone);
    QCOMPARE(local, KADateTime::Spec(KADateTime::LocalZone));
    QVERIFY(!local.isUtc());
    QVERIFY(!local.isOffsetFromUtc());
    QVERIFY(local.isLocalZone());
    QCOMPARE(local.utcOffset(), 0);
    QCOMPARE(local.timeZone(), QTimeZone::systemTimeZone());

    KADateTime::Spec localx(KADateTime::Spec(KADateTime::LocalZone, 2 * 3600));
    QVERIFY(localx.isValid());
    QCOMPARE(localx.type(), KADateTime::LocalZone);
    QCOMPARE(localx, KADateTime::Spec(KADateTime::LocalZone));
    QVERIFY(!localx.isUtc());
    QVERIFY(!localx.isOffsetFromUtc());
    QVERIFY(localx.isLocalZone());
    QCOMPARE(localx.utcOffset(), 0);
    QCOMPARE(localx.timeZone(), QTimeZone::systemTimeZone());

    KADateTime::Spec local2 = KADateTime::Spec::LocalZone();
    QVERIFY(local2.isValid());
    QCOMPARE(local2.type(), KADateTime::LocalZone);
    QCOMPARE(local2, KADateTime::Spec(KADateTime::LocalZone));
    QVERIFY(!local2.isUtc());
    QVERIFY(!local2.isOffsetFromUtc());
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.utcOffset(), 0);
    QCOMPARE(local2.timeZone(), QTimeZone::systemTimeZone());

    // ... copy constructor
    KADateTime::Spec localCopy(local);
    QVERIFY(localCopy.isValid());
    QCOMPARE(localCopy.type(), KADateTime::LocalZone);
    QCOMPARE(localCopy, KADateTime::Spec(KADateTime::LocalZone));
    QVERIFY(!localCopy.isUtc());
    QVERIFY(!localCopy.isOffsetFromUtc());
    QVERIFY(localCopy.isLocalZone());
    QCOMPARE(localCopy.utcOffset(), 0);
    QCOMPARE(localCopy.timeZone(), losAngeles);

    // UTC
    KADateTime::Spec utc(KADateTime::UTC);
    QVERIFY(utc.isValid());
    QCOMPARE(utc.type(), KADateTime::UTC);
    QVERIFY(utc.isUtc());
    QVERIFY(!utc.isOffsetFromUtc());
    QVERIFY(!utc.isLocalZone());
    QCOMPARE(utc.utcOffset(), 0);
    QCOMPARE(utc.timeZone(), QTimeZone::utc());

    KADateTime::Spec utcx(KADateTime::UTC, 2 * 3600);
    QVERIFY(utcx.isValid());
    QCOMPARE(utcx.type(), KADateTime::UTC);
    QVERIFY(utcx.isUtc());
    QVERIFY(!utcx.isOffsetFromUtc());
    QVERIFY(!utcx.isLocalZone());
    QCOMPARE(utcx.utcOffset(), 0);
    QCOMPARE(utcx.timeZone(), QTimeZone::utc());

    const KADateTime::Spec &utc2 = KADateTime::Spec::UTC();
    QVERIFY(utc2.isValid());
    QCOMPARE(utc2.type(), KADateTime::UTC);
    QVERIFY(utc2.isUtc());
    QVERIFY(!utc2.isOffsetFromUtc());
    QVERIFY(!utc2.isLocalZone());
    QCOMPARE(utc2.utcOffset(), 0);
    QCOMPARE(utc2.timeZone(), QTimeZone::utc());

    // ... copy constructor
    KADateTime::Spec utcCopy(utc);
    QVERIFY(utcCopy.isValid());
    QCOMPARE(utcCopy.type(), KADateTime::UTC);
    QVERIFY(utcCopy.isUtc());
    QVERIFY(!utcCopy.isOffsetFromUtc());
    QVERIFY(!utcCopy.isLocalZone());
    QCOMPARE(utcCopy.utcOffset(), 0);
    QCOMPARE(utcCopy.timeZone(), QTimeZone::utc());

    // Offset from UTC
    KADateTime::Spec offset0(KADateTime::OffsetFromUTC);
    QVERIFY(offset0.isValid());
    QCOMPARE(offset0.type(), KADateTime::OffsetFromUTC);
    QVERIFY(offset0.isUtc());
    QVERIFY(offset0.isOffsetFromUtc());
    QVERIFY(!offset0.isLocalZone());
    QCOMPARE(offset0.utcOffset(), 0);
    QVERIFY(!offset0.timeZone().isValid());

    KADateTime::Spec offset(KADateTime::Spec(KADateTime::OffsetFromUTC, -2 * 3600));
    QVERIFY(offset.isValid());
    QCOMPARE(offset.type(), KADateTime::OffsetFromUTC);
    QVERIFY(!offset.isUtc());
    QVERIFY(offset.isOffsetFromUtc());
    QVERIFY(!offset.isLocalZone());
    QCOMPARE(offset.utcOffset(), -2 * 3600);
    QVERIFY(!offset.timeZone().isValid());

    KADateTime::Spec offset2 = KADateTime::Spec::OffsetFromUTC(2 * 3600);
    QVERIFY(offset2.isValid());
    QCOMPARE(offset2.type(), KADateTime::OffsetFromUTC);
    QVERIFY(!offset2.isUtc());
    QVERIFY(offset2.isOffsetFromUtc());
    QVERIFY(!offset2.isLocalZone());
    QCOMPARE(offset2.utcOffset(), 2 * 3600);
    QVERIFY(!offset2.timeZone().isValid());

    // ... copy constructor
    KADateTime::Spec offsetCopy(offset);
    QVERIFY(offsetCopy.isValid());
    QCOMPARE(offsetCopy.type(), KADateTime::OffsetFromUTC);
    QVERIFY(!offsetCopy.isUtc());
    QVERIFY(offsetCopy.isOffsetFromUtc());
    QVERIFY(!offsetCopy.isLocalZone());
    QCOMPARE(offsetCopy.utcOffset(), -2 * 3600);
    QVERIFY(!offsetCopy.timeZone().isValid());

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

////////////////////////////////////////////////////////////////////////
// KADateTime::Spec setType(), operator==(), etc.
////////////////////////////////////////////////////////////////////////

void KADateTimeTest::specSet()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    KADateTime::Spec spec;
    QCOMPARE(spec.type(), KADateTime::Invalid);

    spec.setType(KADateTime::OffsetFromUTC, 7200);
    QCOMPARE(spec.type(), KADateTime::OffsetFromUTC);
    QVERIFY(spec.equivalentTo(KADateTime::Spec::OffsetFromUTC(7200)));
    QVERIFY(!spec.equivalentTo(KADateTime::Spec::OffsetFromUTC(0)));
    QVERIFY(spec == KADateTime::Spec::OffsetFromUTC(7200));
    QVERIFY(!(spec != KADateTime::Spec::OffsetFromUTC(7200)));
    QVERIFY(spec != KADateTime::Spec::OffsetFromUTC(-7200));
    QVERIFY(spec != KADateTime::Spec(london));

    spec.setType(KADateTime::OffsetFromUTC, 0);
    QCOMPARE(spec.type(), KADateTime::OffsetFromUTC);
    QVERIFY(spec.equivalentTo(KADateTime::Spec::OffsetFromUTC(0)));
    QVERIFY(spec.equivalentTo(KADateTime::Spec::UTC()));
    QVERIFY(!spec.equivalentTo(KADateTime::Spec::OffsetFromUTC(7200)));
    QVERIFY(spec == KADateTime::Spec::OffsetFromUTC(0));
    QVERIFY(!(spec != KADateTime::Spec::OffsetFromUTC(0)));
    QVERIFY(spec != KADateTime::Spec::OffsetFromUTC(-7200));
    QVERIFY(spec != KADateTime::Spec(london));

    spec.setType(london);
    QCOMPARE(spec.type(), KADateTime::TimeZone);
    QVERIFY(spec.equivalentTo(KADateTime::Spec(london)));
    QVERIFY(spec == KADateTime::Spec(london));
    QVERIFY(!(spec != KADateTime::Spec(london)));
    QVERIFY(spec != KADateTime::Spec::OffsetFromUTC(0));
    QVERIFY(!spec.equivalentTo(KADateTime::Spec::OffsetFromUTC(0)));

    spec.setType(KADateTime::LocalZone);
    QCOMPARE(spec.type(), KADateTime::LocalZone);
    QVERIFY(spec.equivalentTo(KADateTime::Spec::LocalZone()));
    QVERIFY(spec == KADateTime::Spec::LocalZone());
    QVERIFY(!(spec != KADateTime::Spec::LocalZone()));
    QVERIFY(spec.equivalentTo(KADateTime::Spec(losAngeles)));
    QVERIFY(spec != KADateTime::Spec(losAngeles));
    QVERIFY(spec != KADateTime::Spec(london));
    QVERIFY(!spec.equivalentTo(KADateTime::Spec(london)));

    spec.setType(KADateTime::UTC);
    QCOMPARE(spec.type(), KADateTime::UTC);
    QVERIFY(spec.equivalentTo(KADateTime::Spec::UTC()));
    QVERIFY(spec == KADateTime::Spec::UTC());
    QVERIFY(!(spec != KADateTime::Spec::UTC()));
    QVERIFY(spec != KADateTime::Spec::LocalZone());
    QVERIFY(!spec.equivalentTo(KADateTime::Spec::LocalZone()));
    QVERIFY(spec.equivalentTo(KADateTime::Spec::OffsetFromUTC(0)));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

//////////////////////////////////////////////////////
// Constructors and basic property information methods
//////////////////////////////////////////////////////

void KADateTimeTest::constructors()
{
    QDate d(2001, 2, 13);
    QTime t(3, 45, 14);
    QDateTime dtLocal(d, t, Qt::LocalTime);
    QDateTime dtUTC(d, t, Qt::UTC);
    QTimeZone london("Europe/London");

    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":Europe/London");
    ::tzset();
    QDateTime dtUTCtoLondon = dtUTC.toLocalTime();

    // Ensure that local time is different from UTC and different from 'london'
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Default constructor
    KADateTime deflt;
    QVERIFY(deflt.isNull());
    QVERIFY(!deflt.isValid());

    // No time zone or timeSpec explicitly specified
    KADateTime datetimeL(dtLocal);
    QVERIFY(!datetimeL.isNull());
    QVERIFY(datetimeL.isValid());
    QVERIFY(!datetimeL.isDateOnly());
    QCOMPARE(datetimeL.timeType(), KADateTime::LocalZone);
    QCOMPARE(datetimeL.timeSpec(), KADateTime::Spec::LocalZone());
    QVERIFY(datetimeL.isLocalZone());
    QVERIFY(!datetimeL.isUtc());
    QVERIFY(!datetimeL.isOffsetFromUtc());
    QCOMPARE(datetimeL.utcOffset(), -8 * 3600);
    QCOMPARE(datetimeL.timeZone(), QTimeZone::systemTimeZone());
    QCOMPARE(datetimeL.date(), dtLocal.date());
    QCOMPARE(datetimeL.time(), dtLocal.time());
    QCOMPARE(datetimeL.qDateTime(), dtLocal);

    KADateTime datetimeU(dtUTC);
    QVERIFY(!datetimeU.isNull());
    QVERIFY(datetimeU.isValid());
    QVERIFY(!datetimeU.isDateOnly());
    QCOMPARE(datetimeU.timeType(), KADateTime::UTC);
    QVERIFY(!datetimeU.isLocalZone());
    QVERIFY(datetimeU.isUtc());
    QVERIFY(!datetimeU.isOffsetFromUtc());
    QCOMPARE(datetimeU.utcOffset(), 0);
    QCOMPARE(datetimeU.timeZone(), QTimeZone::utc());
    QCOMPARE(datetimeU.date(), dtUTC.date());
    QCOMPARE(datetimeU.time(), dtUTC.time());
    QCOMPARE(datetimeU.qDateTime(), dtUTC);

    // Time zone
    KADateTime dateTz(d, london);
    QVERIFY(!dateTz.isNull());
    QVERIFY(dateTz.isValid());
    QVERIFY(dateTz.isDateOnly());
    QCOMPARE(dateTz.timeType(), KADateTime::TimeZone);
    QVERIFY(!dateTz.isUtc());
    QVERIFY(!dateTz.isOffsetFromUtc());
    QVERIFY(!dateTz.isLocalZone());
    QCOMPARE(dateTz.utcOffset(), 0);
    QCOMPARE(dateTz.timeZone(), london);
    QCOMPARE(dateTz.date(), d);
    QCOMPARE(dateTz.time(), QTime(0, 0, 0));
    QCOMPARE(dateTz.qDateTime(), QDateTime(d, QTime(0, 0, 0), london));

    KADateTime dateTimeTz(d, QTime(3, 45, 14), london);
    QVERIFY(!dateTimeTz.isNull());
    QVERIFY(dateTimeTz.isValid());
    QVERIFY(!dateTimeTz.isDateOnly());
    QCOMPARE(dateTimeTz.timeType(), KADateTime::TimeZone);
    QVERIFY(!dateTimeTz.isUtc());
    QVERIFY(!dateTimeTz.isOffsetFromUtc());
    QVERIFY(!dateTimeTz.isLocalZone());
    QCOMPARE(dateTimeTz.utcOffset(), 0);
    QCOMPARE(dateTimeTz.timeZone(), london);
    QCOMPARE(dateTimeTz.date(), d);
    QCOMPARE(dateTimeTz.time(), QTime(3, 45, 14));
    QCOMPARE(dateTimeTz.qDateTime(), QDateTime(d, QTime(3, 45, 14), london));

    KADateTime datetimeTz(dtLocal, london);
    QVERIFY(!datetimeTz.isNull());
    QVERIFY(datetimeTz.isValid());
    QVERIFY(!dateTimeTz.isDateOnly());
    QCOMPARE(datetimeTz.timeType(), KADateTime::TimeZone);
    QVERIFY(!datetimeTz.isUtc());
    QVERIFY(!datetimeTz.isOffsetFromUtc());
    QVERIFY(!datetimeTz.isLocalZone());
    QCOMPARE(datetimeTz.utcOffset(), 0);
    QCOMPARE(datetimeTz.timeZone(), london);
    QCOMPARE(datetimeTz.date(), dtLocal.date());
    QCOMPARE(datetimeTz.time(), QTime(11, 45, 14));
    QCOMPARE(datetimeTz.qDateTime(), dtLocal.toTimeZone(london));

    KADateTime datetimeTz2(dtUTC, london);
    QVERIFY(!datetimeTz2.isNull());
    QVERIFY(datetimeTz2.isValid());
    QVERIFY(!dateTimeTz.isDateOnly());
    QCOMPARE(datetimeTz2.timeType(), KADateTime::TimeZone);
    QVERIFY(!datetimeTz2.isUtc());
    QVERIFY(!datetimeTz2.isOffsetFromUtc());
    QVERIFY(!datetimeTz2.isLocalZone());
    QCOMPARE(datetimeTz2.utcOffset(), 0);
    QCOMPARE(datetimeTz2.timeZone(), london);
    QCOMPARE(datetimeTz2.date(), dtUTCtoLondon.date());
    QCOMPARE(datetimeTz2.time(), dtUTCtoLondon.time());
    QCOMPARE(datetimeTz2.qDateTime(), dtUTC);

    // ... copy constructor
    KADateTime datetimeTzCopy(datetimeTz);
    QVERIFY(!datetimeTzCopy.isNull());
    QVERIFY(datetimeTzCopy.isValid());
    QVERIFY(!dateTimeTz.isDateOnly());
    QCOMPARE(datetimeTzCopy.timeType(), KADateTime::TimeZone);
    QVERIFY(!datetimeTzCopy.isUtc());
    QVERIFY(!datetimeTzCopy.isOffsetFromUtc());
    QVERIFY(!datetimeTzCopy.isLocalZone());
    QCOMPARE(datetimeTzCopy.utcOffset(), 0);
    QCOMPARE(datetimeTzCopy.timeZone(), datetimeTz.timeZone());
    QCOMPARE(datetimeTzCopy.date(), datetimeTz.date());
    QCOMPARE(datetimeTzCopy.time(), datetimeTz.time());
    QCOMPARE(datetimeTzCopy.qDateTime(), datetimeTz.qDateTime());

    // UTC
    KADateTime date_UTC(d, KADateTime::Spec::UTC());
    QVERIFY(!date_UTC.isNull());
    QVERIFY(date_UTC.isValid());
    QVERIFY(date_UTC.isDateOnly());
    QCOMPARE(date_UTC.timeType(), KADateTime::UTC);
    QVERIFY(date_UTC.isUtc());
    QVERIFY(!date_UTC.isOffsetFromUtc());
    QVERIFY(!date_UTC.isLocalZone());
    QCOMPARE(date_UTC.utcOffset(), 0);
    QCOMPARE(date_UTC.timeZone(), QTimeZone::utc());
    QCOMPARE(date_UTC.date(), d);
    QCOMPARE(date_UTC.time(), QTime(0, 0, 0));
    QCOMPARE(date_UTC.qDateTime(), QDateTime(d, QTime(0, 0, 0), Qt::UTC));

    KADateTime dateTime_UTC(d, t, KADateTime::UTC);
    QVERIFY(!dateTime_UTC.isNull());
    QVERIFY(dateTime_UTC.isValid());
    QVERIFY(!dateTime_UTC.isDateOnly());
    QCOMPARE(dateTime_UTC.timeType(), KADateTime::UTC);
    QVERIFY(dateTime_UTC.isUtc());
    QVERIFY(!dateTime_UTC.isOffsetFromUtc());
    QVERIFY(!dateTime_UTC.isLocalZone());
    QCOMPARE(dateTime_UTC.utcOffset(), 0);
    QCOMPARE(dateTime_UTC.timeZone(), QTimeZone::utc());
    QCOMPARE(dateTime_UTC.date(), d);
    QCOMPARE(dateTime_UTC.time(), t);
    QCOMPARE(dateTime_UTC.qDateTime(), QDateTime(d, t, Qt::UTC));

    KADateTime datetime_UTC(dtLocal, KADateTime::UTC);
    QVERIFY(!datetime_UTC.isNull());
    QVERIFY(datetime_UTC.isValid());
    QVERIFY(!datetime_UTC.isDateOnly());
    QCOMPARE(datetime_UTC.timeType(), KADateTime::UTC);
    QVERIFY(datetime_UTC.isUtc());
    QVERIFY(!datetime_UTC.isOffsetFromUtc());
    QVERIFY(!datetime_UTC.isLocalZone());
    QCOMPARE(datetime_UTC.utcOffset(), 0);
    QCOMPARE(datetime_UTC.timeZone(), QTimeZone::utc());
    {
        QDateTime utc = dtLocal.toUTC();
        QCOMPARE(datetime_UTC.date(), utc.date());
        QCOMPARE(datetime_UTC.time(), utc.time());
        QCOMPARE(datetime_UTC.qDateTime(), utc);
    }

    KADateTime datetime_UTC2(dtUTC, KADateTime::UTC);
    QVERIFY(!datetime_UTC2.isNull());
    QVERIFY(datetime_UTC2.isValid());
    QVERIFY(!datetime_UTC2.isDateOnly());
    QCOMPARE(datetime_UTC2.timeType(), KADateTime::UTC);
    QVERIFY(datetime_UTC2.isUtc());
    QVERIFY(!datetime_UTC2.isOffsetFromUtc());
    QVERIFY(!datetime_UTC2.isLocalZone());
    QCOMPARE(datetime_UTC2.utcOffset(), 0);
    QCOMPARE(datetime_UTC2.timeZone(), QTimeZone::utc());
    QCOMPARE(datetime_UTC2.date(), dtUTC.date());
    QCOMPARE(datetime_UTC2.time(), dtUTC.time());
    QCOMPARE(datetime_UTC2.qDateTime(), dtUTC);

    // ... copy constructor
    KADateTime datetime_UTCCopy(datetime_UTC);
    QVERIFY(!datetime_UTCCopy.isNull());
    QVERIFY(datetime_UTCCopy.isValid());
    QVERIFY(!datetime_UTCCopy.isDateOnly());
    QCOMPARE(datetime_UTCCopy.timeType(), KADateTime::UTC);
    QVERIFY(datetime_UTCCopy.isUtc());
    QVERIFY(!datetime_UTCCopy.isOffsetFromUtc());
    QVERIFY(!datetime_UTCCopy.isLocalZone());
    QCOMPARE(datetime_UTCCopy.utcOffset(), 0);
    QCOMPARE(datetime_UTCCopy.timeZone(), datetime_UTC.timeZone());
    QCOMPARE(datetime_UTCCopy.date(), datetime_UTC.date());
    QCOMPARE(datetime_UTCCopy.time(), datetime_UTC.time());
    QCOMPARE(datetime_UTCCopy.qDateTime(), datetime_UTC.qDateTime());

    // Offset from UTC
    KADateTime date_OffsetFromUTC(d, KADateTime::Spec::OffsetFromUTC(-2 * 3600));
    QVERIFY(!date_OffsetFromUTC.isNull());
    QVERIFY(date_OffsetFromUTC.isValid());
    QVERIFY(date_OffsetFromUTC.isDateOnly());
    QCOMPARE(date_OffsetFromUTC.timeType(), KADateTime::OffsetFromUTC);
    QVERIFY(!date_OffsetFromUTC.isUtc());
    QVERIFY(date_OffsetFromUTC.isOffsetFromUtc());
    QVERIFY(!date_OffsetFromUTC.isLocalZone());
    QCOMPARE(date_OffsetFromUTC.utcOffset(), -2 * 3600);
    QVERIFY(!date_OffsetFromUTC.timeZone().isValid());
    QCOMPARE(date_OffsetFromUTC.date(), d);
    QCOMPARE(date_OffsetFromUTC.time(), QTime(0, 0, 0));
    QCOMPARE(date_OffsetFromUTC.qDateTime(), QDateTime(d, QTime(0, 0, 0), Qt::OffsetFromUTC, -2 * 3600));

    KADateTime dateTime_OffsetFromUTC(d, t, KADateTime::Spec::OffsetFromUTC(2 * 3600));
    QVERIFY(!dateTime_OffsetFromUTC.isNull());
    QVERIFY(dateTime_OffsetFromUTC.isValid());
    QVERIFY(!dateTime_OffsetFromUTC.isDateOnly());
    QCOMPARE(dateTime_OffsetFromUTC.timeType(), KADateTime::OffsetFromUTC);
    QVERIFY(!dateTime_OffsetFromUTC.isUtc());
    QVERIFY(dateTime_OffsetFromUTC.isOffsetFromUtc());
    QVERIFY(!dateTime_OffsetFromUTC.isLocalZone());
    QCOMPARE(dateTime_OffsetFromUTC.utcOffset(), 2 * 3600);
    QVERIFY(!dateTime_OffsetFromUTC.timeZone().isValid());
    QCOMPARE(dateTime_OffsetFromUTC.date(), d);
    QCOMPARE(dateTime_OffsetFromUTC.time(), t);
    QCOMPARE(dateTime_OffsetFromUTC.qDateTime(), QDateTime(d, t, Qt::OffsetFromUTC, 2 * 3600));

    KADateTime datetime_OffsetFromUTC(dtLocal, KADateTime::Spec::OffsetFromUTC(-2 * 3600));
    QVERIFY(!datetime_OffsetFromUTC.isNull());
    QVERIFY(datetime_OffsetFromUTC.isValid());
    QVERIFY(!datetime_OffsetFromUTC.isDateOnly());
    QCOMPARE(datetime_OffsetFromUTC.timeType(), KADateTime::OffsetFromUTC);
    QVERIFY(!datetime_OffsetFromUTC.isUtc());
    QVERIFY(datetime_OffsetFromUTC.isOffsetFromUtc());
    QVERIFY(!datetime_OffsetFromUTC.isLocalZone());
    QCOMPARE(datetime_OffsetFromUTC.utcOffset(), -2 * 3600);
    QVERIFY(!datetime_OffsetFromUTC.timeZone().isValid());
    QCOMPARE(datetime_OffsetFromUTC.date(), dtLocal.date());
    QCOMPARE(datetime_OffsetFromUTC.time(), dtLocal.time().addSecs(6 * 3600));
    QCOMPARE(datetime_OffsetFromUTC.qDateTime(), dtLocal.toOffsetFromUtc(-2 * 3600));

    KADateTime datetime_OffsetFromUTC2(dtUTC, KADateTime::Spec::OffsetFromUTC(2 * 3600));
    QVERIFY(!datetime_OffsetFromUTC2.isNull());
    QVERIFY(datetime_OffsetFromUTC2.isValid());
    QVERIFY(!datetime_OffsetFromUTC2.isDateOnly());
    QCOMPARE(datetime_OffsetFromUTC2.timeType(), KADateTime::OffsetFromUTC);
    QVERIFY(!datetime_OffsetFromUTC2.isUtc());
    QVERIFY(datetime_OffsetFromUTC2.isOffsetFromUtc());
    QVERIFY(!datetime_OffsetFromUTC2.isLocalZone());
    QCOMPARE(datetime_OffsetFromUTC2.utcOffset(), 2 * 3600);
    QVERIFY(!datetime_OffsetFromUTC2.timeZone().isValid());
    {
        QDateTime dtof = dtUTC.addSecs(2 * 3600);
        dtof.setTimeSpec(Qt::LocalTime);
        QCOMPARE(datetime_OffsetFromUTC2.date(), dtof.date());
        QCOMPARE(datetime_OffsetFromUTC2.time(), dtof.time());
    }
    QCOMPARE(datetime_OffsetFromUTC2.qDateTime(), dtUTC.toOffsetFromUtc(2 * 3600));

    // ... copy constructor
    KADateTime datetime_OffsetFromUTCCopy(datetime_OffsetFromUTC);
    QVERIFY(!datetime_OffsetFromUTCCopy.isNull());
    QVERIFY(datetime_OffsetFromUTCCopy.isValid());
    QVERIFY(!datetime_OffsetFromUTCCopy.isDateOnly());
    QCOMPARE(datetime_OffsetFromUTCCopy.timeType(), KADateTime::OffsetFromUTC);
    QVERIFY(!datetime_OffsetFromUTCCopy.isUtc());
    QVERIFY(datetime_OffsetFromUTCCopy.isOffsetFromUtc());
    QVERIFY(!datetime_OffsetFromUTCCopy.isLocalZone());
    QCOMPARE(datetime_OffsetFromUTCCopy.utcOffset(), -2 * 3600);
    QVERIFY(!datetime_OffsetFromUTCCopy.timeZone().isValid());
    QCOMPARE(datetime_OffsetFromUTCCopy.date(), datetime_OffsetFromUTC.date());
    QCOMPARE(datetime_OffsetFromUTCCopy.time(), datetime_OffsetFromUTC.time());
    QCOMPARE(datetime_OffsetFromUTCCopy.qDateTime(), datetime_OffsetFromUTC.qDateTime());

    // Local time zone
    KADateTime date_LocalZone(d, KADateTime::Spec::LocalZone());
    QVERIFY(!date_LocalZone.isNull());
    QVERIFY(date_LocalZone.isValid());
    QVERIFY(date_LocalZone.isDateOnly());
    QCOMPARE(date_LocalZone.timeType(), KADateTime::LocalZone);
    QCOMPARE(date_LocalZone.timeSpec(), KADateTime::Spec::LocalZone());
    QVERIFY(!date_LocalZone.isUtc());
    QVERIFY(!date_LocalZone.isOffsetFromUtc());
    QVERIFY(date_LocalZone.isLocalZone());
    QCOMPARE(date_LocalZone.utcOffset(), -8 * 3600);
    QCOMPARE(date_LocalZone.timeZone(), QTimeZone::systemTimeZone());
    QCOMPARE(date_LocalZone.date(), d);
    QCOMPARE(date_LocalZone.time(), QTime(0, 0, 0));
    QCOMPARE(date_LocalZone.qDateTime(), QDateTime(d, QTime(0, 0, 0), Qt::LocalTime));

    KADateTime dateTime_LocalZone(d, t, KADateTime::LocalZone);
    QVERIFY(!dateTime_LocalZone.isNull());
    QVERIFY(dateTime_LocalZone.isValid());
    QVERIFY(!dateTime_LocalZone.isDateOnly());
    QCOMPARE(dateTime_LocalZone.timeType(), KADateTime::LocalZone);
    QCOMPARE(dateTime_LocalZone.timeSpec(), KADateTime::Spec::LocalZone());
    QVERIFY(!dateTime_LocalZone.isUtc());
    QVERIFY(!dateTime_LocalZone.isOffsetFromUtc());
    QVERIFY(dateTime_LocalZone.isLocalZone());
    QCOMPARE(dateTime_LocalZone.utcOffset(), -8 * 3600);
    QCOMPARE(dateTime_LocalZone.timeZone(), QTimeZone::systemTimeZone());
    QCOMPARE(dateTime_LocalZone.date(), d);
    QCOMPARE(dateTime_LocalZone.time(), t);
    QCOMPARE(dateTime_LocalZone.qDateTime(), QDateTime(d, t, Qt::LocalTime));

    KADateTime datetime_LocalZone(dtLocal, KADateTime::LocalZone);
    QVERIFY(!datetime_LocalZone.isNull());
    QVERIFY(datetime_LocalZone.isValid());
    QVERIFY(!datetime_LocalZone.isDateOnly());
    QCOMPARE(datetime_LocalZone.timeType(), KADateTime::LocalZone);
    QCOMPARE(datetime_LocalZone.timeSpec(), KADateTime::Spec::LocalZone());
    QVERIFY(!datetime_LocalZone.isUtc());
    QVERIFY(!datetime_LocalZone.isOffsetFromUtc());
    QVERIFY(datetime_LocalZone.isLocalZone());
    QCOMPARE(datetime_LocalZone.utcOffset(), -8 * 3600);
    QCOMPARE(datetime_LocalZone.timeZone(), QTimeZone::systemTimeZone());
    QCOMPARE(datetime_LocalZone.date(), dtLocal.date());
    QCOMPARE(datetime_LocalZone.time(), dtLocal.time());
    QCOMPARE(datetime_LocalZone.qDateTime(), dtLocal);

    KADateTime datetime_LocalZone2(dtUTC, KADateTime::LocalZone);
    QVERIFY(!datetime_LocalZone2.isNull());
    QVERIFY(datetime_LocalZone2.isValid());
    QVERIFY(!datetime_LocalZone2.isDateOnly());
    QCOMPARE(datetime_LocalZone2.timeType(), KADateTime::LocalZone);
    QCOMPARE(datetime_LocalZone2.timeSpec(), KADateTime::Spec::LocalZone());
    QVERIFY(!datetime_LocalZone2.isUtc());
    QVERIFY(!datetime_LocalZone2.isOffsetFromUtc());
    QVERIFY(datetime_LocalZone2.isLocalZone());
    QCOMPARE(datetime_LocalZone2.utcOffset(), -8 * 3600);
    QCOMPARE(datetime_LocalZone2.timeZone(), QTimeZone::systemTimeZone());
    {
        QDateTime local = dtUTC.toLocalTime();
        QCOMPARE(datetime_LocalZone2.date(), local.date());
        QCOMPARE(datetime_LocalZone2.time(), local.time());
    }
    QCOMPARE(datetime_LocalZone2.qDateTime(), dtUTC.toLocalTime());

    // ... copy constructor
    KADateTime datetime_LocalZoneCopy(datetime_LocalZone);
    QVERIFY(!datetime_LocalZoneCopy.isNull());
    QVERIFY(datetime_LocalZoneCopy.isValid());
    QVERIFY(!datetime_LocalZoneCopy.isDateOnly());
    QCOMPARE(datetime_LocalZoneCopy.timeType(), KADateTime::LocalZone);
    QCOMPARE(datetime_LocalZoneCopy.timeSpec(), KADateTime::Spec::LocalZone());
    QVERIFY(!datetime_LocalZoneCopy.isUtc());
    QVERIFY(!datetime_LocalZoneCopy.isOffsetFromUtc());
    QVERIFY(datetime_LocalZoneCopy.isLocalZone());
    QCOMPARE(datetime_LocalZoneCopy.utcOffset(), -8 * 3600);
    QCOMPARE(datetime_LocalZoneCopy.timeZone(), datetime_LocalZone.timeZone());
    QCOMPARE(datetime_LocalZoneCopy.date(), datetime_LocalZone.date());
    QCOMPARE(datetime_LocalZoneCopy.time(), datetime_LocalZone.time());
    QCOMPARE(datetime_LocalZoneCopy.qDateTime(), datetime_LocalZone.qDateTime());

    // Invalid time zone specification for a constructor
    KADateTime date_TimeZone(d, KADateTime::Spec(KADateTime::TimeZone));
    QVERIFY(!date_TimeZone.isValid());
    KADateTime dateTime_TimeZone(d, t, KADateTime::Spec(KADateTime::TimeZone));
    QVERIFY(!dateTime_TimeZone.isValid());
    KADateTime datetime_TimeZone(dtLocal, KADateTime::Spec(KADateTime::TimeZone));
    QVERIFY(!datetime_TimeZone.isValid());
    KADateTime datetime_Invalid(dtLocal, KADateTime::Spec(KADateTime::Invalid));
    QVERIFY(!datetime_Invalid.isValid());

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

///////////////////////////////////
// Time conversion and operator==()
///////////////////////////////////

void KADateTimeTest::toUtc()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Zone -> UTC
    KADateTime londonWinter(QDate(2005, 1, 1), QTime(0, 0, 0), london);
    KADateTime utcWinter = londonWinter.toUtc();
    QVERIFY(utcWinter.isUtc());
    QCOMPARE(utcWinter.date(), QDate(2005, 1, 1));
    QCOMPARE(utcWinter.time(), QTime(0, 0, 0));
    QVERIFY(londonWinter == utcWinter);
    KADateTime londonSummer(QDate(2005, 6, 1), QTime(0, 0, 0), london);
    KADateTime utcSummer = londonSummer.toUtc();
    QVERIFY(utcSummer.isUtc());
    QCOMPARE(utcSummer.date(), QDate(2005, 5, 31));
    QCOMPARE(utcSummer.time(), QTime(23, 0, 0));
    QVERIFY(londonSummer == utcSummer);
    QVERIFY(!(londonSummer == utcWinter));
    QVERIFY(!(londonWinter == utcSummer));

    // UTC offset -> UTC
    KADateTime offset(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime utcOffset = offset.toUtc();
    QVERIFY(utcOffset.isUtc());
    QCOMPARE(utcOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(utcOffset.time(), QTime(2, 32, 30));
    QVERIFY(offset == utcOffset);
    QVERIFY(!(offset == utcSummer));

    // Local time -> UTC
    KADateTime localz(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::LocalZone);
    KADateTime utcLocalz = localz.toUtc();
    QVERIFY(utcLocalz.isUtc());
    QCOMPARE(utcLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(utcLocalz.time(), QTime(8, 2, 30));
    QVERIFY(localz == utcLocalz);
    QVERIFY(!(localz == utcOffset));

    // UTC -> UTC
    KADateTime utc(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::UTC);
    KADateTime utcUtc = utc.toUtc();
    QVERIFY(utcUtc.isUtc());
    QCOMPARE(utcUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(utcUtc.time(), QTime(1, 2, 30));
    QVERIFY(utc == utcUtc);
    QVERIFY(!(utc == utcLocalz));

    // ** Date only ** //

    // Zone -> UTC
    londonSummer.setDateOnly(true);
    utcSummer = londonSummer.toUtc();
    QVERIFY(utcSummer.isDateOnly());
    QCOMPARE(utcSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(utcSummer.time(), QTime(0, 0, 0));
    QVERIFY(utcSummer != londonSummer);
    QVERIFY(!(utcSummer == londonSummer));
    londonWinter.setDateOnly(true);
    utcWinter = londonWinter.toUtc();
    QVERIFY(utcWinter == londonWinter);
    QVERIFY(!(utcWinter != londonWinter));

    // UTC offset -> UTC
    offset.setDateOnly(true);
    utcOffset = offset.toUtc();
    QVERIFY(utcOffset.isDateOnly());
    QCOMPARE(utcOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(utcOffset.time(), QTime(0, 0, 0));
    QVERIFY(offset != utcOffset);
    QVERIFY(!(offset == utcOffset));
    KADateTime utcOffset1(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(0));
    QVERIFY(utcOffset1 == utcOffset1.toUtc());

    // Local time -> UTC
    localz.setDateOnly(true);
    utcLocalz = localz.toUtc();
    QVERIFY(utcLocalz.isDateOnly());
    QCOMPARE(utcLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(utcLocalz.time(), QTime(0, 0, 0));
    QVERIFY(localz != utcLocalz);
    QVERIFY(!(localz == utcLocalz));

    // UTC -> UTC
    utc.setDateOnly(true);
    utcUtc = utc.toUtc();
    QVERIFY(utcUtc.isDateOnly());
    QCOMPARE(utcUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(utcUtc.time(), QTime(0, 0, 0));
    QVERIFY(utc == utcUtc);
    QVERIFY(!(utc != utcUtc));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::toOffsetFromUtc()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // ***** toOffsetFromUtc(void) *****

    // Zone -> UTC offset
    KADateTime londonWinter(QDate(2005, 1, 1), QTime(2, 0, 0), london);
    KADateTime offsetWinter = londonWinter.toOffsetFromUtc();
    QVERIFY(offsetWinter.isOffsetFromUtc());
    QCOMPARE(offsetWinter.utcOffset(), 0);
    QCOMPARE(offsetWinter.date(), QDate(2005, 1, 1));
    QCOMPARE(offsetWinter.time(), QTime(2, 0, 0));
    QVERIFY(londonWinter == offsetWinter);
    KADateTime londonSummer(QDate(2005, 6, 1), QTime(14, 0, 0), london);
    KADateTime offsetSummer = londonSummer.toOffsetFromUtc();
    QVERIFY(offsetSummer.isOffsetFromUtc());
    QCOMPARE(offsetSummer.utcOffset(), 3600);
    QCOMPARE(offsetSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(offsetSummer.time(), QTime(14, 0, 0));
    QVERIFY(londonSummer == offsetSummer);
    QVERIFY(!(londonSummer == offsetWinter));
    QVERIFY(!(londonWinter == offsetSummer));

    // UTC offset -> UTC offset
    KADateTime offset(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime offsetOffset = offset.toOffsetFromUtc();
    QVERIFY(offsetOffset.isOffsetFromUtc());
    QCOMPARE(offsetOffset.utcOffset(), -5400);
    QCOMPARE(offsetOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetOffset.time(), QTime(11, 2, 30));
    QVERIFY(offset == offsetOffset);
    QVERIFY(!(offset == offsetSummer));

    // Local time -> UTC offset
    KADateTime localz(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::LocalZone);
    KADateTime offsetLocalz = localz.toOffsetFromUtc();
    QVERIFY(offsetLocalz.isOffsetFromUtc());
    QCOMPARE(offsetLocalz.utcOffset(), -7 * 3600);
    QCOMPARE(offsetLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetLocalz.time(), QTime(1, 2, 30));
    QVERIFY(localz == offsetLocalz);
    QVERIFY(!(localz == offsetOffset));

    // UTC -> UTC offset
    KADateTime utc(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::UTC);
    KADateTime offsetUtc = utc.toOffsetFromUtc();
    QVERIFY(offsetUtc.isOffsetFromUtc());
    QCOMPARE(offsetUtc.utcOffset(), 0);
    QCOMPARE(offsetUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetUtc.time(), QTime(11, 2, 30));
    QVERIFY(utc == offsetUtc);
    QVERIFY(!(utc == offsetLocalz));

    // ** Date only ** //

    // Zone -> UTC offset
    londonSummer.setDateOnly(true);
    offsetSummer = londonSummer.toOffsetFromUtc();
    QVERIFY(offsetSummer.isDateOnly());
    QVERIFY(offsetSummer.isOffsetFromUtc());
    QCOMPARE(offsetSummer.utcOffset(), 3600);
    QCOMPARE(offsetSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(offsetSummer.time(), QTime(0, 0, 0));
    QVERIFY(offsetSummer == londonSummer);
    QVERIFY(!(offsetSummer != londonSummer));
    londonWinter.setDateOnly(true);
    offsetWinter = londonWinter.toUtc();
    QVERIFY(offsetWinter == londonWinter);
    QVERIFY(!(offsetWinter != londonWinter));

    // UTC offset -> UTC offset
    offset.setDateOnly(true);
    offsetOffset = offset.toOffsetFromUtc();
    QVERIFY(offsetOffset.isDateOnly());
    QVERIFY(offsetOffset.isOffsetFromUtc());
    QCOMPARE(offsetOffset.utcOffset(), -5400);
    QCOMPARE(offsetOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetOffset.time(), QTime(0, 0, 0));
    QVERIFY(offset == offsetOffset);
    QVERIFY(!(offset != offsetOffset));

    // Local time -> UTC offset
    localz.setDateOnly(true);
    offsetLocalz = localz.toOffsetFromUtc();
    QVERIFY(offsetLocalz.isDateOnly());
    QVERIFY(offsetLocalz.isOffsetFromUtc());
    QCOMPARE(offsetLocalz.utcOffset(), -7 * 3600);
    QCOMPARE(offsetLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetLocalz.time(), QTime(0, 0, 0));
    QVERIFY(localz == offsetLocalz);
    QVERIFY(!(localz != offsetLocalz));

    // UTC -> UTC offset
    utc.setDateOnly(true);
    offsetUtc = utc.toOffsetFromUtc();
    QVERIFY(offsetUtc.isDateOnly());
    QVERIFY(offsetUtc.isOffsetFromUtc());
    QCOMPARE(offsetUtc.utcOffset(), 0);
    QCOMPARE(offsetUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetUtc.time(), QTime(0, 0, 0));
    QVERIFY(utc == offsetUtc);
    QVERIFY(!(utc != offsetUtc));

    // ***** toOffsetFromUtc(int utcOffset) *****

    // Zone -> UTC offset
    KADateTime londonWinter2(QDate(2005, 1, 1), QTime(2, 0, 0), london);
    offsetWinter = londonWinter2.toOffsetFromUtc(5400);    // +1H30M
    QVERIFY(offsetWinter.isOffsetFromUtc());
    QCOMPARE(offsetWinter.utcOffset(), 5400);
    QCOMPARE(offsetWinter.date(), QDate(2005, 1, 1));
    QCOMPARE(offsetWinter.time(), QTime(3, 30, 0));
    QVERIFY(londonWinter2 == offsetWinter);
    KADateTime londonSummer2(QDate(2005, 6, 1), QTime(14, 0, 0), london);
    offsetSummer = londonSummer2.toOffsetFromUtc(5400);
    QVERIFY(offsetSummer.isOffsetFromUtc());
    QCOMPARE(offsetSummer.utcOffset(), 5400);
    QCOMPARE(offsetSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(offsetSummer.time(), QTime(14, 30, 0));
    QVERIFY(londonSummer2 == offsetSummer);
    QVERIFY(!(londonSummer2 == offsetWinter));
    QVERIFY(!(londonWinter2 == offsetSummer));

    // UTC offset -> UTC offset
    KADateTime offset2(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    offsetOffset = offset2.toOffsetFromUtc(3600);
    QVERIFY(offsetOffset.isOffsetFromUtc());
    QCOMPARE(offsetOffset.utcOffset(), 3600);
    QCOMPARE(offsetOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetOffset.time(), QTime(13, 32, 30));
    QVERIFY(offset2 == offsetOffset);
    QVERIFY(!(offset2 == offsetSummer));

    // Local time -> UTC offset
    KADateTime localz2(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::LocalZone);
    offsetLocalz = localz2.toOffsetFromUtc(0);
    QVERIFY(offsetLocalz.isOffsetFromUtc());
    QCOMPARE(offsetLocalz.utcOffset(), 0);
    QCOMPARE(offsetLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetLocalz.time(), QTime(8, 2, 30));
    QVERIFY(localz2 == offsetLocalz);
    QVERIFY(!(localz2 == offsetOffset));

    // UTC -> UTC offset
    KADateTime utc2(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::UTC);
    offsetUtc = utc2.toOffsetFromUtc(-3600);
    QVERIFY(offsetUtc.isOffsetFromUtc());
    QCOMPARE(offsetUtc.utcOffset(), -3600);
    QCOMPARE(offsetUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetUtc.time(), QTime(10, 2, 30));
    QVERIFY(utc2 == offsetUtc);
    QVERIFY(!(utc2 == offsetLocalz));

    // ** Date only ** //

    // Zone -> UTC offset
    londonSummer2.setDateOnly(true);
    offsetSummer = londonSummer2.toOffsetFromUtc(5400);
    QVERIFY(offsetSummer.isDateOnly());
    QVERIFY(offsetSummer.isOffsetFromUtc());
    QCOMPARE(offsetSummer.utcOffset(), 5400);
    QCOMPARE(offsetSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(offsetSummer.time(), QTime(0, 0, 0));
    QVERIFY(londonSummer2 != offsetSummer);
    QVERIFY(!(londonSummer2 == offsetSummer));
    QVERIFY(londonSummer2 == KADateTime(QDate(2005, 6, 1), KADateTime::Spec::OffsetFromUTC(3600)));

    // UTC offset -> UTC offset
    offset2.setDateOnly(true);
    offsetOffset = offset2.toOffsetFromUtc(-3600);
    QVERIFY(offsetOffset.isDateOnly());
    QVERIFY(offsetOffset.isOffsetFromUtc());
    QCOMPARE(offsetOffset.utcOffset(), -3600);
    QCOMPARE(offsetOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetOffset.time(), QTime(0, 0, 0));
    QVERIFY(offset2 != offsetOffset);
    QVERIFY(!(offset2 == offsetOffset));

    // Local time -> UTC offset
    localz2.setDateOnly(true);
    offsetLocalz = localz2.toOffsetFromUtc(6 * 3600);
    QVERIFY(offsetLocalz.isDateOnly());
    QVERIFY(offsetLocalz.isOffsetFromUtc());
    QCOMPARE(offsetLocalz.utcOffset(), 6 * 3600);
    QCOMPARE(offsetLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetLocalz.time(), QTime(0, 0, 0));
    QVERIFY(localz2 != offsetLocalz);
    QVERIFY(!(localz2 == offsetLocalz));
    QVERIFY(localz == KADateTime(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(-7 * 3600)));

    // UTC -> UTC offset
    utc2.setDateOnly(true);
    offsetUtc = utc2.toOffsetFromUtc(1800);
    QVERIFY(offsetUtc.isDateOnly());
    QVERIFY(offsetUtc.isOffsetFromUtc());
    QCOMPARE(offsetUtc.utcOffset(), 1800);
    QCOMPARE(offsetUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(offsetUtc.time(), QTime(0, 0, 0));
    QVERIFY(utc2 != offsetUtc);
    QVERIFY(!(utc2 == offsetUtc));
    QVERIFY(utc2 == KADateTime(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(0)));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::toLocalZone()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Zone -> LocalZone
    KADateTime londonWinter(QDate(2005, 1, 1), QTime(0, 0, 0), london);
    KADateTime locWinter = londonWinter.toLocalZone();
    QVERIFY(locWinter.isLocalZone());
    QCOMPARE(locWinter.date(), QDate(2004, 12, 31));
    QCOMPARE(locWinter.time(), QTime(16, 0, 0));
    QVERIFY(londonWinter == locWinter);
    KADateTime londonSummer(QDate(2005, 6, 1), QTime(0, 0, 0), london);
    KADateTime locSummer = londonSummer.toLocalZone();
    QVERIFY(locSummer.isLocalZone());
    QCOMPARE(locSummer.date(), QDate(2005, 5, 31));
    QCOMPARE(locSummer.time(), QTime(16, 0, 0));
    QVERIFY(londonSummer == locSummer);
    QVERIFY(!(londonSummer == locWinter));
    QVERIFY(!(londonWinter == locSummer));

    // UTC offset -> LocalZone
    KADateTime offset(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime locOffset = offset.toLocalZone();
    QVERIFY(locOffset.isLocalZone());
    QCOMPARE(locOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(locOffset.time(), QTime(5, 32, 30));
    QVERIFY(offset == locOffset);
    QVERIFY(!(offset == locSummer));

    // UTC -> LocalZone
    KADateTime utc(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::UTC);
    KADateTime locUtc = utc.toLocalZone();
    QVERIFY(locUtc.isLocalZone());
    QCOMPARE(locUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(locUtc.time(), QTime(4, 2, 30));
    QVERIFY(utc == locUtc);

    // ** Date only ** //

    // Zone -> LocalZone
    londonSummer.setDateOnly(true);
    locSummer = londonSummer.toLocalZone();
    QVERIFY(locSummer.isDateOnly());
    QCOMPARE(locSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(locSummer.time(), QTime(0, 0, 0));
    QVERIFY(londonSummer != locSummer);
    QVERIFY(!(londonSummer == locSummer));

    // UTC offset -> LocalZone
    offset.setDateOnly(true);
    locOffset = offset.toLocalZone();
    QVERIFY(locOffset.isDateOnly());
    QCOMPARE(locOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(locOffset.time(), QTime(0, 0, 0));
    QVERIFY(offset != locOffset);
    QVERIFY(!(offset == locOffset));
    QVERIFY(locOffset == KADateTime(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(-7 * 3600)));

    // UTC -> LocalZone
    utc.setDateOnly(true);
    locUtc = utc.toLocalZone();
    QVERIFY(locUtc.isDateOnly());
    QCOMPARE(locUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(locUtc.time(), QTime(0, 0, 0));
    QVERIFY(utc != locUtc);
    QVERIFY(!(utc == locUtc));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::toZone()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");

    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":Europe/London");
    ::tzset();

    // Zone -> Zone
    KADateTime londonWinter(QDate(2005, 1, 1), QTime(0, 0, 0), london);
    KADateTime locWinter = londonWinter.toZone(losAngeles);
    QCOMPARE(locWinter.timeZone(), losAngeles);
    QCOMPARE(locWinter.date(), QDate(2004, 12, 31));
    QCOMPARE(locWinter.time(), QTime(16, 0, 0));
    QVERIFY(londonWinter == locWinter);
    KADateTime londonSummer(QDate(2005, 6, 1), QTime(0, 0, 0), london);
    KADateTime locSummer = londonSummer.toZone(losAngeles);
    QCOMPARE(locWinter.timeZone(), losAngeles);
    QCOMPARE(locSummer.date(), QDate(2005, 5, 31));
    QCOMPARE(locSummer.time(), QTime(16, 0, 0));
    QVERIFY(londonSummer == locSummer);
    QVERIFY(!(londonSummer == locWinter));
    QVERIFY(!(londonWinter == locSummer));

    // UTC offset -> Zone
    KADateTime offset(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime locOffset = offset.toZone(losAngeles);
    QCOMPARE(locOffset.timeZone(), losAngeles);
    QCOMPARE(locOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(locOffset.time(), QTime(5, 32, 30));
    QVERIFY(offset == locOffset);
    QVERIFY(!(offset == locSummer));

    // Local time -> Zone
    KADateTime localz(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::LocalZone);
    KADateTime locLocalz = localz.toZone(losAngeles);
    QCOMPARE(locLocalz.timeZone(), losAngeles);
    QCOMPARE(locLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(locLocalz.time(), QTime(3, 2, 30));
    QVERIFY(localz == locLocalz);
    QVERIFY(!(localz == locOffset));

    // UTC -> Zone
    KADateTime utc(QDate(2005, 6, 6), QTime(11, 2, 30), KADateTime::UTC);
    KADateTime locUtc = utc.toZone(losAngeles);
    QCOMPARE(locUtc.timeZone(), losAngeles);
    QCOMPARE(locUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(locUtc.time(), QTime(4, 2, 30));
    QVERIFY(utc == locUtc);
    QVERIFY(!(utc == locLocalz));

    // ** Date only ** //

    // Zone -> Zone
    londonSummer.setDateOnly(true);
    locSummer = londonSummer.toZone(losAngeles);
    QVERIFY(locSummer.isDateOnly());
    QCOMPARE(locSummer.date(), QDate(2005, 6, 1));
    QCOMPARE(locSummer.time(), QTime(0, 0, 0));
    QVERIFY(londonSummer != locSummer);
    QVERIFY(!(londonSummer == locSummer));

    // UTC offset -> Zone
    offset.setDateOnly(true);
    locOffset = offset.toZone(losAngeles);
    QVERIFY(locOffset.isDateOnly());
    QCOMPARE(locOffset.date(), QDate(2005, 6, 6));
    QCOMPARE(locOffset.time(), QTime(0, 0, 0));
    QVERIFY(offset != locOffset);
    QVERIFY(!(offset == locOffset));

    // Local time -> Zone
    localz.setDateOnly(true);
    locLocalz = localz.toZone(losAngeles);
    QVERIFY(locLocalz.isDateOnly());
    QCOMPARE(locLocalz.date(), QDate(2005, 6, 6));
    QCOMPARE(locLocalz.time(), QTime(0, 0, 0));
    QVERIFY(localz != locLocalz);
    QVERIFY(!(localz == locLocalz));

    // UTC -> Zone
    utc.setDateOnly(true);
    locUtc = utc.toZone(losAngeles);
    QVERIFY(locUtc.isDateOnly());
    QCOMPARE(locUtc.date(), QDate(2005, 6, 6));
    QCOMPARE(locUtc.time(), QTime(0, 0, 0));
    QVERIFY(utc != locUtc);
    QVERIFY(!(utc == locUtc));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::toTimeSpec()
{
    QTimeZone london("Europe/London");
    QTimeZone cairo("Africa/Cairo");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    KADateTime::Spec utcSpec(KADateTime::UTC);
    KADateTime::Spec cairoSpec(cairo);
    KADateTime::Spec offset1200Spec(KADateTime::OffsetFromUTC, 1200);
    KADateTime::Spec localzSpec(KADateTime::LocalZone);

    KADateTime utc1(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC);
    KADateTime zone1(QDate(2004, 3, 1), QTime(3, 45, 2), cairo);
    KADateTime offset1(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::Spec::OffsetFromUTC(1200)); // +00:20
    KADateTime localz1(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::LocalZone);

    KADateTime utc(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::UTC);
    KADateTime zone(QDate(2005, 7, 1), QTime(2, 0, 0), london);
    KADateTime offset(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -01:30
    KADateTime localz(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::LocalZone);

    // To UTC
    KADateTime utcZone = zone.toTimeSpec(utcSpec);
    QVERIFY(utcZone.isUtc());
    QVERIFY(utcZone == KADateTime(QDate(2005, 7, 1), QTime(1, 0, 0), KADateTime::UTC));
    QVERIFY(zone.timeSpec() != utcSpec);
    QVERIFY(utcZone.timeSpec() == utcSpec);

    KADateTime utcOffset = offset.toTimeSpec(utcSpec);
    QVERIFY(utcOffset.isUtc());
    QVERIFY(utcOffset == KADateTime(QDate(2005, 6, 6), QTime(2, 32, 30), KADateTime::UTC));
    QVERIFY(offset.timeSpec() != utcSpec);
    QVERIFY(utcOffset.timeSpec() == utcSpec);

    KADateTime utcLocalz = localz.toTimeSpec(utcSpec);
    QVERIFY(utcLocalz.isUtc());
    QVERIFY(utcLocalz == KADateTime(QDate(2005, 6, 6), QTime(8, 2, 30), KADateTime::UTC));
    QVERIFY(localz.timeSpec() != utcSpec);
    QVERIFY(utcZone.timeSpec() == utcSpec);

    KADateTime utcUtc = utc.toTimeSpec(utcSpec);
    QVERIFY(utcUtc.isUtc());
    QVERIFY(utcUtc == KADateTime(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::UTC));
    QVERIFY(utc.timeSpec() == utcSpec);
    QVERIFY(utcUtc.timeSpec() == utcSpec);

    // To Zone
    KADateTime zoneZone = zone.toTimeSpec(cairoSpec);
    QCOMPARE(zoneZone.timeZone(), cairo);
    QVERIFY(zoneZone == KADateTime(QDate(2005, 7, 1), QTime(4, 0, 0), cairo));
    QVERIFY(zone.timeSpec() != cairoSpec);
    QVERIFY(zoneZone.timeSpec() == cairoSpec);

    KADateTime zoneOffset = offset.toTimeSpec(cairoSpec);
    QCOMPARE(zoneOffset.timeZone(), cairo);
    QVERIFY(zoneOffset == KADateTime(QDate(2005, 6, 6), QTime(5, 32, 30), cairo));
    QVERIFY(offset.timeSpec() != cairoSpec);
    QVERIFY(zoneOffset.timeSpec() == cairoSpec);

    KADateTime zoneLocalz = localz.toTimeSpec(cairoSpec);
    QCOMPARE(zoneLocalz.timeZone(), cairo);
    QVERIFY(zoneLocalz == KADateTime(QDate(2005, 6, 6), QTime(11, 2, 30), cairo));
    QVERIFY(localz.timeSpec() != cairoSpec);
    QVERIFY(zoneLocalz.timeSpec() == cairoSpec);

    KADateTime zoneUtc = utc.toTimeSpec(cairoSpec);
    QCOMPARE(zoneUtc.timeZone(), cairo);
    QVERIFY(zoneUtc == KADateTime(QDate(2005, 6, 6), QTime(4, 2, 30), cairo));
    QVERIFY(utc.timeSpec() != cairoSpec);
    QVERIFY(zoneUtc.timeSpec() == cairoSpec);

    // To UTC offset
    KADateTime offsetZone = zone.toTimeSpec(offset1200Spec);
    QVERIFY(offsetZone.isOffsetFromUtc());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetZone == KADateTime(QDate(2005, 7, 1), QTime(1, 20, 0), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(zone.timeSpec() != offset1200Spec);
    QVERIFY(offsetZone.timeSpec() == offset1200Spec);

    KADateTime offsetOffset = offset.toTimeSpec(offset1200Spec);
    QVERIFY(offsetOffset.isOffsetFromUtc());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetOffset == KADateTime(QDate(2005, 6, 6), QTime(2, 52, 30), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(offset.timeSpec() != offset1200Spec);
    QVERIFY(offsetOffset.timeSpec() == offset1200Spec);

    KADateTime offsetLocalz = localz.toTimeSpec(offset1200Spec);
    QVERIFY(offsetLocalz.isOffsetFromUtc());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetLocalz == KADateTime(QDate(2005, 6, 6), QTime(8, 22, 30), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(localz.timeSpec() != offset1200Spec);
    QVERIFY(offsetLocalz.timeSpec() == offset1200Spec);

    KADateTime offsetUtc = utc.toTimeSpec(offset1200Spec);
    QVERIFY(offsetUtc.isOffsetFromUtc());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetUtc == KADateTime(QDate(2005, 6, 6), QTime(1, 22, 30), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(utc.timeSpec() != offset1200Spec);
    QVERIFY(offsetUtc.timeSpec() == offset1200Spec);

    // To Local time
    KADateTime localzZone = zone.toTimeSpec(localzSpec);
    QVERIFY(localzZone.isLocalZone());
    QVERIFY(localzZone == KADateTime(QDate(2005, 6, 30), QTime(18, 0, 0), KADateTime::LocalZone));
    QVERIFY(zone.timeSpec() != localzSpec);
    QVERIFY(localzZone.timeSpec() == localzSpec);

    KADateTime localzOffset = offset.toTimeSpec(localzSpec);
    QVERIFY(localzOffset.isLocalZone());
    QVERIFY(localzOffset == KADateTime(QDate(2005, 6, 5), QTime(19, 32, 30), KADateTime::LocalZone));
    QVERIFY(offset.timeSpec() != localzSpec);
    QVERIFY(localzOffset.timeSpec() == localzSpec);

    KADateTime localzLocalz = localz.toTimeSpec(localzSpec);
    QVERIFY(localzLocalz.isLocalZone());
    QVERIFY(localzLocalz == KADateTime(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::LocalZone));
    QVERIFY(localz.timeSpec() == localzSpec);
    QVERIFY(localzLocalz.timeSpec() == localzSpec);

    KADateTime localzUtc = utc.toTimeSpec(localzSpec);
    QVERIFY(localzUtc.isLocalZone());
    QVERIFY(localzUtc == KADateTime(QDate(2005, 6, 5), QTime(18, 2, 30), KADateTime::LocalZone));
    QVERIFY(utc.timeSpec() != localzSpec);
    QVERIFY(localzUtc.timeSpec() == localzSpec);

    // ** Date only ** //

    KADateTime zoned(QDate(2005, 7, 1), london);
    KADateTime offsetd(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(-5400)); // -01:30
    KADateTime localzd(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::LocalZone));
    KADateTime utcd(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::UTC));

    // To UTC
    utcZone = zoned.toTimeSpec(utcSpec);
    QVERIFY(utcZone.isUtc());
    QVERIFY(utcZone.isDateOnly());
    QVERIFY(utcZone == KADateTime(QDate(2005, 7, 1), KADateTime::Spec(KADateTime::UTC)));
    QVERIFY(utcZone != zoned);

    utcOffset = offsetd.toTimeSpec(utcSpec);
    QVERIFY(utcOffset.isUtc());
    QVERIFY(utcOffset.isDateOnly());
    QVERIFY(utcOffset == KADateTime(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::UTC)));
    QVERIFY(utcOffset != offsetd);

    utcLocalz = localzd.toTimeSpec(utcSpec);
    QVERIFY(utcLocalz.isUtc());
    QVERIFY(utcLocalz.isDateOnly());
    QVERIFY(utcLocalz == KADateTime(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::UTC)));
    QVERIFY(utcLocalz != localzd);

    utcUtc = utcd.toTimeSpec(utcSpec);
    QVERIFY(utcUtc.isUtc());
    QVERIFY(utcUtc.isDateOnly());
    QVERIFY(utcUtc == KADateTime(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::UTC)));
    QVERIFY(utcUtc == utcd);

    // To Zone
    zoneZone = zoned.toTimeSpec(cairoSpec);
    QVERIFY(zoneZone.isDateOnly());
    QCOMPARE(zoneZone.timeZone(), cairo);
    QVERIFY(zoneZone == KADateTime(QDate(2005, 7, 1), cairo));
    QVERIFY(zoneZone != zoned);

    zoneOffset = offsetd.toTimeSpec(cairoSpec);
    QVERIFY(zoneOffset.isDateOnly());
    QCOMPARE(zoneOffset.timeZone(), cairo);
    QVERIFY(zoneOffset == KADateTime(QDate(2005, 6, 6), cairo));
    QVERIFY(zoneOffset != offsetd);

    zoneLocalz = localzd.toTimeSpec(cairoSpec);
    QVERIFY(zoneLocalz.isDateOnly());
    QCOMPARE(zoneLocalz.timeZone(), cairo);
    QVERIFY(zoneLocalz == KADateTime(QDate(2005, 6, 6), cairo));
    QVERIFY(zoneLocalz != localzd);

    zoneUtc = utcd.toTimeSpec(cairoSpec);
    QVERIFY(zoneUtc.isDateOnly());
    QCOMPARE(zoneUtc.timeZone(), cairo);
    QVERIFY(zoneUtc == KADateTime(QDate(2005, 6, 6), cairo));
    QVERIFY(zoneUtc != utcd);

    // To UTC offset
    offsetZone = zoned.toTimeSpec(offset1200Spec);
    QVERIFY(offsetZone.isOffsetFromUtc());
    QVERIFY(offsetZone.isDateOnly());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetZone == KADateTime(QDate(2005, 7, 1), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(offsetZone != zoned);

    offsetOffset = offsetd.toTimeSpec(offset1200Spec);
    QVERIFY(offsetOffset.isOffsetFromUtc());
    QVERIFY(offsetOffset.isDateOnly());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetOffset == KADateTime(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(offsetOffset != offsetd);

    offsetLocalz = localzd.toTimeSpec(offset1200Spec);
    QVERIFY(offsetLocalz.isOffsetFromUtc());
    QVERIFY(offsetLocalz.isDateOnly());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetLocalz == KADateTime(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(offsetLocalz != localzd);

    offsetUtc = utcd.toTimeSpec(offset1200Spec);
    QVERIFY(offsetUtc.isOffsetFromUtc());
    QVERIFY(offsetUtc.isDateOnly());
    QCOMPARE(offsetZone.utcOffset(), 1200);
    QVERIFY(offsetUtc == KADateTime(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(1200)));
    QVERIFY(offsetUtc != utcd);

    // To Local time
    localzZone = zoned.toTimeSpec(localzSpec);
    QVERIFY(localzZone.isLocalZone());
    QVERIFY(localzZone.isDateOnly());
    QVERIFY(localzZone == KADateTime(QDate(2005, 7, 1), KADateTime::Spec(KADateTime::LocalZone)));
    QVERIFY(localzZone != zoned);

    localzOffset = offsetd.toTimeSpec(localzSpec);
    QVERIFY(localzOffset.isLocalZone());
    QVERIFY(localzOffset.isDateOnly());
    QVERIFY(localzOffset == KADateTime(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::LocalZone)));
    QVERIFY(localzOffset != offsetd);

    localzLocalz = localzd.toTimeSpec(localzSpec);
    QVERIFY(localzLocalz.isLocalZone());
    QVERIFY(localzLocalz.isDateOnly());
    QVERIFY(localzLocalz == KADateTime(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::LocalZone)));
    QVERIFY(localzLocalz == localzd);

    localzUtc = utcd.toTimeSpec(localzSpec);
    QVERIFY(localzUtc.isLocalZone());
    QVERIFY(localzUtc.isDateOnly());
    QVERIFY(localzUtc == KADateTime(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::LocalZone)));
    QVERIFY(localzUtc != utcd);

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

////////////////////////////////////////////////////////////////////////
// Set methods: setDate(), setTime(), setTimeSpec()
////////////////////////////////////////////////////////////////////////

void KADateTimeTest::set()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Zone
    KADateTime zoned(QDate(2005, 6, 1), london);
    zoned.setDate(QDate(2004, 5, 2));
    QVERIFY(zoned.isDateOnly());
    QCOMPARE(zoned.date(), QDate(2004, 5, 2));
    QCOMPARE(zoned.time(), QTime(0, 0, 0));
    zoned.setTime(QTime(12, 13, 14));
    QVERIFY(!zoned.isDateOnly());
    QCOMPARE(zoned.date(), QDate(2004, 5, 2));
    QCOMPARE(zoned.time(), QTime(12, 13, 14));
    zoned.setDate(QDate(2004, 5, 4));
    QVERIFY(!zoned.isDateOnly());

    zoned.setDateOnly(false);
    QVERIFY(!zoned.isDateOnly());
    QCOMPARE(zoned.date(), QDate(2004, 5, 4));
    QCOMPARE(zoned.time(), QTime(12, 13, 14));
    zoned.setDateOnly(true);
    QVERIFY(zoned.isDateOnly());
    QCOMPARE(zoned.date(), QDate(2004, 5, 4));
    QCOMPARE(zoned.time(), QTime(0, 0, 0));
    zoned.setDateOnly(false);
    QVERIFY(!zoned.isDateOnly());
    QCOMPARE(zoned.date(), QDate(2004, 5, 4));
    QCOMPARE(zoned.time(), QTime(0, 0, 0));

    KADateTime zone(QDate(2005, 6, 1), QTime(3, 40, 0), london);
    zone.setDate(QDate(2004, 5, 2));
    QCOMPARE(zone.date(), QDate(2004, 5, 2));
    QCOMPARE(zone.time(), QTime(3, 40, 0));
    zone.setTime(QTime(12, 13, 14));
    QCOMPARE(zone.date(), QDate(2004, 5, 2));
    QCOMPARE(zone.time(), QTime(12, 13, 14));
    zone.setDate(QDate(2003,6,10));
    zone.setTime(QTime(5,6,7));
    QCOMPARE(zone.date(), QDate(2003, 6, 10));
    QCOMPARE(zone.time(), QTime(5, 6, 7));
    QCOMPARE(zone.utcOffset(), 3600);
    QCOMPARE(zone.toUtc().date(), QDate(2003, 6, 10));
    QCOMPARE(zone.toUtc().time(), QTime(4, 6, 7));

    // UTC offset
    KADateTime offsetd(QDate(2005, 6, 6), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    offsetd.setDate(QDate(2004, 5, 2));
    QVERIFY(offsetd.isDateOnly());
    QCOMPARE(offsetd.date(), QDate(2004, 5, 2));
    QCOMPARE(offsetd.time(), QTime(0, 0, 0));
    offsetd.setTime(QTime(12, 13, 14));
    QVERIFY(!offsetd.isDateOnly());
    QCOMPARE(offsetd.date(), QDate(2004, 5, 2));
    QCOMPARE(offsetd.time(), QTime(12, 13, 14));
    offsetd.setDate(QDate(2004, 5, 4));
    QVERIFY(!offsetd.isDateOnly());

    offsetd.setDateOnly(false);
    QVERIFY(!offsetd.isDateOnly());
    QCOMPARE(offsetd.date(), QDate(2004, 5, 4));
    QCOMPARE(offsetd.time(), QTime(12, 13, 14));
    offsetd.setDateOnly(true);
    QVERIFY(offsetd.isDateOnly());
    QCOMPARE(offsetd.date(), QDate(2004, 5, 4));
    QCOMPARE(offsetd.time(), QTime(0, 0, 0));
    offsetd.setDateOnly(false);
    QVERIFY(!offsetd.isDateOnly());
    QCOMPARE(offsetd.date(), QDate(2004, 5, 4));
    QCOMPARE(offsetd.time(), QTime(0, 0, 0));

    KADateTime offset(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    offset.setDate(QDate(2004, 5, 2));
    QCOMPARE(offset.date(), QDate(2004, 5, 2));
    QCOMPARE(offset.time(), QTime(1, 2, 30));
    offset.setTime(QTime(12, 13, 14));
    QCOMPARE(offset.date(), QDate(2004, 5, 2));
    QCOMPARE(offset.time(), QTime(12, 13, 14));
    offset.setDate(QDate(2003, 12, 10));
    offset.setTime(QTime(5, 6, 7));
    QCOMPARE(offset.date(), QDate(2003, 12, 10));
    QCOMPARE(offset.time(), QTime(5, 6, 7));
    QCOMPARE(offset.utcOffset(), -5400);
    QCOMPARE(offset.toUtc().date(), QDate(2003, 12, 10));
    QCOMPARE(offset.toUtc().time(), QTime(6, 36, 7));

    // Local time
    KADateTime localzd(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::LocalZone));
    localzd.setDate(QDate(2004, 5, 2));
    QVERIFY(localzd.isDateOnly());
    QCOMPARE(localzd.date(), QDate(2004, 5, 2));
    QCOMPARE(localzd.time(), QTime(0, 0, 0));
    localzd.setTime(QTime(12, 13, 14));
    QVERIFY(!localzd.isDateOnly());
    QCOMPARE(localzd.date(), QDate(2004, 5, 2));
    QCOMPARE(localzd.time(), QTime(12, 13, 14));
    localzd.setDate(QDate(2004, 5, 4));
    QVERIFY(!localzd.isDateOnly());

    localzd.setDateOnly(false);
    QVERIFY(!localzd.isDateOnly());
    QCOMPARE(localzd.date(), QDate(2004, 5, 4));
    QCOMPARE(localzd.time(), QTime(12, 13, 14));
    localzd.setDateOnly(true);
    QVERIFY(localzd.isDateOnly());
    QCOMPARE(localzd.date(), QDate(2004, 5, 4));
    QCOMPARE(localzd.time(), QTime(0, 0, 0));
    localzd.setDateOnly(false);
    QVERIFY(!localzd.isDateOnly());
    QCOMPARE(localzd.date(), QDate(2004, 5, 4));
    QCOMPARE(localzd.time(), QTime(0, 0, 0));

    KADateTime localz(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::LocalZone);
    localz.setDate(QDate(2004, 5, 2));
    QCOMPARE(localz.date(), QDate(2004, 5, 2));
    QCOMPARE(localz.time(), QTime(1, 2, 30));
    localz.setTime(QTime(12, 13, 14));
    QCOMPARE(localz.date(), QDate(2004, 5, 2));
    QCOMPARE(localz.time(), QTime(12, 13, 14));
    localz.setDate(QDate(2003, 12, 10));
    localz.setTime(QTime(5, 6, 7));
    QCOMPARE(localz.date(), QDate(2003, 12, 10));
    QCOMPARE(localz.time(), QTime(5, 6, 7));
    QCOMPARE(localz.utcOffset(), -8 * 3600);
    QCOMPARE(localz.toUtc().date(), QDate(2003, 12, 10));
    QCOMPARE(localz.toUtc().time(), QTime(13, 6, 7));

    // UTC
    KADateTime utcd(QDate(2005, 6, 6), KADateTime::Spec(KADateTime::UTC));
    utcd.setDate(QDate(2004, 5, 2));
    QVERIFY(utcd.isDateOnly());
    QCOMPARE(utcd.date(), QDate(2004, 5, 2));
    QCOMPARE(utcd.time(), QTime(0, 0, 0));
    utcd.setTime(QTime(12, 13, 14));
    QVERIFY(!utcd.isDateOnly());
    QCOMPARE(utcd.date(), QDate(2004, 5, 2));
    QCOMPARE(utcd.time(), QTime(12, 13, 14));
    utcd.setDate(QDate(2004, 5, 4));
    QVERIFY(!utcd.isDateOnly());

    utcd.setDateOnly(false);
    QVERIFY(!utcd.isDateOnly());
    QCOMPARE(utcd.date(), QDate(2004, 5, 4));
    QCOMPARE(utcd.time(), QTime(12, 13, 14));
    utcd.setDateOnly(true);
    QVERIFY(utcd.isDateOnly());
    QCOMPARE(utcd.date(), QDate(2004, 5, 4));
    QCOMPARE(utcd.time(), QTime(0, 0, 0));
    utcd.setDateOnly(false);
    QVERIFY(!utcd.isDateOnly());
    QCOMPARE(utcd.date(), QDate(2004, 5, 4));
    QCOMPARE(utcd.time(), QTime(0, 0, 0));

    KADateTime utc(QDate(2005, 6, 6), QTime(1, 2, 30), KADateTime::UTC);
    utc.setDate(QDate(2004, 5, 2));
    QCOMPARE(utc.date(), QDate(2004, 5, 2));
    QCOMPARE(utc.time(), QTime(1, 2, 30));
    utc.setTime(QTime(12, 13, 14));
    QCOMPARE(utc.date(), QDate(2004, 5, 2));
    QCOMPARE(utc.time(), QTime(12, 13, 14));
    utc.setDate(QDate(2003, 12, 10));
    utc.setTime(QTime(5, 6, 7));
    QCOMPARE(utc.utcOffset(), 0);
    QCOMPARE(utc.date(), QDate(2003, 12, 10));
    QCOMPARE(utc.time(), QTime(5, 6, 7));

    // setTimeSpec(SpecType)
    QCOMPARE(zone.date(), QDate(2003, 6, 10));
    QCOMPARE(zone.time(), QTime(5, 6, 7));
    zone.setTimeSpec(KADateTime::Spec::OffsetFromUTC(7200));
    QVERIFY(zone.isOffsetFromUtc());
    QCOMPARE(zone.utcOffset(), 7200);
    QCOMPARE(zone.toUtc().date(), QDate(2003, 6, 10));
    QCOMPARE(zone.toUtc().time(), QTime(3, 6, 7));
    zone.setTimeSpec(KADateTime::LocalZone);
    QVERIFY(zone.isLocalZone());
    QCOMPARE(zone.utcOffset(), -7 * 3600);
    QCOMPARE(zone.toUtc().date(), QDate(2003, 6, 10));
    QCOMPARE(zone.toUtc().time(), QTime(12, 6, 7));
    zone.setTimeSpec(KADateTime::UTC);
    QVERIFY(zone.isUtc());
    QCOMPARE(zone.utcOffset(), 0);
    QCOMPARE(zone.date(), QDate(2003, 6, 10));
    QCOMPARE(zone.time(), QTime(5, 6, 7));

    // setTimeSpec(KADateTime::Spec)
    QCOMPARE(zone.date(), QDate(2003, 6, 10));
    QCOMPARE(zone.time(), QTime(5, 6, 7));
    zone.setTimeSpec(offset.timeSpec());
    QVERIFY(zone.isOffsetFromUtc());
    QCOMPARE(zone.toUtc().date(), QDate(2003, 6, 10));
    QCOMPARE(zone.toUtc().time(), QTime(6, 36, 7));
    QVERIFY(zone.timeSpec() == offset.timeSpec());
    zone.setTimeSpec(KADateTime::LocalZone);
    QVERIFY(zone.isLocalZone());
    QCOMPARE(zone.toUtc().date(), QDate(2003, 6, 10));
    QCOMPARE(zone.toUtc().time(), QTime(12, 6, 7));
    zone.setTimeSpec(utc.timeSpec());
    QVERIFY(zone.isUtc());
    QCOMPARE(zone.date(), QDate(2003, 6, 10));
    QCOMPARE(zone.time(), QTime(5, 6, 7));
    zone.setTimeSpec(london);
    QCOMPARE(zone.timeZone(), london);
    QCOMPARE(zone.utcOffset(), 3600);
    QCOMPARE(zone.toUtc().date(), QDate(2003, 6, 10));
    QCOMPARE(zone.toUtc().time(), QTime(4, 6, 7));

    // time_t
    utcd = KADateTime(QDate(2005, 6, 6), QTime(12, 15, 20), KADateTime::UTC);
    QDateTime qtt = utcd.qDateTime();
    qint64 secs = qtt.toSecsSinceEpoch();
    KADateTime tt;
    tt.setSecsSinceEpoch(secs);
    QVERIFY(tt.isUtc());
    QCOMPARE(tt.date(), utcd.date());
    QCOMPARE(tt.time(), utcd.time());
    QCOMPARE(tt.toSecsSinceEpoch(), secs);

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

/////////////////////////////////////////////////////////
// operator==()
/////////////////////////////////////////////////////////

void KADateTimeTest::equal()
{
    QTimeZone london("Europe/London");
    QTimeZone cairo("Africa/Cairo");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Date/time values
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 2, 28), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 2, 29), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 2), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 3), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)));
    QVERIFY(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 2, 28), QTime(3, 45, 3), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 2, 29), QTime(3, 45, 3), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 2), QTime(3, 45, 3), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 3), QTime(3, 45, 3), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)));
    QVERIFY(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 1), QTime(17, 45, 2), KADateTime::LocalZone));

    // Date/time : date-only
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo) == KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo) == KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo) == KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo) == KADateTime(QDate(2004, 3, 2), cairo)));

    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo) == KADateTime(QDate(2004, 3, 1), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) == KADateTime(QDate(2004, 3, 1), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo) == KADateTime(QDate(2004, 3, 1), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo) == KADateTime(QDate(2004, 3, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo) == KADateTime(QDate(2004, 3, 2), london)));

    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 1), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(9, 59, 59, 999), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(10, 0, 0), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));

    // Date-only : date/time
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) == KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) == KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo)));

    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) == KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) == KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), london)));

    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)) == KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)) == KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)) == KADateTime(QDate(2004, 3, 3), QTime(9, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)) == KADateTime(QDate(2004, 3, 3), QTime(10, 0, 0), cairo)));

    // Date-only values
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(2 * 3600))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), london) == KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(3 * 3600))));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) == KADateTime(QDate(2004, 3, 1), KADateTime::Spec::OffsetFromUTC(2 * 3600)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) == KADateTime(QDate(2004, 3, 1), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(3 * 3600)) == KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) == KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(2 * 3600)) == KADateTime(QDate(2004, 3, 1), cairo)));
    // Compare days when daylight savings changes occur
    QVERIFY(KADateTime(QDate(2005, 3, 27), london) == KADateTime(QDate(2005, 3, 27), london));
    QVERIFY(!(KADateTime(QDate(2005, 3, 27), london) == KADateTime(QDate(2005, 3, 27), KADateTime::Spec::OffsetFromUTC(0))));
    QVERIFY(KADateTime(QDate(2005, 3, 27), KADateTime::Spec::OffsetFromUTC(0)) == KADateTime(QDate(2005, 3, 27), KADateTime::Spec::OffsetFromUTC(0)));
    QVERIFY(!(KADateTime(QDate(2005, 3, 27), KADateTime::Spec::OffsetFromUTC(0)) == KADateTime(QDate(2005, 3, 27), london)));
    QVERIFY(KADateTime(QDate(2005, 10, 30), KADateTime::Spec(KADateTime::UTC)) == KADateTime(QDate(2005, 10, 30), KADateTime::Spec(KADateTime::UTC)));
    QVERIFY(!(KADateTime(QDate(2005, 10, 30), london) == KADateTime(QDate(2005, 10, 30), KADateTime::Spec(KADateTime::UTC))));
    QVERIFY(!(KADateTime(QDate(2005, 10, 30), KADateTime::Spec(KADateTime::UTC)) == KADateTime(QDate(2005, 10, 30), london)));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

/////////////////////////////////////////////////////////
// operator<()
/////////////////////////////////////////////////////////

void KADateTimeTest::lessThan()
{
    QTimeZone london("Europe/London");
    QTimeZone cairo("Africa/Cairo");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Date/time values
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 2, 28), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 2, 29), QTime(3, 45, 3), cairo)));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 3), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 3), QTime(3, 45, 3), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo) < KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)));
    QVERIFY(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 1), cairo) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 2, 28), QTime(3, 45, 3), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 2, 29), QTime(3, 45, 3), london)));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), london));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 3), london));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 3), QTime(3, 45, 3), london));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), london) < KADateTime(QDate(2004, 2, 28), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), london) < KADateTime(QDate(2004, 2, 29), QTime(3, 45, 3), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), london) < KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo)));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), london) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 3), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), london) < KADateTime(QDate(2004, 3, 3), QTime(3, 45, 3), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC) < KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(17, 45, 2), KADateTime::LocalZone)));
    QVERIFY(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(17, 45, 3), KADateTime::LocalZone));

    // Date/time : date-only
    QVERIFY(KADateTime(QDate(2004, 2, 29), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo) < KADateTime(QDate(2004, 3, 2), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(23, 59, 59, 999), cairo) < KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 4), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 5), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), cairo)));

    QVERIFY(KADateTime(QDate(2004, 2, 29), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), london));
    QVERIFY(KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), london));
    QVERIFY(KADateTime(QDate(2004, 3, 2), QTime(1, 59, 59, 999), cairo) < KADateTime(QDate(2004, 3, 2), london));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(2, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo) < KADateTime(QDate(2004, 3, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(23, 59, 59, 999), cairo) < KADateTime(QDate(2004, 3, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 4), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 5), QTime(0, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), london)));

    QVERIFY(KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 1), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(9, 59, 59, 999), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), QTime(10, 0, 0), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))));

    // Date-only : date/time
    QVERIFY(KADateTime(QDate(2004, 2, 28), cairo) < KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo));
    QVERIFY(KADateTime(QDate(2004, 2, 29), cairo) < KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(23, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), cairo) < KADateTime(QDate(2004, 3, 2), QTime(23, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 4), cairo) < KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 5), cairo) < KADateTime(QDate(2004, 3, 2), QTime(0, 0, 0), cairo)));

    QVERIFY(KADateTime(QDate(2004, 2, 28), cairo) < KADateTime(QDate(2004, 3, 1), QTime(22, 0, 0), london));
    QVERIFY(KADateTime(QDate(2004, 2, 29), cairo) < KADateTime(QDate(2004, 3, 1), QTime(22, 0, 0), london));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 1), QTime(22, 0, 0), london));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(22, 0, 0), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 1), QTime(21, 59, 59, 999), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), cairo) < KADateTime(QDate(2004, 3, 1), QTime(21, 59, 59, 999), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 4), cairo) < KADateTime(QDate(2004, 3, 1), QTime(22, 0, 0), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 5), cairo) < KADateTime(QDate(2004, 3, 1), QTime(22, 0, 0), london)));

    QVERIFY(KADateTime(QDate(2004, 2, 28), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo));
    QVERIFY(KADateTime(QDate(2004, 2, 29), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo)));
    QVERIFY(KADateTime(QDate(2004, 3, 1), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 3), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 6), KADateTime::Spec(KADateTime::LocalZone)) < KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo)));

    // Date-only values
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 2), cairo));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(2 * 3600)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), london) < KADateTime(QDate(2004, 3, 2), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(3 * 3600))));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 3), KADateTime::Spec::OffsetFromUTC(3 * 3600)));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 4), KADateTime::Spec::OffsetFromUTC(3 * 3600)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 1), KADateTime::Spec::OffsetFromUTC(2 * 3600))));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 5), london));
    QVERIFY(KADateTime(QDate(2004, 3, 1), cairo) < KADateTime(QDate(2004, 3, 2), london));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 1), london)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(3 * 3600)) < KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), cairo) < KADateTime(QDate(2004, 3, 1), cairo)));
    QVERIFY(!(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(2 * 3600)) < KADateTime(QDate(2004, 3, 1), cairo)));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

/////////////////////////////////////////////////////////
// compare()
/////////////////////////////////////////////////////////

void KADateTimeTest::compare()
{
    QTimeZone london("Europe/London");
    QTimeZone cairo("Africa/Cairo");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Date/time values
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo)), KADateTime::Before);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC)), KADateTime::Before);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), KADateTime::UTC).compare(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)), KADateTime::After);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 3), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)), KADateTime::After);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(17, 45, 2), KADateTime::LocalZone)), KADateTime::Equal);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo).compare(KADateTime(QDate(2004, 3, 2), QTime(3, 45, 2), cairo)), KADateTime::Equal);

    // Date/time : date-only
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo).compare(KADateTime(QDate(2004, 3, 1), cairo)), KADateTime::AtStart);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), cairo)), KADateTime::Inside);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo).compare(KADateTime(QDate(2004, 3, 1), cairo)), KADateTime::AtEnd);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo).compare(KADateTime(QDate(2004, 3, 2), cairo)), KADateTime::Before);
    QCOMPARE(KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo).compare(KADateTime(QDate(2004, 3, 2), cairo)), KADateTime::After);

    QCOMPARE(KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))), KADateTime::Before);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))), KADateTime::AtStart);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 1), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))), KADateTime::Inside);
    QCOMPARE(KADateTime(QDate(2004, 3, 3), QTime(9, 59, 59, 999), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))), KADateTime::AtEnd);
    QCOMPARE(KADateTime(QDate(2004, 3, 3), QTime(10, 0, 0), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone))), KADateTime::After);

    // Date-only : date/time
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(0, 0, 0), cairo)), KADateTime::StartsAt);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(3, 45, 2), cairo)), KADateTime::Outside);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo)), KADateTime::EndsAt);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), QTime(23, 59, 59, 999), cairo)), KADateTime::After);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), cairo).compare(KADateTime(QDate(2004, 3, 3), QTime(0, 0, 0), cairo)), KADateTime::Before);

    QCOMPARE(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)).compare(KADateTime(QDate(2004, 3, 2), QTime(9, 59, 59, 999), cairo)), KADateTime::After);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)).compare(KADateTime(QDate(2004, 3, 2), QTime(10, 0, 0), cairo)), KADateTime::StartsAt);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)).compare(KADateTime(QDate(2004, 3, 3), QTime(9, 59, 59, 999), cairo)), KADateTime::EndsAt);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), KADateTime::Spec(KADateTime::LocalZone)).compare(KADateTime(QDate(2004, 3, 3), QTime(10, 0, 0), cairo)), KADateTime::Before);

    // Date-only values
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 2), cairo)), KADateTime::Before);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(2 * 3600))), KADateTime::Before);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), london).compare(KADateTime(QDate(2004, 3, 2), cairo)), static_cast<KADateTime::Comparison>(KADateTime::Before | KADateTime::AtStart | KADateTime::Inside));
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(3 * 3600))), static_cast<KADateTime::Comparison>(KADateTime::Before | KADateTime::AtStart | KADateTime::Inside));
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 1), cairo)), KADateTime::Equal);
    QCOMPARE(KADateTime(QDate(2004, 3, 1), cairo).compare(KADateTime(QDate(2004, 3, 1), KADateTime::Spec::OffsetFromUTC(2 * 3600))), KADateTime::Equal);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), london)), static_cast<KADateTime::Comparison>(KADateTime::Inside | KADateTime::AtEnd | KADateTime::After));
    QCOMPARE(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(3 * 3600)).compare(KADateTime(QDate(2004, 3, 1), cairo)), static_cast<KADateTime::Comparison>(KADateTime::Inside | KADateTime::AtEnd | KADateTime::After));
    QCOMPARE(KADateTime(QDate(2004, 3, 2), cairo).compare(KADateTime(QDate(2004, 3, 1), cairo)), KADateTime::After);
    QCOMPARE(KADateTime(QDate(2004, 3, 2), KADateTime::Spec::OffsetFromUTC(2 * 3600)).compare(KADateTime(QDate(2004, 3, 1), cairo)), KADateTime::After);
    // Compare days when daylight savings changes occur
    QCOMPARE(KADateTime(QDate(2005, 3, 27), london).compare(KADateTime(QDate(2005, 3, 27), KADateTime::Spec::OffsetFromUTC(0))), static_cast<KADateTime::Comparison>(KADateTime::AtStart | KADateTime::Inside));
    QCOMPARE(KADateTime(QDate(2005, 3, 27), KADateTime::Spec::OffsetFromUTC(0)).compare(KADateTime(QDate(2005, 3, 27), london)), KADateTime::StartsAt);
    QCOMPARE(KADateTime(QDate(2005, 10, 30), london).compare(KADateTime(QDate(2005, 10, 30), KADateTime::Spec(KADateTime::UTC))), KADateTime::EndsAt);
    QCOMPARE(KADateTime(QDate(2005, 10, 30), KADateTime::Spec(KADateTime::UTC)).compare(KADateTime(QDate(2005, 10, 30), london)), static_cast<KADateTime::Comparison>(KADateTime::Inside | KADateTime::AtEnd));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

/////////////////////////////////////////////////////////
// Addition and subtraction methods, and operator<() etc.
/////////////////////////////////////////////////////////

void KADateTimeTest::addSubtract()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // UTC
    KADateTime utc1(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::UTC);
    KADateTime utc2 = utc1.addSecs(184 * 86400);
    QVERIFY(utc2.isUtc());
    QCOMPARE(utc2.date(), QDate(2006, 1, 6));
    QCOMPARE(utc2.time(), QTime(3, 40, 0));
    KADateTime utc3 = utc1.addDays(184);
    QVERIFY(utc3.isUtc());
    QCOMPARE(utc2.date(), utc3.date());
    QCOMPARE(utc2.time(), utc3.time());
    KADateTime utc4 = utc1.addMonths(6);
    QVERIFY(utc4.isUtc());
    QCOMPARE(utc2.date(), utc4.date());
    QCOMPARE(utc2.time(), utc4.time());
    KADateTime utc5 = utc1.addYears(4);
    QVERIFY(utc5.isUtc());
    QCOMPARE(utc5.date(), QDate(2009, 7, 6));
    QCOMPARE(utc5.time(), QTime(3, 40, 0));
    QCOMPARE(utc1.secsTo(utc2), 184 * 86400);
    QCOMPARE(utc1.secsTo(utc3), 184 * 86400);
    QCOMPARE(utc1.daysTo(utc2), 184);
    QVERIFY(utc1 < utc2);
    QVERIFY(!(utc2 < utc1));
    QVERIFY(utc2 == utc3);

    // UTC offset
    KADateTime offset1(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime offset2 = offset1.addSecs(184 * 86400);
    QVERIFY(offset2.isOffsetFromUtc());
    QCOMPARE(offset2.utcOffset(), -5400);
    QCOMPARE(offset2.date(), QDate(2006, 1, 6));
    QCOMPARE(offset2.time(), QTime(3, 40, 0));
    KADateTime offset3 = offset1.addDays(184);
    QVERIFY(offset3.isOffsetFromUtc());
    QCOMPARE(offset3.utcOffset(), -5400);
    QCOMPARE(offset2.date(), offset3.date());
    QCOMPARE(offset2.time(), offset3.time());
    KADateTime offset4 = offset1.addMonths(6);
    QVERIFY(offset4.isOffsetFromUtc());
    QCOMPARE(offset4.utcOffset(), -5400);
    QCOMPARE(offset2.date(), offset4.date());
    QCOMPARE(offset2.time(), offset4.time());
    KADateTime offset5 = offset1.addYears(4);
    QVERIFY(offset5.isOffsetFromUtc());
    QCOMPARE(offset5.utcOffset(), -5400);
    QCOMPARE(offset5.date(), QDate(2009, 7, 6));
    QCOMPARE(offset5.time(), QTime(3, 40, 0));
    QCOMPARE(offset1.secsTo(offset2), 184 * 86400);
    QCOMPARE(offset1.secsTo(offset3), 184 * 86400);
    QCOMPARE(offset1.daysTo(offset2), 184);
    QVERIFY(offset1 < offset2);
    QVERIFY(!(offset2 < offset1));
    QVERIFY(offset2 == offset3);

    // Zone
    KADateTime zone1(QDate(2005, 7, 6), QTime(3, 40, 0), london);
    KADateTime zone2 = zone1.addSecs(184 * 86400);
    QCOMPARE(zone2.timeZone(), london);
    QCOMPARE(zone2.date(), QDate(2006, 1, 6));
    QCOMPARE(zone2.time(), QTime(2, 40, 0));
    KADateTime zone3 = zone1.addDays(184);
    QCOMPARE(zone3.timeZone(), london);
    QCOMPARE(zone3.date(), QDate(2006, 1, 6));
    QCOMPARE(zone3.time(), QTime(3, 40, 0));
    KADateTime zone4 = zone1.addMonths(6);
    QCOMPARE(zone4.timeZone(), london);
    QCOMPARE(zone4.date(), zone3.date());
    QCOMPARE(zone4.time(), zone3.time());
    KADateTime zone5 = zone1.addYears(4);
    QCOMPARE(zone5.timeZone(), london);
    QCOMPARE(zone5.date(), QDate(2009, 7, 6));
    QCOMPARE(zone5.time(), QTime(3, 40, 0));
    QCOMPARE(zone1.secsTo(zone2), 184 * 86400);
    QCOMPARE(zone1.secsTo(zone3), 184 * 86400 + 3600);
    QCOMPARE(zone1.daysTo(zone2), 184);
    QCOMPARE(zone1.daysTo(zone3), 184);
    QVERIFY(zone1 < zone2);
    QVERIFY(!(zone2 < zone1));
    QVERIFY(!(zone2 == zone3));

    // Local zone
    KADateTime local1(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::LocalZone);
    KADateTime local2 = local1.addSecs(184 * 86400);
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.timeZone(), losAngeles);
    QCOMPARE(local2.date(), QDate(2006, 1, 6));
    QCOMPARE(local2.time(), QTime(2, 40, 0));
    KADateTime local3 = local1.addDays(184);
    QVERIFY(local3.isLocalZone());
    QCOMPARE(local3.date(), QDate(2006, 1, 6));
    QCOMPARE(local3.time(), QTime(3, 40, 0));
    KADateTime local4 = local1.addMonths(6);
    QVERIFY(local4.isLocalZone());
    QCOMPARE(local4.date(), local3.date());
    QCOMPARE(local4.time(), local3.time());
    KADateTime local5 = local1.addYears(4);
    QVERIFY(local5.isLocalZone());
    QCOMPARE(local5.date(), QDate(2009, 7, 6));
    QCOMPARE(local5.time(), QTime(3, 40, 0));
    QCOMPARE(local1.secsTo(local2), 184 * 86400);
    QCOMPARE(local1.secsTo(local3), 184 * 86400 + 3600);
    QCOMPARE(local1.daysTo(local2), 184);
    QCOMPARE(local1.daysTo(local3), 184);
    QVERIFY(local1 < local2);
    QVERIFY(!(local2 < local1));
    QVERIFY(!(local2 == local3));

    // Mixed timeSpecs
    QCOMPARE(utc1.secsTo(offset1), 5400);
    QCOMPARE(utc1.secsTo(offset2), 184 * 86400 + 5400);
    QCOMPARE(offset2.secsTo(utc1), -(184 * 86400 + 5400));
    QVERIFY(utc1 < offset1);
    QVERIFY(utc1 <= offset1);
    QVERIFY(!(offset1 < utc1));
    QVERIFY(!(offset1 <= utc1));
    QCOMPARE(utc1.secsTo(zone1), -3600);
    QCOMPARE(utc1.secsTo(zone2), 184 * 86400 - 3600);
    QCOMPARE(zone2.secsTo(utc1), -(184 * 86400 - 3600));
    QVERIFY(utc1 > zone1);
    QVERIFY(utc1 >= zone1);
    QVERIFY(!(zone1 > utc1));
    QVERIFY(!(zone1 >= utc1));
    QCOMPARE(utc1.secsTo(local1), 7 * 3600);
    QCOMPARE(utc1.secsTo(local2), 184 * 86400 + 7 * 3600);
    QCOMPARE(local2.secsTo(utc1), -(184 * 86400 + 7 * 3600));
    QVERIFY(utc1 < local1);
    QVERIFY(utc1 <= local1);
    QVERIFY(!(local1 < utc1));
    QVERIFY(!(local1 <= utc1));

    QCOMPARE(offset1.secsTo(zone1), -9000);
    QCOMPARE(offset1.secsTo(zone2), 184 * 86400 - 9000);
    QCOMPARE(zone2.secsTo(offset1), -(184 * 86400 - 9000));
    QVERIFY(offset1 > zone1);
    QVERIFY(offset1 >= zone1);
    QVERIFY(!(zone1 > offset1));
    QVERIFY(!(zone1 >= offset1));
    QCOMPARE(offset1.secsTo(local1), 7 * 3600 - 5400);
    QCOMPARE(offset1.secsTo(local2), 184 * 86400 + 7 * 3600 - 5400);
    QCOMPARE(local2.secsTo(offset1), -(184 * 86400 + 7 * 3600 - 5400));
    QVERIFY(offset1 < local1);
    QVERIFY(offset1 <= local1);
    QVERIFY(!(local1 < offset1));
    QVERIFY(!(local1 <= offset1));

    QCOMPARE(zone1.secsTo(local1), 8 * 3600);
    QCOMPARE(zone1.secsTo(local2), 184 * 86400 + 8 * 3600);
    QCOMPARE(local2.secsTo(zone1), -(184 * 86400 + 8 * 3600));
    QVERIFY(zone1 < local1);
    QVERIFY(zone1 <= local1);
    QVERIFY(!(local1 < zone1));
    QVERIFY(!(local1 <= zone1));

    KADateTime dt(QDate(1998, 3, 1), QTime(0, 0), QTimeZone("America/New_York"));
    const KADateTime dtF = dt.addMonths(1);
    while (dt < dtF)
    {
        if (!dt.addSecs(1200).isValid())
        {
            qDebug() << "Last valid date" << dt.toString();   // print the value which fails
            QVERIFY(dt.addSecs(1200).isValid());   // now fail the test
            break;
        }
        dt = dt.addSecs(1200);
    }

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::addMSecs()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // UTC
    KADateTime utc1(QDate(2005, 7, 6), QTime(23, 59, 0, 100), KADateTime::UTC);
    KADateTime utc2 = utc1.addMSecs(59899);
    QVERIFY(utc2.isUtc());
    QCOMPARE(utc2.date(), QDate(2005, 7, 6));
    QCOMPARE(utc2.time(), QTime(23, 59, 59, 999));
    QCOMPARE(utc1.msecsTo(utc2), 59899);
    utc2 = utc1.addMSecs(59900);
    QVERIFY(utc2.isUtc());
    QCOMPARE(utc2.date(), QDate(2005, 7, 7));
    QCOMPARE(utc2.time(), QTime(0, 0, 0, 0));
    QCOMPARE(utc1.msecsTo(utc2), 59900);
    KADateTime utc1a(QDate(2005, 7, 6), QTime(0, 0, 5, 100), KADateTime::UTC);
    utc2 = utc1a.addMSecs(-5100);
    QVERIFY(utc2.isUtc());
    QCOMPARE(utc2.date(), QDate(2005, 7, 6));
    QCOMPARE(utc2.time(), QTime(0, 0, 0, 0));
    QCOMPARE(utc1a.msecsTo(utc2), -5100);
    utc2 = utc1a.addMSecs(-5101);
    QVERIFY(utc2.isUtc());
    QCOMPARE(utc2.date(), QDate(2005, 7, 5));
    QCOMPARE(utc2.time(), QTime(23, 59, 59, 999));
    QCOMPARE(utc1a.msecsTo(utc2), -5101);

    // UTC offset
    KADateTime offset1(QDate(2005, 7, 6), QTime(3, 40, 0, 100), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime offset2 = offset1.addMSecs(5899);
    QVERIFY(offset2.isOffsetFromUtc());
    QCOMPARE(offset2.utcOffset(), -5400);
    QCOMPARE(offset2.date(), QDate(2005, 7, 6));
    QCOMPARE(offset2.time(), QTime(3, 40, 5, 999));
    offset2 = offset1.addMSecs(5900);
    QVERIFY(offset2.isOffsetFromUtc());
    QCOMPARE(offset2.utcOffset(), -5400);
    QCOMPARE(offset2.date(), QDate(2005, 7, 6));
    QCOMPARE(offset2.time(), QTime(3, 40, 6, 0));
    offset2 = offset1.addMSecs(-5100);
    QVERIFY(offset2.isOffsetFromUtc());
    QCOMPARE(offset2.utcOffset(), -5400);
    QCOMPARE(offset2.date(), QDate(2005, 7, 6));
    QCOMPARE(offset2.time(), QTime(3, 39, 55, 0));
    offset2 = offset1.addMSecs(-5101);
    QVERIFY(offset2.isOffsetFromUtc());
    QCOMPARE(offset2.utcOffset(), -5400);
    QCOMPARE(offset2.date(), QDate(2005, 7, 6));
    QCOMPARE(offset2.time(), QTime(3, 39, 54, 999));

    // Zone
    KADateTime zone1(QDate(2002, 3, 31), QTime(0, 40, 0, 100), london); // time changes at 01:00 UTC
    KADateTime zone2 = zone1.addMSecs(3600 * 1000 + 899);
    QCOMPARE(zone2.timeZone(), london);
    QCOMPARE(zone2.date(), QDate(2002, 3, 31));
    QCOMPARE(zone2.time(), QTime(2, 40, 0, 999));
    zone2 = zone1.addMSecs(3600 * 1000 + 900);
    QCOMPARE(zone2.timeZone(), london);
    QCOMPARE(zone2.date(), QDate(2002, 3, 31));
    QCOMPARE(zone2.time(), QTime(2, 40, 1, 0));
    KADateTime zone1a(QDate(2002, 3, 31), QTime(2, 40, 0, 100), london); // time changes at 01:00 UTC
    zone2 = zone1a.addMSecs(-(3600 * 1000 + 100));
    QCOMPARE(zone2.timeZone(), london);
    QCOMPARE(zone2.date(), QDate(2002, 3, 31));
    QCOMPARE(zone2.time(), QTime(0, 40, 0, 0));
    zone2 = zone1a.addMSecs(-(3600 * 1000 + 101));
    QCOMPARE(zone2.timeZone(), london);
    QCOMPARE(zone2.date(), QDate(2002, 3, 31));
    QCOMPARE(zone2.time(), QTime(0, 39, 59, 999));

    // Local zone
    KADateTime local1(QDate(2002, 4, 7), QTime(1, 59, 0, 100), KADateTime::LocalZone); // time changes at 02:00 local
    KADateTime local2 = local1.addMSecs(59899);
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.timeZone(), losAngeles);
    QCOMPARE(local2.date(), QDate(2002, 4, 7));
    QCOMPARE(local2.time(), QTime(1, 59, 59, 999));
    local2 = local1.addMSecs(59900);
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.timeZone(), losAngeles);
    QCOMPARE(local2.date(), QDate(2002, 4, 7));
    QCOMPARE(local2.time(), QTime(3, 0, 0, 0));
    KADateTime local1a(QDate(2002, 4, 7), QTime(3, 0, 0, 100), KADateTime::LocalZone); // time changes at 02:00 local
    local2 = local1a.addMSecs(-100);
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.timeZone(), losAngeles);
    QCOMPARE(local2.date(), QDate(2002, 4, 7));
    QCOMPARE(local2.time(), QTime(3, 0, 0, 0));
    local2 = local1a.addMSecs(-101);
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.timeZone(), losAngeles);
    QCOMPARE(local2.date(), QDate(2002, 4, 7));
    QCOMPARE(local2.time(), QTime(1, 59, 59, 999));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::addSubtractDate()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // UTC
    KADateTime utc1(QDate(2005, 7, 6), KADateTime::Spec(KADateTime::UTC));
    KADateTime utc2 = utc1.addSecs(184 * 86400 + 100);
    QVERIFY(utc2.isUtc());
    QVERIFY(utc2.isDateOnly());
    QCOMPARE(utc2.date(), QDate(2006, 1, 6));
    QCOMPARE(utc2.time(), QTime(0, 0, 0));
    KADateTime utc3 = utc1.addDays(184);
    QVERIFY(utc3.isUtc());
    QVERIFY(utc3.isDateOnly());
    QCOMPARE(utc2.date(), utc3.date());
    QCOMPARE(utc2.time(), utc3.time());
    KADateTime utc4 = utc1.addMonths(6);
    QVERIFY(utc4.isUtc());
    QVERIFY(utc4.isDateOnly());
    QCOMPARE(utc2.date(), utc4.date());
    QCOMPARE(utc2.time(), utc4.time());
    KADateTime utc5 = utc1.addYears(4);
    QVERIFY(utc5.isUtc());
    QVERIFY(utc5.isDateOnly());
    QCOMPARE(utc5.date(), QDate(2009, 7, 6));
    QCOMPARE(utc5.time(), QTime(0, 0, 0));
    QCOMPARE(utc1.secsTo(utc2), 184 * 86400);
    QCOMPARE(utc1.secsTo(utc3), 184 * 86400);
    QCOMPARE(utc1.daysTo(utc2), 184);
    QVERIFY(utc1 < utc2);
    QVERIFY(!(utc2 < utc1));
    QVERIFY(utc2 == utc3);

    // UTC offset
    KADateTime offset1(QDate(2005, 7, 6), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    KADateTime offset2 = offset1.addSecs(184 * 86400);
    QVERIFY(offset2.isDateOnly());
    QVERIFY(offset2.isOffsetFromUtc());
    QCOMPARE(offset2.utcOffset(), -5400);
    QCOMPARE(offset2.date(), QDate(2006, 1, 6));
    QCOMPARE(offset2.time(), QTime(0, 0, 0));
    KADateTime offset3 = offset1.addDays(184);
    QVERIFY(offset3.isDateOnly());
    QVERIFY(offset3.isOffsetFromUtc());
    QCOMPARE(offset3.utcOffset(), -5400);
    QCOMPARE(offset2.date(), offset3.date());
    QCOMPARE(offset2.time(), offset3.time());
    KADateTime offset4 = offset1.addMonths(6);
    QVERIFY(offset4.isDateOnly());
    QVERIFY(offset4.isOffsetFromUtc());
    QCOMPARE(offset4.utcOffset(), -5400);
    QCOMPARE(offset2.date(), offset4.date());
    QCOMPARE(offset2.time(), offset4.time());
    KADateTime offset5 = offset1.addYears(4);
    QVERIFY(offset5.isDateOnly());
    QVERIFY(offset5.isOffsetFromUtc());
    QCOMPARE(offset5.utcOffset(), -5400);
    QCOMPARE(offset5.date(), QDate(2009, 7, 6));
    QCOMPARE(offset5.time(), QTime(0, 0, 0));
    QCOMPARE(offset1.secsTo(offset2), 184 * 86400);
    QCOMPARE(offset1.secsTo(offset3), 184 * 86400);
    QCOMPARE(offset1.daysTo(offset2), 184);
    QVERIFY(offset1 < offset2);
    QVERIFY(!(offset2 < offset1));
    QVERIFY(offset2 == offset3);

    // Zone
    KADateTime zone1(QDate(2005, 7, 6), london);
    KADateTime zone2 = zone1.addSecs(184 * 86400);
    QVERIFY(zone2.isDateOnly());
    QCOMPARE(zone2.timeZone(), london);
    QCOMPARE(zone2.date(), QDate(2006, 1, 6));
    QCOMPARE(zone2.time(), QTime(0, 0, 0));
    KADateTime zone3 = zone1.addDays(184);
    QVERIFY(zone3.isDateOnly());
    QCOMPARE(zone3.timeZone(), london);
    QCOMPARE(zone3.date(), QDate(2006, 1, 6));
    QCOMPARE(zone3.time(), QTime(0, 0, 0));
    KADateTime zone4 = zone1.addMonths(6);
    QVERIFY(zone4.isDateOnly());
    QCOMPARE(zone4.timeZone(), london);
    QCOMPARE(zone4.date(), zone3.date());
    QCOMPARE(zone4.time(), zone3.time());
    KADateTime zone5 = zone1.addYears(4);
    QVERIFY(zone5.isDateOnly());
    QCOMPARE(zone5.timeZone(), london);
    QCOMPARE(zone5.date(), QDate(2009, 7, 6));
    QCOMPARE(zone5.time(), QTime(0, 0, 0));
    QCOMPARE(zone1.secsTo(zone2), 184 * 86400);
    QCOMPARE(zone1.secsTo(zone3), 184 * 86400);
    QCOMPARE(zone1.daysTo(zone2), 184);
    QCOMPARE(zone1.daysTo(zone3), 184);
    QVERIFY(zone1 < zone2);
    QVERIFY(!(zone2 < zone1));
    QVERIFY(zone2 == zone3);

    // Local zone
    KADateTime local1(QDate(2005, 7, 6), KADateTime::Spec(KADateTime::LocalZone));
    KADateTime local2 = local1.addSecs(184 * 86400);
    QVERIFY(local2.isDateOnly());
    QVERIFY(local2.isLocalZone());
    QCOMPARE(local2.timeZone(), losAngeles);
    QCOMPARE(local2.date(), QDate(2006, 1, 6));
    QCOMPARE(local2.time(), QTime(0, 0, 0));
    KADateTime local3 = local1.addDays(184);
    QVERIFY(local3.isDateOnly());
    QVERIFY(local3.isLocalZone());
    QCOMPARE(local3.date(), QDate(2006, 1, 6));
    QCOMPARE(local3.time(), QTime(0, 0, 0));
    KADateTime local4 = local1.addMonths(6);
    QVERIFY(local4.isDateOnly());
    QVERIFY(local4.isLocalZone());
    QCOMPARE(local4.date(), local3.date());
    QCOMPARE(local4.time(), local3.time());
    KADateTime local5 = local1.addYears(4);
    QVERIFY(local5.isDateOnly());
    QVERIFY(local5.isLocalZone());
    QCOMPARE(local5.date(), QDate(2009, 7, 6));
    QCOMPARE(local5.time(), QTime(0, 0, 0));
    QCOMPARE(local1.secsTo(local2), 184 * 86400);
    QCOMPARE(local1.secsTo(local3), 184 * 86400);
    QCOMPARE(local1.daysTo(local2), 184);
    QCOMPARE(local1.daysTo(local3), 184);
    QVERIFY(local1 < local2);
    QVERIFY(!(local2 < local1));
    QVERIFY(local2 == local3);

    // Mixed timeSpecs
    QCOMPARE(utc1.secsTo(offset1), 0);
    QCOMPARE(utc1.secsTo(offset2), 184 * 86400);
    QCOMPARE(offset2.secsTo(utc1), -(184 * 86400));
    QVERIFY(!(utc1 < offset1));
    QVERIFY(utc1 <= offset1);
    QVERIFY(!(offset1 < utc1));
    QVERIFY(offset1 <= utc1);
    QCOMPARE(utc1.secsTo(zone1), 0);
    QCOMPARE(utc1.secsTo(zone2), 184 * 86400);
    QCOMPARE(zone2.secsTo(utc1), -(184 * 86400));
    QVERIFY(!(utc1 > zone1));
    QVERIFY(utc1 >= zone1);
    QVERIFY(!(zone1 > utc1));
    QVERIFY(zone1 >= utc1);
    QCOMPARE(utc1.secsTo(local1), 0);
    QCOMPARE(utc1.secsTo(local2), 184 * 86400);
    QCOMPARE(local2.secsTo(utc1), -(184 * 86400));
    QVERIFY(!(utc1 < local1));
    QVERIFY(utc1 <= local1);
    QVERIFY(!(local1 < utc1));
    QVERIFY(local1 <= utc1);

    QCOMPARE(offset1.secsTo(zone1), 0);
    QCOMPARE(offset1.secsTo(zone2), 184 * 86400);
    QCOMPARE(zone2.secsTo(offset1), -(184 * 86400));
    QVERIFY(!(offset1 > zone1));
    QVERIFY(offset1 >= zone1);
    QVERIFY(!(zone1 > offset1));
    QVERIFY(zone1 >= offset1);
    QCOMPARE(offset1.secsTo(local1), 0);
    QCOMPARE(offset1.secsTo(local2), 184 * 86400);
    QCOMPARE(local2.secsTo(offset1), -(184 * 86400));
    QVERIFY(!(offset1 < local1));
    QVERIFY(offset1 <= local1);
    QVERIFY(!(local1 < offset1));
    QVERIFY(local1 <= offset1);

    QCOMPARE(zone1.secsTo(local1), 0);
    QCOMPARE(zone1.secsTo(local2), 184 * 86400);
    QCOMPARE(local2.secsTo(zone1), -(184 * 86400));
    QVERIFY(!(zone1 < local1));
    QVERIFY(zone1 <= local1);
    QVERIFY(!(local1 < zone1));
    QVERIFY(local1 <= zone1);

    // Mixed date/time and date-only

    // UTC
    utc3.setTime(QTime(13, 14, 15));
    QVERIFY(!utc3.isDateOnly());
    QCOMPARE(utc3.time(), QTime(13, 14, 15));
    QCOMPARE(utc1.secsTo(utc3), 184 * 86400);

    KADateTime utc1t(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::UTC);
    QCOMPARE(utc1t.secsTo(utc2), 184 * 86400);

    // UTC offset
    offset3.setTime(QTime(13, 14, 15));
    QVERIFY(!offset3.isDateOnly());
    QCOMPARE(offset3.time(), QTime(13, 14, 15));
    QCOMPARE(offset1.secsTo(offset3), 184 * 86400);

    KADateTime offset1t(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::Spec::OffsetFromUTC(-5400)); // -0130
    QCOMPARE(offset1t.secsTo(offset2), 184 * 86400);

    KADateTime offset2t(QDate(2005, 7, 6), QTime(0, 40, 0), KADateTime::Spec::OffsetFromUTC(5400)); // +0130

    // Zone
    zone3.setTime(QTime(13, 14, 15));
    QVERIFY(!zone3.isDateOnly());
    QCOMPARE(zone3.time(), QTime(13, 14, 15));
    QCOMPARE(zone1.secsTo(zone3), 184 * 86400);

    KADateTime zone1t(QDate(2005, 7, 6), QTime(3, 40, 0), london);
    QCOMPARE(zone1t.secsTo(zone2), 184 * 86400);

    // Local zone
    local3.setTime(QTime(13, 14, 15));
    QVERIFY(!local3.isDateOnly());
    QCOMPARE(local3.time(), QTime(13, 14, 15));
    QCOMPARE(local1.secsTo(local3), 184 * 86400);

    KADateTime local1t(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::LocalZone);
    QCOMPARE(local1t.secsTo(local2), 184 * 86400);

    KADateTime local2t(QDate(2005, 7, 5), QTime(23, 40, 0), KADateTime::LocalZone);

    // Mixed timeSpecs
    QCOMPARE(utc1t.secsTo(offset1), 0);
    QVERIFY(utc1t != offset1);
    QVERIFY(offset1 != utc1t);
    QVERIFY(!(utc1t < offset1));
    QVERIFY(utc1t <= offset1);
    QVERIFY(!(offset1 < utc1t));
    QVERIFY(offset1 <= utc1t);
    QCOMPARE(utc1.secsTo(offset2t), -86400);
    QCOMPARE(offset2t.secsTo(utc1), 86400);
    QVERIFY(utc1 != offset2t);
    QVERIFY(offset2t != utc1);
    QVERIFY(utc1 > offset2t);
    QVERIFY(utc1 >= offset2t);
    QVERIFY(offset2t < utc1);
    QVERIFY(offset2t <= utc1);
    QCOMPARE(utc1t.secsTo(offset2), 184 * 86400);
    QCOMPARE(offset2.secsTo(utc1t), -(184 * 86400));
    QCOMPARE(utc1t.secsTo(zone1), 0);
    QVERIFY(utc1t != zone1);
    QVERIFY(zone1 != utc1t);
    QVERIFY(!(utc1t < zone1));
    QVERIFY(!(utc1t > zone1));
    QVERIFY(!(zone1 < utc1t));
    QVERIFY(!(zone1 > utc1t));
    QCOMPARE(utc1t.secsTo(zone2), 184 * 86400);
    QCOMPARE(zone2.secsTo(utc1t), -(184 * 86400));
    QVERIFY(utc1t != zone2);
    QVERIFY(zone2 != utc1t);
    QVERIFY(utc1t < zone2);
    QVERIFY(utc1t <= zone2);
    QVERIFY(!(zone2 < utc1t));
    QVERIFY(!(zone2 <= utc1t));
    QCOMPARE(utc1t.secsTo(local1), 86400);
    QCOMPARE(utc1t.secsTo(local2), 185 * 86400);
    QCOMPARE(local2.secsTo(utc1t), -(185 * 86400));
    QVERIFY(utc1t != local1);
    QVERIFY(local1 != utc1t);
    QVERIFY(utc1t < local1);
    QVERIFY(utc1t <= local1);
    QVERIFY(!(local1 < utc1t));
    QVERIFY(!(local1 <= utc1t));
    QCOMPARE(utc1.secsTo(local2t), 0);
    QCOMPARE(local2t.secsTo(utc1), 0);
    QVERIFY(utc1 != local2t);
    QVERIFY(local2t != utc1);
    QVERIFY(!(utc1 < local2t));
    QVERIFY(utc1 <= local2t);
    QVERIFY(!(local2t < utc1));
    QVERIFY(local2t <= utc1);

    QCOMPARE(offset1t.secsTo(zone1), 0);
    QCOMPARE(offset1t.secsTo(zone2), 184 * 86400);
    QCOMPARE(zone2.secsTo(offset1t), -(184 * 86400));
    QVERIFY(offset1t != zone1);
    QVERIFY(zone1 != offset1t);
    QVERIFY(!(offset1t > zone1));
    QVERIFY(offset1t >= zone1);
    QVERIFY(!(zone1 > offset1t));
    QVERIFY(zone1 >= offset1t);
    QCOMPARE(offset1t.secsTo(local1), 86400);
    QCOMPARE(offset1t.secsTo(local2), 185 * 86400);
    QCOMPARE(local2.secsTo(offset1t), -(185 * 86400));
    QVERIFY(offset1t != local1);
    QVERIFY(local1 != offset1t);
    QVERIFY(offset1t < local1);
    QVERIFY(offset1t <= local1);
    QVERIFY(!(local1 < offset1t));
    QVERIFY(!(local1 <= offset1t));

    QCOMPARE(zone1t.secsTo(local1), 86400);
    QCOMPARE(zone1t.secsTo(local2), 185 * 86400);
    QCOMPARE(local2.secsTo(zone1t), -(185 * 86400));
    QVERIFY(zone1t != local1);
    QVERIFY(local1 != zone1t);
    QVERIFY(zone1t < local1);
    QVERIFY(zone1t <= local1);
    QVERIFY(!(local1 < zone1t));
    QVERIFY(!(local1 <= zone1t));

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

///////////////////////////////////////////
// Tests around daylight saving time shifts
///////////////////////////////////////////

void KADateTimeTest::dstShifts()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Shift from DST to standard time for the UK in 2005 was at 2005-10-30 01:00 UTC.
    QDateTime qdt(QDate(2005, 10, 29), QTime(23, 59, 59), Qt::UTC);
    KADateTime dt(qdt, london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(0, 59, 59));
    dt = KADateTime(QDateTime(QDate(2005, 10, 30), QTime(0, 0, 0), Qt::UTC), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 0, 0));
    dt = KADateTime(QDateTime(QDate(2005, 10, 30), QTime(0, 59, 59), Qt::UTC), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 59, 59));
    dt = KADateTime(QDateTime(QDate(2005, 10, 30), QTime(1, 0, 0), Qt::UTC), london);
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 0, 0));
    dt = KADateTime(QDateTime(QDate(2005, 10, 30), QTime(1, 59, 59), Qt::UTC), london);
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 59, 59));
    dt = KADateTime(QDateTime(QDate(2005, 10, 30), QTime(2, 0, 0), Qt::UTC), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(2, 0, 0));

    dt = KADateTime(QDate(2005, 10, 30), QTime(0, 59, 59), london);
    dt.setSecondOccurrence(true);   // this has no effect
    QVERIFY(dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.toUtc().date(), QDate(2005, 10, 29));
    QCOMPARE(dt.toUtc().time(), QTime(23, 59, 59));
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 0, 0), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.toUtc().date(), QDate(2005, 10, 30));
    QCOMPARE(dt.toUtc().time(), QTime(0, 0, 0));

    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.toUtc().date(), QDate(2005, 10, 30));
    QCOMPARE(dt.toUtc().time(), QTime(0, 59, 59));
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 0, 0), london);
    dt.setSecondOccurrence(true);
    QCOMPARE(dt.toUtc().date(), QDate(2005, 10, 30));
    QCOMPARE(dt.toUtc().time(), QTime(1, 0, 0));
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    dt.setSecondOccurrence(true);
    QCOMPARE(dt.toUtc().date(), QDate(2005, 10, 30));
    QCOMPARE(dt.toUtc().time(), QTime(1, 59, 59));
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(2, 0, 0), london);
    dt.setSecondOccurrence(true);   // this has no effect
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    QCOMPARE(dt.toUtc().date(), QDate(2005, 10, 30));
    QCOMPARE(dt.toUtc().time(), QTime(2, 0, 0));

    dt = KADateTime(QDate(2005, 10, 30), QTime(0, 59, 59), london);
    KADateTime dt1 = dt.addSecs(1);   // local time 01:00:00
    QVERIFY(!dt1.isSecondOccurrence());
    QVERIFY(dt1.qDateTime().isDaylightTime());
    dt1 = dt.addSecs(3600);   // local time 01:59:59
    QVERIFY(!dt1.isSecondOccurrence());
    QVERIFY(dt1.qDateTime().isDaylightTime());
    dt1 = dt.addSecs(3601);   // local time 01:00:00
    QVERIFY(dt1.isSecondOccurrence());
    QVERIFY(!dt1.qDateTime().isDaylightTime());
    dt1 = dt.addSecs(7200);   // local time 01:59:59
    QVERIFY(dt1.isSecondOccurrence());
    QVERIFY(!dt1.qDateTime().isDaylightTime());
    dt1 = dt.addSecs(7201);   // local time 02:00:00
    QVERIFY(!dt1.isSecondOccurrence());
    QVERIFY(!dt1.qDateTime().isDaylightTime());

    QVERIFY(KADateTime(QDate(2005, 10, 29), london) == KADateTime(QDate(2005, 10, 29), KADateTime::Spec::OffsetFromUTC(3600)));
    QVERIFY(KADateTime(QDate(2005, 10, 30), london) != KADateTime(QDate(2005, 10, 30), KADateTime::Spec::OffsetFromUTC(3600)));
    QVERIFY(KADateTime(QDate(2005, 10, 30), london) != KADateTime(QDate(2005, 10, 30), KADateTime::Spec::OffsetFromUTC(0)));
    QVERIFY(KADateTime(QDate(2005, 10, 31), london) == KADateTime(QDate(2005, 10, 31), KADateTime::Spec::OffsetFromUTC(0)));

    // Constructor (QDateTime)
    qdt = QDateTime(QDate(2005, 10, 30), QTime(0, 59, 59), london);
    bool dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QVERIFY(!dt.isSecondOccurrence());
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);
    qdt = QDateTime(QDate(2005, 10, 30), QTime(1, 0, 0), london);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QCOMPARE(dt.isSecondOccurrence(), !dst);
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);
    qdt = QDateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QCOMPARE(dt.isSecondOccurrence(), !dst);
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);
    qdt = QDateTime(QDate(2005, 10, 30), QTime(2, 0, 0), london);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QVERIFY(!dt.isSecondOccurrence());
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);

    // Set local time to London
    qputenv("TZ", ":Europe/London");
    ::tzset();
    qdt = QDateTime(QDate(2005, 10, 30), QTime(0, 59, 59), Qt::LocalTime);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QVERIFY(!dt.isSecondOccurrence());
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);
    qdt = QDateTime(QDate(2005, 10, 30), QTime(1, 0, 0), Qt::LocalTime);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QCOMPARE(dt.isSecondOccurrence(), !dst);
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);
    qdt = QDateTime(QDate(2005, 10, 30), QTime(1, 59, 59), Qt::LocalTime);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QCOMPARE(dt.isSecondOccurrence(), !dst);
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);
    qdt = QDateTime(QDate(2005, 10, 30), QTime(2, 0, 0), Qt::LocalTime);
    dst = qdt.isDaylightTime();
    dt = KADateTime(qdt);
    QVERIFY(!dt.isSecondOccurrence());
    QCOMPARE(dt.qDateTime().isDaylightTime(), dst);

    // setDate()
    dt = KADateTime(QDate(2005, 10, 29), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setDate(QDate(2005, 10, 30));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setDate(QDate(2005, 10, 31));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    dt.setSecondOccurrence(true);
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt.setDate(QDate(2005, 10, 31));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());

    // setTime()
    dt = KADateTime(QDate(2005, 10, 29), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setTime(QTime(5, 30, 25));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setTime(QTime(1, 30, 25));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setTime(QTime(5, 30, 25));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    dt.setSecondOccurrence(true);
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt.setTime(QTime(1, 30, 25));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    dt.setSecondOccurrence(true);
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt.setTime(QTime(5, 30, 25));
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());

    // setDateOnly()
    dt = KADateTime(QDate(2005, 10, 29), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setDateOnly(true);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setDateOnly(true);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), QTime(1, 59, 59), london);
    dt.setSecondOccurrence(true);
    QVERIFY(dt.isSecondOccurrence());
    QVERIFY(!dt.qDateTime().isDaylightTime());
    dt.setDateOnly(true);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt = KADateTime(QDate(2005, 10, 30), london);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());
    dt.setDateOnly(false);
    QVERIFY(!dt.isSecondOccurrence());
    QVERIFY(dt.qDateTime().isDaylightTime());

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

////////////////////
// String conversion
////////////////////

void KADateTimeTest::strings_iso8601()
{
    QTimeZone london("Europe/London");
    bool decpt = QLocale().decimalPoint() == QLatin1Char('.');   // whether this locale uses '.' as decimal symbol

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    KADateTime dtlocal(QDate(1999, 12, 11), QTime(3, 45, 6, 12), KADateTime::LocalZone);
    QString s = dtlocal.toString(KADateTime::ISODate);
    if (decpt) {
        QCOMPARE(s, QStringLiteral("1999-12-11T03:45:06.012"));
    } else {
        QCOMPARE(s, QStringLiteral("1999-12-11T03:45:06,012"));
    }
    KADateTime dtlocal1 = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtlocal1.qDateTime().toUTC(), dtlocal.qDateTime().toUTC());
    QCOMPARE(dtlocal1.timeType(), KADateTime::LocalZone);
    QCOMPARE(dtlocal1.utcOffset(), -8 * 3600);
    QVERIFY(dtlocal1 == dtlocal);

    s = dtlocal.toString(KADateTime::ISODateFull);
    if (decpt) {
        QCOMPARE(s, QStringLiteral("1999-12-11T03:45:06.012-08:00"));
    } else {
        QCOMPARE(s, QStringLiteral("1999-12-11T03:45:06,012-08:00"));
    }
    dtlocal1 = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtlocal1.qDateTime().toUTC(), dtlocal.qDateTime().toUTC());
    QCOMPARE(dtlocal1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtlocal1.utcOffset(), -8 * 3600);
    QVERIFY(dtlocal1 == dtlocal);

    dtlocal.setDateOnly(true);
    s = dtlocal.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("1999-12-11"));

    KADateTime dtzone(QDate(1999, 6, 11), QTime(3, 45, 6, 12), london);
    s = dtzone.toString(KADateTime::ISODate);
    if (decpt) {
        QCOMPARE(s, QStringLiteral("1999-06-11T03:45:06.012+01:00"));
    } else {
        QCOMPARE(s, QStringLiteral("1999-06-11T03:45:06,012+01:00"));
    }
    KADateTime dtzone1 = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtzone1.qDateTime().toUTC(), dtzone.qDateTime().toUTC());
    QCOMPARE(dtzone1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtzone1.utcOffset(), 3600);
    QVERIFY(dtzone1 == dtzone);
    dtzone.setDateOnly(true);
    s = dtzone.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("1999-06-11T00:00:00+01:00"));

    KADateTime dtutc(QDate(1999, 12, 11), QTime(3, 45, 0), KADateTime::UTC);
    s = dtutc.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("1999-12-11T03:45:00Z"));
    KADateTime dtutc1 = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtutc1.date(), dtutc.date());
    QCOMPARE(dtutc1.time(), dtutc.time());
    QCOMPARE(dtutc1.timeType(), KADateTime::UTC);
    QVERIFY(dtutc1 == dtutc);
    dtutc.setDateOnly(true);
    s = dtutc.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("1999-12-11T00:00:00Z"));

    // Check signed years
    KADateTime dtneg(QDate(-1999, 12, 11), QTime(3, 45, 6), KADateTime::LocalZone);
    s = dtneg.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("-1999-12-11T03:45:06"));
    KADateTime dtneg1 = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtneg1.date(), dtneg.date());
    QCOMPARE(dtneg1.time(), dtneg.time());
    QCOMPARE(dtneg1.timeType(), KADateTime::LocalZone);
    QVERIFY(dtneg1 == dtneg);
    KADateTime dtneg2 = KADateTime::fromString(QStringLiteral("-19991211T034506"), KADateTime::ISODate);
    QVERIFY(dtneg2 == dtneg);

    dtneg.setDateOnly(true);
    s = dtneg.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("-1999-12-11"));
    dtneg1 = KADateTime::fromString(s, KADateTime::ISODate);
    QVERIFY(dtneg1.isDateOnly());
    QCOMPARE(dtneg1.timeType(), KADateTime::LocalZone);
    QCOMPARE(dtneg1.date(), QDate(-1999, 12, 11));
    dtneg2 = KADateTime::fromString(QStringLiteral("-19991211"), KADateTime::ISODate);
    QVERIFY(dtneg2 == dtneg1);

    s = QStringLiteral("+1999-12-11T03:45:06");
    KADateTime dtpos = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtpos.date(), QDate(1999, 12, 11));
    QCOMPARE(dtpos.time(), QTime(3, 45, 6));
    QCOMPARE(dtpos.timeType(), KADateTime::LocalZone);
    KADateTime dtpos2 = KADateTime::fromString(QStringLiteral("+19991211T034506"), KADateTime::ISODate);
    QVERIFY(dtpos2 == dtpos);

    dtpos.setDateOnly(true);
    s = QStringLiteral("+1999-12-11");
    dtpos = KADateTime::fromString(s, KADateTime::ISODate);
    QVERIFY(dtpos.isDateOnly());
    QCOMPARE(dtpos.timeType(), KADateTime::LocalZone);
    QCOMPARE(dtpos.date(), QDate(1999, 12, 11));
    dtpos2 = KADateTime::fromString(QStringLiteral("+19991211"), KADateTime::ISODate);
    QVERIFY(dtpos2 == dtpos);

    // Check years with >4 digits
    KADateTime dtbig(QDate(123456, 12, 11), QTime(3, 45, 06), KADateTime::LocalZone);
    s = dtbig.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("123456-12-11T03:45:06"));
    KADateTime dtbig1 = KADateTime::fromString(s, KADateTime::ISODate);
    QCOMPARE(dtbig1.date(), dtbig.date());
    QCOMPARE(dtbig1.time(), dtbig.time());
    QCOMPARE(dtbig1.timeType(), KADateTime::LocalZone);
    QVERIFY(dtbig1 == dtbig);
    KADateTime dtbig2 = KADateTime::fromString(QStringLiteral("1234561211T034506"), KADateTime::ISODate);
    QVERIFY(dtbig2 == dtbig);

    dtbig.setDateOnly(true);
    s = dtbig.toString(KADateTime::ISODate);
    QCOMPARE(s, QStringLiteral("123456-12-11"));
    dtbig1 = KADateTime::fromString(s, KADateTime::ISODate);
    QVERIFY(dtbig1.isDateOnly());
    QCOMPARE(dtbig1.timeType(), KADateTime::LocalZone);
    QCOMPARE(dtbig1.date(), QDate(123456, 12, 11));
    dtbig2 = KADateTime::fromString(QStringLiteral("1234561211"), KADateTime::ISODate);
    QVERIFY(dtbig2 == dtbig1);

    // Check basic format strings
    bool negZero = true;
    KADateTime dt = KADateTime::fromString(QStringLiteral("20000301T1213"), KADateTime::ISODate, &negZero);
    QVERIFY(dt.timeType() == KADateTime::LocalZone);
    QVERIFY(!dt.isDateOnly());
    QVERIFY(!negZero);
    QCOMPARE(dt.date(), QDate(2000, 3, 1));
    QCOMPARE(dt.time(), QTime(12, 13, 0));
    dt = KADateTime::fromString(QStringLiteral("20000301"), KADateTime::ISODate, &negZero);
    QVERIFY(dt.timeType() == KADateTime::LocalZone);
    QVERIFY(dt.isDateOnly());
    QVERIFY(!negZero);
    QCOMPARE(dt.date(), QDate(2000, 3, 1));
    KADateTime::setFromStringDefault(KADateTime::UTC);
    dt = KADateTime::fromString(QStringLiteral("20000301T1213"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::UTC);
    QCOMPARE(dt.date(), QDate(2000, 3, 1));
    QCOMPARE(dt.time(), QTime(12, 13, 0));
    KADateTime::setFromStringDefault(KADateTime::LocalZone);
    dt = KADateTime::fromString(QStringLiteral("20000301T1213"), KADateTime::ISODate);
    QVERIFY(dt.timeSpec() == KADateTime::Spec::LocalZone());
    QCOMPARE(dt.date(), QDate(2000, 3, 1));
    QCOMPARE(dt.time(), QTime(12, 13, 0));
    KADateTime::setFromStringDefault(london);
    dt = KADateTime::fromString(QStringLiteral("20000301T1213"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::TimeZone);
    QCOMPARE(dt.date(), QDate(2000, 3, 1));
    QCOMPARE(dt.time(), QTime(12, 13, 0));
    KADateTime::setFromStringDefault(KADateTime::Spec::OffsetFromUTC(5000));  // = +01:23:20
    dt = KADateTime::fromString(QStringLiteral("20000601T1213"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 5000);
    QCOMPARE(dt.toUtc().date(), QDate(2000, 6, 1));
    QCOMPARE(dt.toUtc().time(), QTime(10, 49, 40));
    KADateTime::setFromStringDefault(KADateTime::LocalZone);
    dt = KADateTime::fromString(QStringLiteral("6543210301T1213"), KADateTime::ISODate, &negZero);
    QVERIFY(dt.timeType() == KADateTime::LocalZone);
    QVERIFY(!dt.isDateOnly());
    QVERIFY(!negZero);
    QCOMPARE(dt.date(), QDate(654321, 3, 1));
    QCOMPARE(dt.time(), QTime(12, 13, 0));
    dt = KADateTime::fromString(QStringLiteral("6543210301"), KADateTime::ISODate, &negZero);
    QVERIFY(dt.isDateOnly());
    QVERIFY(!negZero);
    QCOMPARE(dt.date(), QDate(654321, 3, 1));
    dt = KADateTime::fromString(QStringLiteral("-47120301T1213"), KADateTime::ISODate, &negZero);
    QVERIFY(dt.timeType() == KADateTime::LocalZone);
    QVERIFY(!dt.isDateOnly());
    QVERIFY(!negZero);
    QCOMPARE(dt.date(), QDate(-4712, 3, 1));
    QCOMPARE(dt.time(), QTime(12, 13, 0));
    dt = KADateTime::fromString(QStringLiteral("-47120301"), KADateTime::ISODate, &negZero);
    QVERIFY(dt.isDateOnly());
    QVERIFY(!negZero);
    QCOMPARE(dt.date(), QDate(-4712, 3, 1));

    // Check strings containing day-of-the-year
    dt = KADateTime::fromString(QStringLiteral("1999-060T19:20:21.06-11:20"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), -11 * 3600 - 20 * 60);
    QCOMPARE(dt.date(), QDate(1999, 3, 1));
    QCOMPARE(dt.time(), QTime(19, 20, 21, 60));
    dt = KADateTime::fromString(QStringLiteral("1999-060T19:20:21,06-11:20"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), -11 * 3600 - 20 * 60);
    QCOMPARE(dt.date(), QDate(1999, 3, 1));
    QCOMPARE(dt.time(), QTime(19, 20, 21, 60));
    dt = KADateTime::fromString(QStringLiteral("1999060T192021.06-1120"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), -11 * 3600 - 20 * 60);
    QCOMPARE(dt.date(), QDate(1999, 3, 1));
    QCOMPARE(dt.time(), QTime(19, 20, 21, 60));
    dt = KADateTime::fromString(QStringLiteral("1999-060"), KADateTime::ISODate);
    QVERIFY(dt.timeType() == KADateTime::LocalZone);
    QVERIFY(dt.isDateOnly());
    QCOMPARE(dt.date(), QDate(1999, 3, 1));

    // Check 24:00:00
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T24:00:00+03:00"), KADateTime::ISODate);
    QCOMPARE(dt.date(), QDate(1999, 6, 12));
    QCOMPARE(dt.time(), QTime(0, 0, 0));
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T24:00:01+03:00"), KADateTime::ISODate);
    QVERIFY(!dt.isValid());

    // Check leap second
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T23:59:60Z"), KADateTime::ISODate);
    QCOMPARE(dt.date(), QDate(1999, 6, 11));
    QCOMPARE(dt.time(), QTime(23, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T13:59:60Z"), KADateTime::ISODate);
    QVERIFY(!dt.isValid());
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T13:59:60-10:00"), KADateTime::ISODate);
    QCOMPARE(dt.date(), QDate(1999, 6, 11));
    QCOMPARE(dt.time(), QTime(13, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T23:59:60-10:00"), KADateTime::ISODate);
    QVERIFY(!dt.isValid());

    // Check negZero
    dt = KADateTime::fromString(QStringLiteral("1999-060T19:20:21.06-00:00"), KADateTime::ISODate, &negZero);
    QVERIFY(negZero);
    dt = KADateTime::fromString(QStringLiteral("1999-060T19:20:21.06+00:00"), KADateTime::ISODate, &negZero);
    QVERIFY(!negZero);

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::strings_rfc2822()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    bool negZero = true;
    KADateTime dtlocal(QDate(1999, 12, 11), QTime(3, 45, 6), KADateTime::LocalZone);
    QString s = dtlocal.toString(KADateTime::RFCDate);
    QCOMPARE(s, QStringLiteral("11 Dec 1999 03:45:06 -0800"));
    KADateTime dtlocal1 = KADateTime::fromString(s, KADateTime::RFCDate, &negZero);
    QCOMPARE(dtlocal1.qDateTime().toUTC(), dtlocal.qDateTime().toUTC());
    QCOMPARE(dtlocal1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtlocal1.utcOffset(), -8 * 3600);
    QVERIFY(dtlocal1 == dtlocal);
    QVERIFY(!negZero);
    KADateTime dtlocal2 = KADateTime::fromString(s, KADateTime::RFCDateDay);
    QVERIFY(!dtlocal2.isValid());
    s = dtlocal.toString(KADateTime::RFCDateDay);
    QCOMPARE(s, QStringLiteral("Sat, 11 Dec 1999 03:45:06 -0800"));
    dtlocal2 = KADateTime::fromString(s, KADateTime::RFCDate);
    QVERIFY(dtlocal1 == dtlocal2);
    QCOMPARE(dtlocal1.date(), dtlocal2.date());
    QCOMPARE(dtlocal1.time(), dtlocal2.time());
    dtlocal2 = KADateTime::fromString(s, KADateTime::RFCDateDay);
    QVERIFY(dtlocal1 == dtlocal2);
    dtlocal2 = KADateTime::fromString(QStringLiteral("Saturday, 11-Dec-99 03:45:06 -0800"), KADateTime::RFCDate);
    QVERIFY(dtlocal1 == dtlocal2);
    dtlocal2 = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 PST"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtlocal1 == dtlocal2);
    QVERIFY(!negZero);
    dtlocal.setDateOnly(true);
    s = dtlocal.toString(KADateTime::RFCDate);
    QCOMPARE(s, QStringLiteral("11 Dec 1999 00:00 -0800"));
    s = dtlocal.toString(KADateTime::RFCDateDay);
    QCOMPARE(s, QStringLiteral("Sat, 11 Dec 1999 00:00 -0800"));

    KADateTime dtzone(QDate(1999, 6, 11), QTime(3, 45, 6), london);
    s = dtzone.toString(KADateTime::RFCDate);
    QCOMPARE(s, QStringLiteral("11 Jun 1999 03:45:06 +0100"));
    KADateTime dtzone1 = KADateTime::fromString(s, KADateTime::RFCDate);
    QCOMPARE(dtzone1.qDateTime().toUTC(), dtzone.qDateTime().toUTC());
    QCOMPARE(dtzone1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtzone1.utcOffset(), 3600);
    QVERIFY(dtzone1 == dtzone);
    KADateTime dtzone2 = KADateTime::fromString(s, KADateTime::RFCDateDay);
    QVERIFY(!dtzone2.isValid());
    s = dtzone.toString(KADateTime::RFCDateDay);
    QCOMPARE(s, QStringLiteral("Fri, 11 Jun 1999 03:45:06 +0100"));
    dtzone2 = KADateTime::fromString(s, KADateTime::RFCDate);
    QVERIFY(dtzone1 == dtzone2);
    QCOMPARE(dtzone1.date(), dtzone2.date());
    QCOMPARE(dtzone1.time(), dtzone2.time());
    dtzone2 = KADateTime::fromString(s, KADateTime::RFCDateDay, &negZero);
    QVERIFY(dtzone1 == dtzone2);
    QVERIFY(!negZero);
    dtzone2 = KADateTime::fromString(QStringLiteral("Friday, 11-Jun-99 03:45:06 +0100"), KADateTime::RFCDateDay);
    QVERIFY(dtzone1 == dtzone2);
    dtzone.setDateOnly(true);
    s = dtzone.toString(KADateTime::RFCDate);
    QCOMPARE(s, QStringLiteral("11 Jun 1999 00:00 +0100"));
    s = dtzone.toString(KADateTime::RFCDateDay);
    QCOMPARE(s, QStringLiteral("Fri, 11 Jun 1999 00:00 +0100"));

    KADateTime dtutc(QDate(1999, 12, 11), QTime(3, 45, 00), KADateTime::UTC);
    s = dtutc.toString(KADateTime::RFCDate);
    QCOMPARE(s, QStringLiteral("11 Dec 1999 03:45 +0000"));
    KADateTime dtutc1 = KADateTime::fromString(s, KADateTime::RFCDate, &negZero);
    QCOMPARE(dtutc1.date(), dtutc.date());
    QCOMPARE(dtutc1.time(), dtutc.time());
    QCOMPARE(dtutc1.timeType(), KADateTime::UTC);
    QVERIFY(dtutc1 == dtutc);
    QVERIFY(!negZero);
    KADateTime dtutc2 = KADateTime::fromString(s, KADateTime::RFCDateDay);
    QVERIFY(!dtutc2.isValid());
    s = dtutc.toString(KADateTime::RFCDateDay);
    QCOMPARE(s, QStringLiteral("Sat, 11 Dec 1999 03:45 +0000"));
    dtutc2 = KADateTime::fromString(s, KADateTime::RFCDate);
    QVERIFY(dtutc1 == dtutc2);
    QCOMPARE(dtutc1.date(), dtutc2.date());
    QCOMPARE(dtutc1.time(), dtutc2.time());
    dtutc2 = KADateTime::fromString(s, KADateTime::RFCDateDay);
    QVERIFY(dtutc1 == dtutc2);
    dtutc2 = KADateTime::fromString(QStringLiteral("Saturday, 11-Dec-99 03:45 +0000"), KADateTime::RFCDate);
    QVERIFY(dtutc1 == dtutc2);
    dtutc.setDateOnly(true);
    s = dtutc.toString(KADateTime::RFCDate);
    QCOMPARE(s, QStringLiteral("11 Dec 1999 00:00 +0000"));
    s = dtutc.toString(KADateTime::RFCDateDay);
    QCOMPARE(s, QStringLiteral("Sat, 11 Dec 1999 00:00 +0000"));

    // Check '-0000' and unknown/invalid time zone names
    dtutc2 = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45 -0000"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtutc1 == dtutc2);
    QVERIFY(negZero);
    dtutc2 = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45 B"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtutc1 == dtutc2);
    QVERIFY(negZero);
    dtutc2 = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45 BCDE"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtutc1 == dtutc2);
    QVERIFY(negZero);

    // Check named time offsets
    KADateTime dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 UT"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::UTC);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 GMT"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::UTC);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 EDT"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -4 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 EST"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -5 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 CDT"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -5 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 CST"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -6 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 MDT"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -6 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 MST"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -7 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 PDT"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -7 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);
    dtzname = KADateTime::fromString(QStringLiteral("11 Dec 1999 03:45:06 PST"), KADateTime::RFCDate, &negZero);
    QVERIFY(dtzname.timeType() == KADateTime::OffsetFromUTC);
    QCOMPARE(dtzname.utcOffset(), -8 * 3600);
    QCOMPARE(dtzname.date(), QDate(1999, 12, 11));
    QCOMPARE(dtzname.time(), QTime(3, 45, 6));
    QVERIFY(!negZero);

    // Check leap second
    KADateTime dt = KADateTime::fromString(QStringLiteral("11 Dec 1999 23:59:60 -0000"), KADateTime::RFCDate);
    QCOMPARE(dt.date(), QDate(1999, 12, 11));
    QCOMPARE(dt.time(), QTime(23, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("11 Dec 1999 13:59:60 -0000"), KADateTime::RFCDate);
    QVERIFY(!dt.isValid());
    dt = KADateTime::fromString(QStringLiteral("11 Jun 1999 13:59:60 -1000"), KADateTime::RFCDate);
    QCOMPARE(dt.date(), QDate(1999, 6, 11));
    QCOMPARE(dt.time(), QTime(13, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("11 Dec 1999 23:59:60 -1000"), KADateTime::RFCDate);
    QVERIFY(!dt.isValid());

    // Check erroneous strings:
    dtutc2 = KADateTime::fromString(QStringLiteral("11 Dec 1999 23:59:60 -00:00"), KADateTime::RFCDate);
    QVERIFY(!dtutc2.isValid());     // colon in UTC offset
    dtutc2 = KADateTime::fromString(QStringLiteral("Sun, 11 Dec 1999 03:45 +0000"), KADateTime::RFCDate);
    QVERIFY(!dtutc2.isValid());     // wrong weekday
    dtutc2 = KADateTime::fromString(QStringLiteral("Satu, 11 Dec 1999 03:45 +0000"), KADateTime::RFCDate);
    QVERIFY(!dtutc2.isValid());     // bad weekday
    dtutc2 = KADateTime::fromString(QStringLiteral("11 Dece 1999 03:45 +0000"), KADateTime::RFCDate);
    QVERIFY(!dtutc2.isValid());     // bad month
    dtutc2 = KADateTime::fromString(QStringLiteral("11-Dec 1999 03:45 +0000"), KADateTime::RFCDate);
    QVERIFY(!dtutc2.isValid());     // only one hyphen in date

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::strings_rfc3339()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    bool negZero = true;
    KADateTime dtlocal(QDate(1999, 2, 9), QTime(3, 45, 6, 236), KADateTime::LocalZone);
    QString s = dtlocal.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-02-09T03:45:06.236-08:00"));
    KADateTime dtlocal1 = KADateTime::fromString(s, KADateTime::RFC3339Date, &negZero);
    QCOMPARE(dtlocal1.qDateTime().toUTC(), dtlocal.qDateTime().toUTC());
    QCOMPARE(dtlocal1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtlocal1.utcOffset(), -8 * 3600);
    QVERIFY(dtlocal1 == dtlocal);
    QVERIFY(!negZero);
    dtlocal.setDateOnly(true);
    s = dtlocal.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-02-09T00:00:00-08:00"));

    KADateTime dtzone(QDate(1999, 6, 9), QTime(3, 45, 06, 230), london);
    s = dtzone.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-06-09T03:45:06.23+01:00"));
    KADateTime dtzone1 = KADateTime::fromString(s, KADateTime::RFC3339Date);
    QCOMPARE(dtzone1.qDateTime().toUTC(), dtzone.qDateTime().toUTC());
    QCOMPARE(dtzone1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtzone1.utcOffset(), 3600);
    QVERIFY(dtzone1 == dtzone);
    dtzone.setDateOnly(true);
    s = dtzone.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-06-09T00:00:00+01:00"));

    KADateTime dtzone2(QDate(1999, 6, 9), QTime(3, 45, 06, 200), london);
    s = dtzone2.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-06-09T03:45:06.2+01:00"));
    KADateTime dtzone3 = KADateTime::fromString(s, KADateTime::RFC3339Date);
    QCOMPARE(dtzone3.qDateTime().toUTC(), dtzone2.qDateTime().toUTC());

    KADateTime dtutc(QDate(1999, 2, 9), QTime(3, 45, 00), KADateTime::UTC);
    s = dtutc.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-02-09T03:45:00Z"));
    KADateTime dtutc1 = KADateTime::fromString(s, KADateTime::RFC3339Date, &negZero);
    QCOMPARE(dtutc1.date(), dtutc.date());
    QCOMPARE(dtutc1.time(), dtutc.time());
    QCOMPARE(dtutc1.timeType(), KADateTime::UTC);
    QVERIFY(dtutc1 == dtutc);
    QVERIFY(!negZero);
    KADateTime dtutc2 = KADateTime::fromString(QStringLiteral("1999-02-09t03:45:00z"), KADateTime::RFC3339Date, &negZero);
    QVERIFY(dtutc1 == dtutc2);
    dtutc.setDateOnly(true);
    s = dtutc.toString(KADateTime::RFC3339Date);
    QCOMPARE(s, QStringLiteral("1999-02-09T00:00:00Z"));

    // Check '-00:00' (specifies unknown local offset)
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-02-09T03:45:00-00:00"), KADateTime::RFC3339Date, &negZero);
    QVERIFY(dtutc1 == dtutc2);
    QVERIFY(negZero);
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-02-09T03:45:00+00:00"), KADateTime::RFC3339Date, &negZero);
    QVERIFY(dtutc1 == dtutc2);
    QVERIFY(!negZero);

    // Check leap second
    KADateTime dt = KADateTime::fromString(QStringLiteral("1999-02-09T23:59:60z"), KADateTime::RFC3339Date);
    QCOMPARE(dt.date(), QDate(1999, 2, 9));
    QCOMPARE(dt.time(), QTime(23, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("1999-02-09T23:59:60+00:00"), KADateTime::RFC3339Date);
    QCOMPARE(dt.toUtc().date(), QDate(1999, 2, 9));
    QCOMPARE(dt.toUtc().time(), QTime(23, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("1999-02-09T13:59:60-00:00"), KADateTime::RFC3339Date);
    QVERIFY(!dt.isValid());
    dt = KADateTime::fromString(QStringLiteral("1999-06-11T13:59:60-10:00"), KADateTime::RFC3339Date);
    QCOMPARE(dt.toUtc().date(), QDate(1999, 6, 11));
    QCOMPARE(dt.toUtc().time(), QTime(23, 59, 59));
    dt = KADateTime::fromString(QStringLiteral("1999-12-11T23:59:60-10:00"), KADateTime::RFC3339Date);
    QVERIFY(!dt.isValid());

    // Check erroneous strings:
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-02-09 03:45:00"), KADateTime::RFC3339Date, &negZero);
    QVERIFY(!dtutc2.isValid());
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-02-09T03:45:00B"), KADateTime::RFC3339Date, &negZero);
    QVERIFY(!dtutc2.isValid());
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-02-09T23:59:60-0000"), KADateTime::RFC3339Date);
    QVERIFY(!dtutc2.isValid());     // no colon in UTC offset
    dtutc2 = KADateTime::fromString(QStringLiteral("19990-12-10T03:45:01+00:00"), KADateTime::RFC3339Date);
    QVERIFY(!dtutc2.isValid());     // bad year
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-13-10T03:45:01+00:00"), KADateTime::RFC3339Date);
    QVERIFY(!dtutc2.isValid());     // bad month
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-10-32T03:45:01+00:00"), KADateTime::RFC3339Date);
    QVERIFY(!dtutc2.isValid());     // bad day
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-1209T03:45:00+00:00"), KADateTime::RFC3339Date);
    QVERIFY(!dtutc2.isValid());     // only one hyphen in date
    dtutc2 = KADateTime::fromString(QStringLiteral("1999-12T03:45:00+00:00"), KADateTime::RFC3339Date);
    QVERIFY(!dtutc2.isValid());     // no day of month

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::strings_qttextdate()
{
    QTimeZone london("Europe/London");

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    bool negZero = true;
    KADateTime dtlocal(QDate(1999, 12, 11), QTime(3, 45, 6), KADateTime::LocalZone);
    QString s = dtlocal.toString(KADateTime::QtTextDate);
    QCOMPARE(s, QStringLiteral("Sat Dec 11 03:45:06 1999"));
    KADateTime dtlocal1 = KADateTime::fromString(s, KADateTime::QtTextDate, &negZero);
    QCOMPARE(dtlocal1.qDateTime().toUTC(), dtlocal.qDateTime().toUTC());
    QCOMPARE(dtlocal1.timeType(), KADateTime::LocalZone);
    QCOMPARE(dtlocal1.utcOffset(), -8 * 3600);
    QVERIFY(dtlocal1 == dtlocal);
    QVERIFY(!dtlocal1.isDateOnly());
    QVERIFY(!negZero);
    dtlocal.setDateOnly(true);
    s = dtlocal.toString(KADateTime::QtTextDate);
    QCOMPARE(s, QStringLiteral("Sat Dec 11 1999"));
    dtlocal1 = KADateTime::fromString(s, KADateTime::QtTextDate, &negZero);
    QVERIFY(dtlocal1.isDateOnly());
    QCOMPARE(dtlocal1.date(), QDate(1999, 12, 11));
    QCOMPARE(dtlocal1.timeType(), KADateTime::LocalZone);
    QCOMPARE(dtlocal1.utcOffset(), -8 * 3600);

    KADateTime dtzone(QDate(1999, 6, 11), QTime(3, 45, 6), london);
    s = dtzone.toString(KADateTime::QtTextDate);
    QCOMPARE(s, QStringLiteral("Fri Jun 11 03:45:06 1999 +0100"));
    KADateTime dtzone1 = KADateTime::fromString(s, KADateTime::QtTextDate);
    QCOMPARE(dtzone1.qDateTime().toUTC(), dtzone.qDateTime().toUTC());
    QCOMPARE(dtzone1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtzone1.utcOffset(), 3600);
    QVERIFY(!dtzone1.isDateOnly());
    QVERIFY(dtzone1 == dtzone);
    KADateTime dtzone2 = KADateTime::fromString(s, KADateTime::QtTextDate, &negZero);
    QVERIFY(dtzone1 == dtzone2);
    QVERIFY(!negZero);
    dtzone.setDateOnly(true);
    s = dtzone.toString(KADateTime::QtTextDate);
    QCOMPARE(s, QStringLiteral("Fri Jun 11 1999 +0100"));
    dtzone1 = KADateTime::fromString(s, KADateTime::QtTextDate, &negZero);
    QVERIFY(dtzone1.isDateOnly());
    QCOMPARE(dtzone1.date(), QDate(1999, 6, 11));
    QCOMPARE(dtzone1.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dtzone1.utcOffset(), 3600);

    KADateTime dtutc(QDate(1999, 12, 11), QTime(3, 45, 0), KADateTime::UTC);
    s = dtutc.toString(KADateTime::QtTextDate);
    QCOMPARE(s, QStringLiteral("Sat Dec 11 03:45:00 1999 +0000"));
    KADateTime dtutc1 = KADateTime::fromString(s, KADateTime::QtTextDate, &negZero);
    QCOMPARE(dtutc1.date(), dtutc.date());
    QCOMPARE(dtutc1.time(), dtutc.time());
    QCOMPARE(dtutc1.timeType(), KADateTime::UTC);
    QVERIFY(dtutc1 == dtutc);
    QVERIFY(!dtutc1.isDateOnly());
    QVERIFY(!negZero);
    dtutc.setDateOnly(true);
    s = dtutc.toString(KADateTime::QtTextDate);
    QCOMPARE(s, QStringLiteral("Sat Dec 11 1999 +0000"));
    dtutc1 = KADateTime::fromString(s, KADateTime::QtTextDate, &negZero);
    QVERIFY(dtutc1.isDateOnly());
    QCOMPARE(dtutc1.date(), QDate(1999, 12, 11));
    QCOMPARE(dtutc1.timeType(), KADateTime::UTC);

    // Check '-0000'
    KADateTime dtutc2 = KADateTime::fromString(QStringLiteral("Sat Dec 11 03:45:00 1999 -0000"), KADateTime::QtTextDate, &negZero);
    QVERIFY(dtutc1 != dtutc2);
    QVERIFY(negZero);

    // Check erroneous strings
    dtutc2 = KADateTime::fromString(QStringLiteral("Sat Dec 11 03:45:00 1999 GMT"), KADateTime::QtTextDate, &negZero);
    QVERIFY(!dtutc2.isValid());
    dtutc2 = KADateTime::fromString(QStringLiteral("Sun Dec 11 03:45:00 1999 +0000"), KADateTime::QtTextDate);
    QVERIFY(dtutc2.isValid());     // wrong weekday: accepted by Qt!!
    dtutc2 = KADateTime::fromString(QStringLiteral("Satu, Dec 11 03:45:00 1999 +0000"), KADateTime::QtTextDate);
    QVERIFY(dtutc2.isValid());     // bad weekday, accepted by Qt (since 4.3)
    dtutc2 = KADateTime::fromString(QStringLiteral("Sat Dece 11 03:45:00 1999 +0000"), KADateTime::QtTextDate);
    QVERIFY(!dtutc2.isValid());     // bad month, not accepted by Qt anymore (since 4.3)

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::strings_format()
{
    QTimeZone london("Europe/London");
    QTimeZone paris("Europe/Paris");
    QTimeZone berlin("Europe/Berlin");
    QTimeZone cairo("Africa/Cairo");
    QList<QTimeZone> zones{london, paris, berlin, cairo};

    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    QLocale locale;

    // toString()
    QString all = QStringLiteral("%Y.%y.%m.%:m.%B.%b.%d.%e.%A.%a-%H.%k.%I.%l.%M.%S?%:s?%P.%p.%:u.%z.%Z.%:Z.%:A.%:a.%:B.%:b/%:S.%:z.%%.");
    KADateTime dt(QDate(1999, 2, 3), QTime(6, 5, 0), KADateTime::LocalZone);
    QString s = dt.toString(all);
    QCOMPARE(s, QStringLiteral("1999.99.02.2.%1.%2.03.3.%3.%4-06.6.06.6.05.00?000?am.AM.-08.-0800.PST.America/Los_Angeles.Wednesday.Wed.February.Feb/.-08:00.%.")
             .arg(locale.monthName(2, QLocale::LongFormat),
                  locale.monthName(2, QLocale::ShortFormat),
                  locale.dayName(3, QLocale::LongFormat),
                  locale.dayName(3, QLocale::ShortFormat)));

    KADateTime dtzone(QDate(1970, 4, 30), QTime(12, 45, 16, 25), london);
    s = dtzone.toString(all);
    QCOMPARE(s, QStringLiteral("1970.70.04.4.%1.%2.30.30.%3.%4-12.12.12.12.45.16?025?pm.PM.+01.+0100.BST.Europe/London.Thursday.Thu.April.Apr/:16.+01:00.%.")
             .arg(locale.monthName(4, QLocale::LongFormat),
                  locale.monthName(4, QLocale::ShortFormat),
                  locale.dayName(4, QLocale::LongFormat),
                  locale.dayName(4, QLocale::ShortFormat)));

    KADateTime dtutc(QDate(2000, 12, 31), QTime(13, 45, 16, 100), KADateTime::UTC);
    s = dtutc.toString(all);
    QCOMPARE(s, QStringLiteral("2000.00.12.12.%1.%2.31.31.%3.%4-13.13.01.1.45.16?100?pm.PM.+00.+0000.UTC.UTC.Sunday.Sun.December.Dec/:16.+00:00.%.")
             .arg(locale.monthName(12, QLocale::LongFormat),
                  locale.monthName(12, QLocale::ShortFormat),
                  locale.dayName(7, QLocale::LongFormat),
                  locale.dayName(7, QLocale::ShortFormat)));

    // fromString() without QList<QTimeZone> parameter
    dt = KADateTime::fromString(QStringLiteral("2005/10/03/20:2,03"), QStringLiteral("%Y/%:m/%d/%S:%k,%M"));
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(2, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::LocalZone);

    dt = KADateTime::fromString(QStringLiteral("%1pm05ab%2t/032/20:2,03+10")
                               .arg(locale.dayName(1, QLocale::LongFormat),
                                    locale.monthName(10, QLocale::LongFormat)),
                               QStringLiteral("%a%p%yab%Bt/%e2/%S:%l,%M %z"));
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 10 * 3600);
    dt = KADateTime::fromString(QStringLiteral("%1pm05ab%2t/032/20:2,03+10")
                               .arg(locale.dayName(1, QLocale::ShortFormat),
                                    locale.monthName(10, QLocale::ShortFormat)),
                               QStringLiteral("%a%p%yab%Bt/%d2/%s:%l,%:M %z"));
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 10 * 3600);
    dt = KADateTime::fromString(QStringLiteral("monpm05aboCtt/032/20:2,03+10"), QStringLiteral("%a%p%yab%Bt/%d2/%S:%l,%M %z"));
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 10 * 3600);
    dt = KADateTime::fromString(QStringLiteral("monDAYpm05aboCtoBert/032/20:2,03+10"), QStringLiteral("%a%p%yab%Bt/%e2/%S:%l,%M %z"));
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 10 * 3600);
    dt = KADateTime::fromString(QStringLiteral("monDAYpm05abmzatemer/032/20:2,03+10"), QStringLiteral("%a%p%yab%B/%e2/%S:%l,%M %z"));
    QVERIFY(!dt.isValid());    // invalid month name
    dt = KADateTime::fromString(QStringLiteral("monDApm05aboct/032/20:2,03+10"), QStringLiteral("%a%p%yab%B/%e2/%S:%l,%M %z"));
    QVERIFY(!dt.isValid());    // invalid day name
    dt = KADateTime::fromString(QStringLiteral("mONdAYPM2005aboCtt/032/20:02,03+1000"), QStringLiteral("%:A%:p%Yab%Bt/%d2/%S:%I,%M %:u"));
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 3, 20));
    QCOMPARE(dt.utcOffset(), 10 * 3600);
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    KADateTime dtlocal = KADateTime::fromString(QStringLiteral("mONdAYPM2005abOctt/032/20:02,03+100"), QStringLiteral("%:A%:p%Yab%Bt/%e2/%S:%l,%M %:u"));
    QVERIFY(!dtlocal.isValid());    // wrong number of digits in UTC offset
    dtlocal = KADateTime::fromString(QStringLiteral("mONdAYPM2005abOctt/032/20:02,03+1"), QStringLiteral("%:A%:p%Yab%Bt/%d2/%S:%I,%M %z"));
    QVERIFY(!dtlocal.isValid());    // wrong number of digits in UTC offset
    dtlocal = KADateTime::fromString(QStringLiteral("mONdAYPM2005aboCtt/032/20:13,03+1000"), QStringLiteral("%:A%:p%Yab%Bt/%d2/%S:%I,%M %:u"));
    QVERIFY(!dtlocal.isValid());    // hours out of range for am/pm
    dtlocal = KADateTime::fromString(QStringLiteral("mONdAYPM2005aboCtt/032/20:00,03+1000"), QStringLiteral("%:A%:p%Yab%Bt/%d2/%S:%I,%M %:u"));
    QVERIFY(!dtlocal.isValid());    // hours out of range for am/pm

    // fromString() with QList<QTimeZone> parameter
    dt = KADateTime::fromString(QStringLiteral("mon 2005/10/03/20:2,03"), QStringLiteral("%:a %Y/%:m/%e/%S:%k,%M"), &zones);
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(2, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::LocalZone);
    dt = KADateTime::fromString(QStringLiteral("tue 2005/10/03/20:2,03"), QStringLiteral("%:a %Y/%:m/%d/%S:%k,%M"), &zones);
    QVERIFY(!dt.isValid());    // wrong day-of-week

    dt = KADateTime::fromString(QStringLiteral("pm2005aboCtt/03monday/20:2,03+03:00"), QStringLiteral("%p%Yab%Bt/%e%:A/%S:%l,%M %:z"), &zones);
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 3 * 3600);
    QVERIFY(!dt.timeZone().isValid());
    dt = KADateTime::fromString(QStringLiteral("pm2005aboCtt/03sunday/20:2,03+03:00"), QStringLiteral("%p%Yab%Bt/%d%A/%S:%l,%M %:z"), &zones);
    QVERIFY(!dt.isValid());    // wrong day-of-week

    dtutc = KADateTime::fromString(QStringLiteral("2000-01-01T00:00:00.000+0000"), QStringLiteral("%Y-%m-%dT%H:%M%:S%:s%z"));
    QVERIFY(dtutc.isValid());

    dt = KADateTime::fromString(QStringLiteral("2000-01-01T05:00:00.000+0500"), QStringLiteral("%Y-%m-%dT%H:%M%:S%:s%z"));
    QVERIFY(dt.isValid());
    QVERIFY(dtutc == dt);

    dt = KADateTime::fromString(QStringLiteral("1999-12-31T20:30:00.000-0330"), QStringLiteral("%Y-%m-%dT%H:%M%:S%:s%z"));
    QVERIFY(dt.isValid());
    QVERIFY(dtutc == dt);

    dt = KADateTime::fromString(QStringLiteral("200510031430:01.3+0100"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"), &zones, true);
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 30, 01, 300));
    QCOMPARE(dt.timeType(), KADateTime::TimeZone);
    QCOMPARE(dt.timeZone(), london);
    QCOMPARE(dt.utcOffset(), 3600);

    dt = KADateTime::fromString(QStringLiteral("200510031430:01.3+0500"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"), &zones, false);
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 30, 01, 300));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 5 * 3600);

    dt = KADateTime::fromString(QStringLiteral("200510031430:01.3+0200"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"), &zones, true);
    QCOMPARE(dt.date(), QDate(2005, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 30, 01, 300));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 2 * 3600);
    dt = KADateTime::fromString(QStringLiteral("200509031430:01.3+0200"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"), &zones, false);
    QVERIFY(!dt.isValid());    // matches paris and berlin

    const QString abbrev = paris.displayName(QTimeZone::DaylightTime, QTimeZone::ShortName, QLocale::c());
    dt = KADateTime::fromString(QStringLiteral("2005October051430 ")+abbrev, QStringLiteral("%Y%:B%d%H%M%:S %Z"), &zones, true);
    QCOMPARE(dt.date(), QDate(2005, 10, 5));
    QCOMPARE(dt.time(), QTime(14, 30, 0));
    QCOMPARE(dt.timeType(), KADateTime::OffsetFromUTC);
    QCOMPARE(dt.utcOffset(), 2 * 3600);
    dt = KADateTime::fromString(QStringLiteral("2005October051430 ")+abbrev, QStringLiteral("%Y%:B%d%H%M%:S %Z"), &zones, false);
    QVERIFY(!dt.isValid());    // matches paris and berlin

    // GMT is used by multiple time zones
    dt = KADateTime::fromString(QStringLiteral("30 October 2005 1:30 GMT"), QStringLiteral("%d %:B %Y %k:%M %Z"));
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 30, 0));
    QCOMPARE(dt.timeType(), KADateTime::UTC);
    QCOMPARE(dt.utcOffset(), 0);
    dt = KADateTime::fromString(QStringLiteral("30 October 2005 1:30 GMT"), QStringLiteral("%d %:B %Y %k:%M %Z"), &zones);
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 30, 0));
    QCOMPARE(dt.timeType(), KADateTime::TimeZone);
    QCOMPARE(dt.timeZone(), london);
    QCOMPARE(dt.utcOffset(), 0);
    dt = KADateTime::fromString(QStringLiteral("30 October 2005 1:30 BST"), QStringLiteral("%d %:B %Y %k:%M %Z"));
    QCOMPARE(dt.date(), QDate(2005, 10, 30));
    QCOMPARE(dt.time(), QTime(1, 30, 0));
    QCOMPARE(dt.timeType(), KADateTime::TimeZone);
    QCOMPARE(dt.timeZone(), london);
    QCOMPARE(dt.utcOffset(), 1 * 3600);

    dt = KADateTime::fromString(QStringLiteral("pm05aboCtobeRt/   052/   20:12,03+0100"), QStringLiteral("%:P%yab%:bt/  %e2/%t%S:%l,%M %z"), &zones);
    QCOMPARE(dt.date(), QDate(2005, 10, 5));
    QCOMPARE(dt.time(), QTime(12, 3, 20));
    QCOMPARE(dt.timeType(), KADateTime::TimeZone);
    QCOMPARE(dt.utcOffset(), 3600);
    QCOMPARE(dt.timeZone(), london);

    dt = KADateTime::fromString(QStringLiteral("2005aboCtt/022sun/20.0123456:12Am,3Africa/Cairo%"), QStringLiteral("%Yab%bt/%e2%a/%S%:s:%I%P,%:M %:Z%%"), &zones);
    QCOMPARE(dt.date(), QDate(2005, 10, 2));
    QCOMPARE(dt.time(), QTime(0, 3, 20, 12));
    QCOMPARE(dt.timeType(), KADateTime::TimeZone);
    QCOMPARE(dt.timeZone(), cairo);
    QCOMPARE(dt.utcOffset(), 2 * 3600);

    // Test large and minimum date values
    dt = KADateTime(QDate(-2005, 10, 3), QTime(0, 0, 06, 1), KADateTime::LocalZone);
    s = dt.toString(QStringLiteral("%Y"));
    QCOMPARE(s, QStringLiteral("-2005"));

    dt = KADateTime(QDate(-15, 10, 3), QTime(0, 0, 06, 1), KADateTime::LocalZone);
    s = dt.toString(QStringLiteral("%Y"));
    QCOMPARE(s, QStringLiteral("-0015"));

    dt = KADateTime::fromString(QStringLiteral("-471210031430:01.3+0500"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"));
    QCOMPARE(dt.date(), QDate(-4712, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 30, 1, 300));
    QCOMPARE(dt.utcOffset(), 5 * 3600);
    QVERIFY(dt.isValid());

    dt = KADateTime::fromString(QStringLiteral("999910031430:01.3+0500"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"));
    QCOMPARE(dt.date(), QDate(9999, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 30, 1, 300));
    QCOMPARE(dt.utcOffset(), 5 * 3600);
    QVERIFY(dt.isValid());

    dt = KADateTime::fromString(QStringLiteral("123456.10031430:01.3+0500"), QStringLiteral("%:Y.%m%d%H%M%:S%:s%z"));
    QCOMPARE(dt.date(), QDate(123456, 10, 3));
    QCOMPARE(dt.time(), QTime(14, 30, 1, 300));
    QCOMPARE(dt.utcOffset(), 5 * 3600);
    QVERIFY(dt.isValid());
    s = dt.toString(QStringLiteral("%Y"));
    QCOMPARE(s, QStringLiteral("123456"));

    dt = KADateTime::fromString(QStringLiteral("-471411231430:01.3+0500"), QStringLiteral("%Y%m%d%H%M%:S%:s%z"));
    QVERIFY(dt.isValid());
    QVERIFY(dt.date().toJulianDay() == -1);

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

#ifdef COMPILING_TESTS
// This test requires a specially-modified kalarmcal2, so use the same compile guard here
// as used in kalarmcal2/src/kadatetime.cpp
void KADateTimeTest::cache()
{
    QTimeZone london("Europe/London");
    QTimeZone losAngeles("America/Los_Angeles");
    QTimeZone cairo("Africa/Cairo");

    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":Europe/London");
    ::tzset();

    // Ensure that local time is different from UTC and different from 'london'
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    int utcHit  = KADateTime_utcCacheHit;
    int zoneHit = KADateTime_zoneCacheHit;
    KADateTime local(QDate(2005, 6, 1), QTime(12, 0, 0), KADateTime::LocalZone);
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt1 = local.toZone(london);
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime cai = local.toZone(cairo);
    ++utcHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt2a = local.toZone(london);
    ++utcHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt2 = local.toZone(london);
    ++zoneHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt3 = dt2;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt4 = dt2.toZone(losAngeles);
    ++zoneHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt4a = dt3.toZone(losAngeles);
    ++zoneHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt5 = dt2.toZone(losAngeles);
    ++zoneHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt5a = dt3.toZone(losAngeles);
    ++zoneHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt6 = dt2.toZone(cairo);
    ++utcHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt6a = dt3.toZone(cairo);
    ++zoneHit;
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    dt3.detach();
    KADateTime dt7 = dt2.toZone(london);
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);
    KADateTime dt7a = dt3.toZone(london);
    QCOMPARE(KADateTime_utcCacheHit, utcHit);
    QCOMPARE(KADateTime_zoneCacheHit, zoneHit);

    // Check that cached time zone conversions are cleared correctly
    KADateTime utc1(QDate(2005, 7, 6), QTime(3, 40, 0), KADateTime::UTC);
    KADateTime la1 = utc1.toTimeSpec(losAngeles);
    KADateTime utc2 = utc1.addDays(1);
    KADateTime la2 = utc2.toTimeSpec(losAngeles);
    QVERIFY(la1 != la2);
    QCOMPARE(la1.secsTo(la2), 86400);

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}
#endif /* COMPILING_TESTS */

void KADateTimeTest::stream()
{
    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    // Ensure that the original contents of the KADateTime receiving a streamed value
    // don't affect the new contents.
    QByteArray data;
    QDataStream ds(&data, QIODevice::ReadWrite);
    KADateTime testdt;
    KADateTime result;

    data.clear();
    testdt = KADateTime(QDate(2005, 6, 1), QTime(12, 0, 0), KADateTime::LocalZone);
    result = KADateTime::currentUtcDateTime();
    ds << testdt;
    ds.device()->seek(0);
    ds >> result;
    QCOMPARE(result, testdt);

    data.clear();
    testdt = KADateTime(QDate(2005, 6, 1), QTime(12, 0, 0), KADateTime::LocalZone);
    result = KADateTime::currentLocalDateTime();
    ds.device()->seek(0);
    ds << testdt;
    ds.device()->seek(0);
    ds >> result;
    QCOMPARE(result, testdt);

    data.clear();
    testdt = KADateTime(QDate(2006, 8, 30), QTime(7, 0, 0), KADateTime::UTC);
    result = KADateTime::currentUtcDateTime();
    ds.device()->seek(0);
    ds << testdt;
    ds.device()->seek(0);
    ds >> result;
    QCOMPARE(result, testdt);

    data.clear();
    testdt = KADateTime(QDate(2006, 8, 30), QTime(7, 0, 0), KADateTime::UTC);
    result = KADateTime::currentLocalDateTime();
    ds.device()->seek(0);
    ds << testdt;
    ds.device()->seek(0);
    ds >> result;
    QCOMPARE(result, testdt);

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

void KADateTimeTest::misc()
{
    // Ensure that local time is different from UTC and different from 'london'
    QByteArray originalZone = qgetenv("TZ");   // save the original local time zone
    qputenv("TZ", ":America/Los_Angeles");
    ::tzset();

    KADateTime local = KADateTime::currentLocalDateTime();
    KADateTime utc = KADateTime::currentUtcDateTime();
    QDateTime qcurrent = QDateTime::currentDateTime();
    // Because 3 calls to fetch the current time were made, they will differ slightly
    KADateTime localUtc = local.toUtc();
    int diff = localUtc.secsTo(utc);
    if (diff > 1  ||  diff < 0) {
        QCOMPARE(local.toUtc().date(), utc.date());
        QCOMPARE(local.toUtc().time(), utc.time());
    }
    diff = local.qDateTime().secsTo(qcurrent);
    if (diff > 1  ||  diff < 0) {
        QCOMPARE(local.qDateTime(), qcurrent);
    }

    // Restore the original local time zone
    if (originalZone.isEmpty()) {
        unsetenv("TZ");
    } else {
        qputenv("TZ", originalZone);
    }
    ::tzset();
}

// vim: et sw=4:
