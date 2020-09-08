//#define SIMULATION
/*
    This file is part of kalarmcal library, which provides access to KAlarm
    calendar data. Qt5 version of KDE 4 kdelibs/kdecore/date/kdatetime.cpp.

    SPDX-FileCopyrightText: 2005-2019 David Jarvie <djarvie@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kadatetime.h"

#include <QDateTime>
#include <QTimeZone>
#include <QRegExp>
#include <QStringList>
#include <QSharedData>
#include <QDataStream>
#include <QLocale>
#include <QDebug>

#include <limits>

namespace
{

const int InvalidOffset = 0x80000000;   // invalid UTC offset
const int NO_NUMBER = std::numeric_limits<int>::min();   // indicates that no number is present in string conversion functions

QList<QString> shortDayNames;
QList<QString> longDayNames;
QList<QString> shortMonthNames;
QList<QString> longMonthNames;

// Short day name, in English
const QString &shortDay(int day);   // Mon = 1, ...
// Long day name, in English
const QString &longDay(int day);   // Monday = 1, ...
// Short month name, in English
const QString &shortMonth(int month);   // Jan = 1, ...
// Long month name, in English
const QString &longMonth(int month);   // January = 1, ...

QDateTime fromStr(const QString &string, const QString &format, int &utcOffset,
                  QString &zoneName, QString &zoneAbbrev, bool &dateOnly);
int matchDay(const QString &string, int &offset, bool localised);
int matchMonth(const QString &string, int &offset, bool localised);
bool getUTCOffset(const QString &string, int &offset, bool colon, int &result);
int getAmPm(const QString &string, int &offset, bool localised);
bool getNumber(const QString &string, int &offset, int mindigits, int maxdigits, int minval, int maxval, int &result);
typedef const QString &(*DayMonthName)(int);
int findString(const QString &string, DayMonthName func, int count, int &offset);
// Return number as zero-padded numeric string.
QString numString(int n, int width);

int offsetAtZoneTime(const QTimeZone &tz, const QDateTime&, int *secondOffset = nullptr);
QDateTime toZoneTime(const QTimeZone &tz, const QDateTime &utcDateTime, bool *secondOccurrence = nullptr); 

} // namespace

namespace KAlarmCal
{

#ifdef COMPILING_TESTS
KALARMCAL_EXPORT int KADateTime_utcCacheHit  = 0;
KALARMCAL_EXPORT int KADateTime_zoneCacheHit = 0;
#endif

/*----------------------------------------------------------------------------*/

class KADateTimeSpecPrivate
{
public:
    // *** NOTE: This structure is replicated in KADateTimePrivate. Any changes must be copied there.
    QTimeZone tz;            // if type == TimeZone, the instance's time zone.
    int       utcOffset = 0;     // if type == OffsetFromUTC, the offset from UTC
    KADateTime::SpecType type;  // time spec type
};

KADateTime::Spec::Spec()
    : d(new KADateTimeSpecPrivate)
{
    d->type = KADateTime::Invalid;
}

KADateTime::Spec::Spec(const QTimeZone &tz)
    : d(new KADateTimeSpecPrivate())
{
    setType(tz);
}

KADateTime::Spec::Spec(SpecType type, int utcOffset)
    : d(new KADateTimeSpecPrivate())
{
    setType(type, utcOffset);
}

KADateTime::Spec::Spec(const Spec &spec)
    : d(new KADateTimeSpecPrivate())
{
    operator=(spec);
}

KADateTime::Spec::~Spec()
{
    delete d;
}

KADateTime::Spec &KADateTime::Spec::operator=(const Spec &spec)
{
    if (&spec != this) {
        d->type = spec.d->type;
        if (d->type == KADateTime::TimeZone)
            d->tz = spec.d->tz;
        else if (d->type == KADateTime::OffsetFromUTC)
            d->utcOffset = spec.d->utcOffset;
    }
    return *this;
}

void KADateTime::Spec::setType(SpecType type, int utcOffset)
{
    switch (type) {
        case KADateTime::OffsetFromUTC:
            d->utcOffset = utcOffset;
            // fall through to UTC
            Q_FALLTHROUGH();
        case KADateTime::UTC:
            d->type = type;
            break;
        case KADateTime::LocalZone:
            d->tz = QTimeZone::systemTimeZone();
            d->type = type;
            break;
        case KADateTime::TimeZone:
        default:
            d->type = KADateTime::Invalid;
            break;
    }
}

void KADateTime::Spec::setType(const QTimeZone &tz)
{
    if (tz == QTimeZone::utc())
        d->type = KADateTime::UTC;
    else if (tz.isValid()) {
        d->type = KADateTime::TimeZone;
        d->tz   = tz;
    }
    else
        d->type = KADateTime::Invalid;
}

QTimeZone KADateTime::Spec::timeZone() const
{
    switch (d->type) {
        case KADateTime::TimeZone:
            return d->tz;
        case KADateTime::UTC:
            return QTimeZone::utc();
        case KADateTime::LocalZone:
            return QTimeZone::systemTimeZone();
        default:
            return QTimeZone();
    }
}

bool KADateTime::Spec::isUtc() const
{
    if (d->type == KADateTime::UTC
    ||  (d->type == KADateTime::OffsetFromUTC  &&  d->utcOffset == 0))
        return true;
    return false;
}

KADateTime::Spec KADateTime::Spec::UTC()
{
    return Spec(KADateTime::UTC);
}
KADateTime::Spec KADateTime::Spec::LocalZone()
{
    return Spec(KADateTime::LocalZone);
}
KADateTime::Spec KADateTime::Spec::OffsetFromUTC(int utcOffset)
{
    return Spec(KADateTime::OffsetFromUTC, utcOffset);
}
KADateTime::SpecType KADateTime::Spec::type() const
{
    return d->type;
}
bool KADateTime::Spec::isValid() const
{
    return d->type != KADateTime::Invalid;
}
bool KADateTime::Spec::isLocalZone() const
{
    return d->type == KADateTime::LocalZone;
}
bool KADateTime::Spec::isOffsetFromUtc() const
{
    return d->type == KADateTime::OffsetFromUTC;
}
int  KADateTime::Spec::utcOffset() const
{
    return d->type == KADateTime::OffsetFromUTC ? d->utcOffset : 0;
}

bool KADateTime::Spec::operator==(const Spec &other) const
{
    if (d->type != other.d->type
    ||  (d->type == KADateTime::TimeZone  &&  d->tz != other.d->tz)
    ||  (d->type == KADateTime::OffsetFromUTC  &&  d->utcOffset != other.d->utcOffset))
        return false;
    return true;
}

bool KADateTime::Spec::equivalentTo(const Spec &other) const
{
    if (d->type == other.d->type) {
        if ((d->type == KADateTime::TimeZone  &&  d->tz != other.d->tz)
        ||  (d->type == KADateTime::OffsetFromUTC  &&  d->utcOffset != other.d->utcOffset))
            return false;
        return true;
    } else {
        if ((d->type == KADateTime::UTC  &&  other.d->type == KADateTime::OffsetFromUTC  &&  other.d->utcOffset == 0)
        ||  (other.d->type == KADateTime::UTC  &&  d->type == KADateTime::OffsetFromUTC  &&  d->utcOffset == 0))
            return true;
        const QTimeZone local = QTimeZone::systemTimeZone();
        if ((d->type == KADateTime::LocalZone  &&  other.d->type == KADateTime::TimeZone  &&  other.d->tz == local)
        ||  (other.d->type == KADateTime::LocalZone  &&  d->type == KADateTime::TimeZone  &&  d->tz == local))
            return true;
        return false;
    }
}

QDataStream &operator<<(QDataStream &s, const KADateTime::Spec &spec)
{
    // The specification type is encoded in order to insulate from changes
    // to the SpecType enum.
    switch (spec.type()) {
        case KADateTime::UTC:
            s << static_cast<quint8>('u');
            break;
        case KADateTime::OffsetFromUTC:
            s << static_cast<quint8>('o') << spec.utcOffset();
            break;
        case KADateTime::TimeZone:
            s << static_cast<quint8>('z') << (spec.timeZone().isValid() ? spec.timeZone().id() : QByteArray());
            break;
        case KADateTime::LocalZone:
            s << static_cast<quint8>('c');
            break;
        case KADateTime::Invalid:
        default:
            s << static_cast<quint8>(' ');
            break;
    }
    return s;
}

QDataStream &operator>>(QDataStream &s, KADateTime::Spec &spec)
{
    // The specification type is encoded in order to insulate from changes
    // to the SpecType enum.
    quint8 t;
    s >> t;
    switch (static_cast<char>(t)) {
        case 'u':
            spec.setType(KADateTime::UTC);
            break;
        case 'o': {
            int utcOffset;
            s >> utcOffset;
            spec.setType(KADateTime::OffsetFromUTC, utcOffset);
            break;
        }
        case 'z': {
            QByteArray zone;
            s >> zone;
            spec.setType(QTimeZone(zone));
            break;
        }
        case 'c':
            spec.setType(KADateTime::LocalZone);
            break;
        default:
            spec.setType(KADateTime::Invalid);
            break;
    }
    return s;
}

/*----------------------------------------------------------------------------*/

class KADateTimePrivate : public QSharedData
{
public:
    KADateTimePrivate()
        : QSharedData(),
          specType(KADateTime::Invalid),
          utcCached(true),
          convertedCached(false),
          m2ndOccurrence(false),
          mDateOnly(false)
    {}

    KADateTimePrivate(const QDate &d, const QTime &t, const KADateTime::Spec &s, bool donly = false)
        : QSharedData(),
          mDt(QDateTime(d, t, Qt::UTC)),
          specType(s.type()),
          utcCached(false),
          convertedCached(false),
          m2ndOccurrence(false),
          mDateOnly(donly)
    {
        setDtSpec(s);
    }

    KADateTimePrivate(const QDateTime &d, const KADateTime::Spec &s, bool donly = false)
        : QSharedData(),
          mDt(d),
          specType(s.type()),
          utcCached(false),
          convertedCached(false),
          m2ndOccurrence(false),
          mDateOnly(donly)
    {
        setDtSpec(s);
        setDateTime(d);
    }

    explicit KADateTimePrivate(const QDateTime &d)
        : QSharedData(),
          mDt(d),
          specType(KADateTime::Invalid),
          utcCached(false),
          convertedCached(false),
          m2ndOccurrence(false),
          mDateOnly(false)
    {
        switch (d.timeSpec()) {
            case Qt::UTC:
                specType = KADateTime::UTC;
                break;
            case Qt::OffsetFromUTC:
                specType = KADateTime::OffsetFromUTC;
                break;
            case Qt::TimeZone:
                specType = KADateTime::TimeZone;
                break;
            case Qt::LocalTime:
                specType = KADateTime::LocalZone;
                mDt.setTimeZone(QTimeZone::systemTimeZone());
                break;
        }
    }

    KADateTimePrivate(const KADateTimePrivate &rhs)
        : QSharedData(rhs),
          mDt(rhs.mDt),
          ut(rhs.ut),
          converted(rhs.converted),
          specType(rhs.specType),
          utcCached(rhs.utcCached),
          convertedCached(rhs.convertedCached),
          m2ndOccurrence(rhs.m2ndOccurrence),
          mDateOnly(rhs.mDateOnly),
          converted2ndOccur(rhs.converted2ndOccur)
    {}

    ~KADateTimePrivate()  {}
    const QDateTime &rawDt() const
    {
        return mDt;
    }
    QDateTime dt() const
    {
        if (specType == KADateTime::LocalZone) {
            return QDateTime(mDt.date(), mDt.time(), Qt::LocalTime);
        }
        return mDt;
    }
    QDateTime updatedDt(QTimeZone &local) const;
    const QDate date() const
    {
        return mDt.date();
    }
    const QTime time() const
    {
        return mDt.time();
    }
    KADateTime::Spec spec() const;
    QDateTime cachedUtc() const
    {
        return (specType != KADateTime::Invalid) ? QDateTime(ut.date, ut.time, Qt::UTC) : QDateTime();
    }
    bool dateOnly() const
    {
        return mDateOnly;
    }
    bool secondOccurrence() const
    {
        return m2ndOccurrence;
    }
    // Set mDt and its time spec, without changing timeSpec.
    // Condition: 'dt' time spec must correspond to timeSpec.
    void setDtWithSpec(const QDateTime &dt)
    {
        mDt = dt;
        utcCached = convertedCached = m2ndOccurrence = false;
    }
    // Set mDt and its time spec, without changing timeSpec.
    // Condition: 'dt' time spec must correspond to timeSpec.
    // 'utcDt' is the UTC equivalent of dt.
    void setDtWithSpec(const QDateTime &dt, const QDateTime &utcDt)
    {
        mDt = dt;
        ut.date = utcDt.date();
        ut.time = utcDt.time();
        utcCached = true;
        convertedCached = false;
        m2ndOccurrence = false;
    }

    void setDtSpec(const KADateTime::Spec &s);
    void setDateTime(const QDateTime &d);
    void setDate(const QDate &d)
    {
        mDt.setDate(d);
        utcCached = convertedCached = m2ndOccurrence = false;
    }
    void setTime(const QTime &t)
    {
        mDt.setTime(t);
        utcCached = convertedCached = mDateOnly = m2ndOccurrence = false;
    }
    void setSpec(const KADateTime::Spec&);
    void setDateOnly(bool d);
    QTimeZone timeZone() const
    {
        return specType == KADateTime::TimeZone ? mDt.timeZone() : QTimeZone();
    }
    QTimeZone timeZoneOrLocal() const
    {
        return (specType == KADateTime::TimeZone) ? mDt.timeZone()
             : (specType == KADateTime::LocalZone) ? QTimeZone::systemTimeZone() : QTimeZone();
    }
    int  timeZoneOffset(QTimeZone &local) const;
    QDateTime toUtc(QTimeZone &local) const;
    QDateTime toZone(const QTimeZone &zone, QTimeZone &local) const;
    void newToZone(KADateTimePrivate *newd, const QTimeZone &zone, QTimeZone &local) const;
    bool equalSpec(const KADateTimePrivate&) const;
    void clearCache()
    {
        utcCached = convertedCached = false;
    }
    void setCachedUtc(const QDateTime &dt) const
    {
        ut.date = dt.date();
        ut.time = dt.time();
        utcCached = true;
        convertedCached = false;
    }

    // Default time spec used by fromString()
    static KADateTime::Spec &fromStringDefault()
    {
        static KADateTime::Spec s_fromStringDefault(KADateTime::LocalZone);
        return s_fromStringDefault;
    }

    static QTime         sod;               // start of day (00:00:00)
#ifdef SIMULATION
#ifndef NDEBUG
    // For simulating the current time:
    static qint64        simulationOffset;    // offset to apply to current system time
    static QTimeZone     simulationLocalZone; // time zone to use
#endif
#endif

    /* Because some applications create thousands of instances of KADateTime, this
     * data structure is designed to minimize memory usage. Ensure that all small
     * members are kept together at the end!
     */
private:
    // This contains the Qt time spec, including QTimeZone or UTC offset.
    // For specType = LocalZone, it is set to the system time zone used to calculate
    // the cached UTC time, instead of Qt::LocalTime which doesn't handle historical
    // daylight savings times.
    QDateTime             mDt;
public:
    mutable struct ut {                // cached UTC equivalent of 'mDt'. Saves space compared to storing QDateTime.
        QDate             date;
        QTime             time;
    } ut;
private:
    mutable struct converted {         // cached conversion to another time zone (if 'tz' is valid)
        QDate             date;
        QTime             time;
        QTimeZone         tz;
    } converted;
public:
    KADateTime::SpecType   specType          : 4; // time spec type (N.B. need 3 bits + sign bit, since enums are signed on some platforms)
    mutable bool          utcCached         : 1; // true if 'ut' is valid
    mutable bool          convertedCached   : 1; // true if 'converted' is valid
    mutable bool          m2ndOccurrence    : 1; // this is the second occurrence of a time zone time
private:
    bool                  mDateOnly         : 1; // true to ignore the time part
    mutable bool          converted2ndOccur : 1; // this is the second occurrence of 'converted' time
};

QTime KADateTimePrivate::sod(0, 0, 0);
#ifdef SIMULATION
#ifndef NDEBUG
qint64    KADateTimePrivate::simulationOffset = 0;
QTimeZone KADateTimePrivate::simulationLocalZone;
#endif
#endif

KADateTime::Spec KADateTimePrivate::spec() const
{
    switch (specType) {
        case KADateTime::TimeZone:
            return KADateTime::Spec(mDt.timeZone());
        case KADateTime::OffsetFromUTC:
            return KADateTime::Spec(specType, mDt.offsetFromUtc());
        default:
            return KADateTime::Spec(specType);
    }
}

// Set mDt to the appropriate time spec for a given KADateTime::Spec.
// Its date and time components are not changed.
void KADateTimePrivate::setDtSpec(const KADateTime::Spec &s)
{
    switch (s.type()) {
        case KADateTime::UTC:
            mDt.setTimeSpec(Qt::UTC);
            break;
        case KADateTime::OffsetFromUTC:
            mDt.setOffsetFromUtc(s.utcOffset());
            break;
        case KADateTime::TimeZone:
            mDt.setTimeZone(s.timeZone());
            break;
        case KADateTime::LocalZone:
            mDt.setTimeZone(QTimeZone::systemTimeZone());
            break;
        case KADateTime::Invalid:
        default:
            break;
    }
}

void KADateTimePrivate::setSpec(const KADateTime::Spec &other)
{
    if (specType == other.type()) {
        switch (specType) {
            case KADateTime::TimeZone: {
                const QTimeZone tz = other.timeZone();
                if (mDt.timeZone() == tz)
                    return;
                mDt.setTimeZone(tz);
                break;
            }
            case KADateTime::OffsetFromUTC: {
                int offset = other.utcOffset();
                if (mDt.offsetFromUtc() == offset)
                    return;
                mDt.setOffsetFromUtc(offset);
                break;
            }
            default:
                return;
        }
    } else {
        specType = other.type();
        setDtSpec(other);
        if (specType == KADateTime::Invalid) {
            ut.date = QDate();   // cache an invalid UTC value
            utcCached = true;
            convertedCached = m2ndOccurrence = false;
            return;
        }
    }
    utcCached = convertedCached = m2ndOccurrence = false;
}

bool KADateTimePrivate::equalSpec(const KADateTimePrivate &other) const
{
    if (specType != other.specType
    ||  (specType == KADateTime::TimeZone  &&  mDt.timeZone() != other.mDt.timeZone())
    ||  (specType == KADateTime::OffsetFromUTC  &&  mDt.offsetFromUtc() != other.mDt.offsetFromUtc()))
        return false;
    return true;
}

/* Return mDt, updated to current system time zone if it's LocalZone.
 * Parameters:
 *   local - the local time zone if already known (if invalid, this function will
 *           fetch it if required)
 */
QDateTime KADateTimePrivate::updatedDt(QTimeZone &local) const
{
    Q_UNUSED(local)
    if (specType == KADateTime::LocalZone) {
        const QTimeZone local = QTimeZone::systemTimeZone();
        if (mDt.timeZone() != local) {
            const_cast<QDateTime*>(&mDt)->setTimeZone(local);
            utcCached = convertedCached = false;
        }
    }
    return mDt;
}

// Set the date/time without changing the time spec.
// 'd' is converted to the current time spec.
void KADateTimePrivate::setDateTime(const QDateTime &d)
{
    switch (d.timeSpec()) {
        case Qt::UTC:
            switch (specType) {
                case KADateTime::UTC:
                    setDtWithSpec(d);
                    break;
                case KADateTime::OffsetFromUTC:
                    setDtWithSpec(d.toOffsetFromUtc(mDt.offsetFromUtc()), d);
                    break;
                case KADateTime::LocalZone:
                case KADateTime::TimeZone: {
                    bool second = false;
                    setDtWithSpec(toZoneTime(mDt.timeZone(), d, &second), d);
                    m2ndOccurrence = second;
                    break;
                }
                default:    // invalid
                    break;
            }
            break;
        case Qt::OffsetFromUTC:
            setDateTime(d.toUTC());
            break;
        case Qt::TimeZone:
            switch (specType) {
                case KADateTime::UTC:
                    mDt               = d.toUTC();
                    utcCached         = false;
                    converted.date    = d.date();
                    converted.time    = d.time();
                    converted.tz      = d.timeZone();
                    convertedCached   = true;
                    converted2ndOccur = false;   // QDateTime::toUtc() returns the first occurrence
                    break;
                case KADateTime::OffsetFromUTC:
                    mDt               = d.toOffsetFromUtc(mDt.offsetFromUtc());
                    utcCached         = false;
                    converted.date    = d.date();
                    converted.time    = d.time();
                    converted.tz      = d.timeZone();
                    convertedCached   = true;
                    converted2ndOccur = false;   // QDateTime::toUtc() returns the first occurrence
                    break;
                case KADateTime::LocalZone:
                case KADateTime::TimeZone:
                    if (d.timeZone() == mDt.timeZone()) {
                        mDt               = d;
                        utcCached         = false;
                        convertedCached   = false;
                    } else {
                        mDt               = d.toTimeZone(mDt.timeZone());
                        utcCached         = false;
                        converted.date    = d.date();
                        converted.time    = d.time();
                        converted.tz      = d.timeZone();
                        convertedCached   = true;
                        converted2ndOccur = false;   // QDateTime::toUtc() returns the first occurrence
                    }
                    break;
                default:
                    break;
            }
            break;
        case Qt::LocalTime:
            // Qt::LocalTime doesn't handle historical daylight savings times,
            // so use the local time zone instead.
            setDateTime(QDateTime(d.date(), d.time(), QTimeZone::systemTimeZone()));
            break;
    }
}

void KADateTimePrivate::setDateOnly(bool dateOnly)
{
    if (dateOnly != mDateOnly) {
        mDateOnly = dateOnly;
        if (dateOnly  &&  mDt.time() != sod) {
            mDt.setTime(sod);
            utcCached = false;
            convertedCached = false;
        }
        m2ndOccurrence = false;
    }
}

/*
 * Returns the UTC offset for the date/time, provided that it is a time zone type.
 * Calculates and caches the UTC value.
 * Parameters:
 *   local - the local time zone if already known (if invalid, this function will
 *           fetch it if required)
 */
int KADateTimePrivate::timeZoneOffset(QTimeZone &local) const
{
    if (specType != KADateTime::TimeZone && specType != KADateTime::LocalZone)
        return InvalidOffset;
    QDateTime dt = updatedDt(local);   // update the cache if it's LocalZone
    if (utcCached) {
        dt.setTimeSpec(Qt::UTC);
        return cachedUtc().secsTo(dt);
    }
    int secondOffset;
    int offset = offsetAtZoneTime(mDt.timeZone(), mDt, &secondOffset);
    if (m2ndOccurrence) {
        m2ndOccurrence = (secondOffset != offset);   // cancel "second occurrence" flag if not applicable
        offset = secondOffset;
    }
    if (offset == InvalidOffset) {
        ut.date = QDate();   // cache an invalid UTC value
        utcCached = true;
        convertedCached = false;
    } else {
        // Calculate the UTC time from the offset and cache it
        QDateTime utcdt = mDt;
        utcdt.setTimeSpec(Qt::UTC);
        setCachedUtc(utcdt.addSecs(-offset));
    }
    return offset;
}

/*
 * Returns the date/time converted to UTC.
 * The calculated UTC value is cached, to save time in future conversions.
 * Parameters:
 *   local - the local time zone if already known (if invalid, this function will
 *           fetch it if required)
 */
QDateTime KADateTimePrivate::toUtc(QTimeZone &local) const
{
    updatedDt(local);   // update the cache if it's LocalZone
    if (utcCached) {
        // Return cached UTC value
        if (specType == KADateTime::LocalZone) {
            // LocalZone uses the dynamic current local system time zone.
            // Check for a time zone change before using the cached UTC value.
            if (!local.isValid())
                local = QTimeZone::systemTimeZone();
            if (mDt.timeZone() == local) {
//                qDebug() << "toUtc(): cached -> " << cachedUtc() << endl,
#ifdef COMPILING_TESTS
                ++KADateTime_utcCacheHit;
#endif
                return cachedUtc();
            }
            utcCached = false;
        } else {
//            qDebug() << "toUtc(): cached -> " << cachedUtc() << endl,
#ifdef COMPILING_TESTS
            ++KADateTime_utcCacheHit;
#endif
            return cachedUtc();
        }
    }

    // No cached UTC value, so calculate it
    switch (specType) {
        case KADateTime::UTC:
            return mDt;
        case KADateTime::OffsetFromUTC: {
            if (!mDt.isValid())
                break;
            const QDateTime dt = mDt.toUTC();
            setCachedUtc(dt);
//            qDebug() << "toUtc(): calculated -> " << dt << endl,
            return dt;
        }
        case KADateTime::LocalZone:   // mDt is set to the system time zone
        case KADateTime::TimeZone:
            if (!mDt.isValid())
                break;
            timeZoneOffset(local);   // calculate offset and cache UTC value
//            qDebug() << "toUtc(): calculated -> " << cachedUtc() << endl,
            return cachedUtc();
        default:
            break;
    }

    // Invalid - mark it cached to avoid having to process it again
    ut.date = QDate();    // (invalid)
    utcCached = true;
    convertedCached = false;
//    qDebug() << "toUtc(): invalid";
    return mDt;
}

/* Convert this value to another time zone.
 * The value is cached to save having to repeatedly calculate it.
 * The caller should check for an invalid date/time.
 * Parameters:
 *   zone  - the time zone to convert to
 *   local - the local time zone if already known (if invalid, this function will
 *           fetch it if required)
 */
QDateTime KADateTimePrivate::toZone(const QTimeZone &zone, QTimeZone &local) const
{
    updatedDt(local);   // update the cache if it's LocalZone
    if (convertedCached  &&  converted.tz == zone) {
        // Converted value is already cached
#ifdef COMPILING_TESTS
//        qDebug() << "KADateTimePrivate::toZone(" << zone->id() << "): " << mDt << " cached";
        ++KADateTime_zoneCacheHit;
#endif
        return QDateTime(converted.date, converted.time, zone);
    } else {
        // Need to convert the value
        bool second;
        const QDateTime result = toZoneTime(zone, toUtc(local), &second);
        converted.date    = result.date();
        converted.time    = result.time();
        converted.tz      = zone;
        convertedCached   = true;
        converted2ndOccur = second;
        return result;
    }
}

/* Convert this value to another time zone, and write it into the specified instance.
 * The value is cached to save having to repeatedly calculate it.
 * The caller should check for an invalid date/time.
 * Parameters:
 *   newd  - the instance to set equal to the converted value
 *   zone  - the time zone to convert to
 *   local - the local time zone if already known (if invalid, this function will
 *           fetch it if required)
 */
void KADateTimePrivate::newToZone(KADateTimePrivate *newd, const QTimeZone &zone, QTimeZone &local) const
{
    newd->mDt            = toZone(zone, local);
    newd->specType       = KADateTime::TimeZone;
    newd->utcCached      = utcCached;
    newd->mDateOnly      = mDateOnly;
    newd->m2ndOccurrence = converted2ndOccur;
    switch (specType) {
        case KADateTime::UTC:
            newd->ut.date = mDt.date();   // cache the UTC value
            newd->ut.time = mDt.time();
            break;
        case KADateTime::LocalZone:
        case KADateTime::TimeZone:
            // This instance is also type time zone, so cache its value in the new instance
            newd->converted.date    = mDt.date();
            newd->converted.time    = mDt.time();
            newd->converted.tz      = mDt.timeZone();
            newd->convertedCached   = true;
            newd->converted2ndOccur = m2ndOccurrence;
            newd->ut                = ut;
            return;
        case KADateTime::OffsetFromUTC:
        default:
            newd->ut = ut;
            break;
    }
    newd->convertedCached = false;
}

/*----------------------------------------------------------------------------*/
Q_GLOBAL_STATIC_WITH_ARGS(QSharedDataPointer<KADateTimePrivate>, emptyDateTimePrivate, (new KADateTimePrivate))

KADateTime::KADateTime()
    : d(*emptyDateTimePrivate())
{
}

KADateTime::KADateTime(const QDate &date, const Spec &spec)
    : d(new KADateTimePrivate(date, KADateTimePrivate::sod, spec, true))
{
}

KADateTime::KADateTime(const QDate &date, const QTime &time, const Spec &spec)
    : d(new KADateTimePrivate(date, time, spec))
{
}

KADateTime::KADateTime(const QDateTime &dt, const Spec &spec)
    : d(new KADateTimePrivate(dt, spec))
{
}

KADateTime::KADateTime(const QDateTime &dt)
    : d(new KADateTimePrivate(dt))
{
}

KADateTime::KADateTime(const KADateTime &other)
    : d(other.d)
{
}

KADateTime::~KADateTime()
{
}

KADateTime &KADateTime::operator=(const KADateTime &other)
{
    if (&other != this)
        d = other.d;
    return *this;
}

void KADateTime::detach()
{
    d.detach();
}
bool KADateTime::isNull() const
{
    return d->rawDt().isNull();
}
bool KADateTime::isValid() const
{
    return d->specType != Invalid  &&  d->rawDt().isValid();
}
bool KADateTime::isDateOnly() const
{
    return d->dateOnly();
}
bool KADateTime::isLocalZone() const
{
    return d->specType == LocalZone;
}
bool KADateTime::isUtc() const
{
    return d->specType == UTC || (d->specType == OffsetFromUTC && d->spec().utcOffset() == 0);
}
bool KADateTime::isOffsetFromUtc() const
{
    return d->specType == OffsetFromUTC;
}
bool KADateTime::isSecondOccurrence() const
{
    return d->specType == TimeZone && d->secondOccurrence();
}
QDate KADateTime::date() const
{
    return d->date();
}
QTime KADateTime::time() const
{
    return d->time();
}
QDateTime KADateTime::qDateTime() const
{
    return d->dt();
}

KADateTime::Spec KADateTime::timeSpec() const
{
    return d->spec();
}
KADateTime::SpecType KADateTime::timeType() const
{
    return d->specType;
}

QTimeZone KADateTime::timeZone() const
{
    switch (d->specType) {
        case UTC:
            return QTimeZone::utc();
        case TimeZone:
            return d->timeZone();
        case LocalZone:
            return QTimeZone::systemTimeZone();
        default:
            return QTimeZone();
    }
}

int KADateTime::utcOffset() const
{
    switch (d->specType) {
        case TimeZone:
        case LocalZone: {
            QTimeZone local;
            int offset = d->timeZoneOffset(local);   // calculate offset and cache UTC value
            return (offset == InvalidOffset) ? 0 : offset;
        }
        case OffsetFromUTC:
            return d->spec().utcOffset();
        case UTC:
        default:
            return 0;
    }
}

KADateTime KADateTime::toUtc() const
{
    if (!isValid())
        return KADateTime();
    if (d->specType == UTC)
        return *this;
    if (d->dateOnly())
        return KADateTime(d->date(), Spec(UTC));
    QTimeZone local;
    const QDateTime udt = d->toUtc(local);
    if (!udt.isValid())
        return KADateTime();
    return KADateTime(udt, UTC);
}

KADateTime KADateTime::toOffsetFromUtc() const
{
    if (!isValid())
        return KADateTime();
    int offset = 0;
    switch (d->specType) {
        case OffsetFromUTC:
            return *this;
        case UTC: {
            if (d->dateOnly())
                return KADateTime(d->date(), Spec(OffsetFromUTC, 0));
            QDateTime qdt = d->rawDt();
            return KADateTime(qdt.date(), qdt.time(), Spec(OffsetFromUTC, 0));
        }
        case TimeZone: {
            QTimeZone local;
            offset = d->timeZoneOffset(local);   // calculate offset and cache UTC value
            break;
        }
        case LocalZone: {
            QTimeZone local;
            const QDateTime dt = d->updatedDt(local);
            offset = offsetAtZoneTime(dt.timeZone(), dt);
            break;
        }
        default:
            return KADateTime();
    }
    if (offset == InvalidOffset)
        return KADateTime();
    if (d->dateOnly()) {
        return KADateTime(d->date(), Spec(OffsetFromUTC, offset));
    }
    return KADateTime(d->date(), d->time(), Spec(OffsetFromUTC, offset));
}

KADateTime KADateTime::toOffsetFromUtc(int utcOffset) const
{
    if (!isValid())
        return KADateTime();
    if (d->specType == OffsetFromUTC  &&   d->spec().utcOffset() == utcOffset)
        return *this;
    if (d->dateOnly())
        return KADateTime(d->date(), Spec(OffsetFromUTC, utcOffset));
    QTimeZone local;
    return KADateTime(d->toUtc(local), Spec(OffsetFromUTC, utcOffset));
}

KADateTime KADateTime::toLocalZone() const
{
    if (!isValid())
        return KADateTime();
    if (d->dateOnly())
        return KADateTime(d->date(), LocalZone);
    QTimeZone local = QTimeZone::systemTimeZone();
    if (d->specType == TimeZone  &&  d->timeZone() == local)
        return KADateTime(d->date(), d->time(), LocalZone);
    switch (d->specType) {
        case TimeZone:
        case OffsetFromUTC:
        case UTC: {
            KADateTime result;
            d->newToZone(result.d, local, local);  // cache the time zone conversion
            result.d->specType = LocalZone;
            return result;
        }
        case LocalZone:
            return *this;
        default:
            return KADateTime();
    }
}

KADateTime KADateTime::toZone(const QTimeZone &zone) const
{
    if (!zone.isValid()  ||  !isValid())
        return KADateTime();
    if (d->specType == TimeZone  &&  d->timeZone() == zone)
        return *this;    // preserve UTC cache, if any
    if (d->dateOnly())
        return KADateTime(d->date(), Spec(zone));
    KADateTime result;
    QTimeZone local;
    d->newToZone(result.d, zone, local);  // cache the time zone conversion
    return result;
}

KADateTime KADateTime::toTimeSpec(const KADateTime &dt) const
{
    return toTimeSpec(dt.timeSpec());
}

KADateTime KADateTime::toTimeSpec(const Spec &spec) const
{
    if (spec == d->spec())
        return *this;
    if (!isValid())
        return KADateTime();
    if (d->dateOnly())
        return KADateTime(d->date(), spec);
    if (spec.type() == TimeZone) {
        KADateTime result;
        QTimeZone local;
        d->newToZone(result.d, spec.timeZone(), local);  // cache the time zone conversion
        return result;
    }
    QTimeZone local;
    return KADateTime(d->toUtc(local), spec);
}

qint64 KADateTime::toSecsSinceEpoch() const
{
    QTimeZone local;
    const QDateTime qdt = d->toUtc(local);
    if (!qdt.isValid())
        return LLONG_MIN;
    return qdt.toSecsSinceEpoch();
}

uint KADateTime::toTime_t() const
{
    qint64 t = toSecsSinceEpoch();
    if (static_cast<quint64>(t) >= uint(-1))
        return uint(-1);
    return static_cast<uint>(t);
}

void KADateTime::setSecsSinceEpoch(qint64 seconds)
{
    QDateTime dt;
    dt.setTimeSpec(Qt::UTC);   // prevent QDateTime::setMSecsSinceEpoch() converting to local time
    dt.setMSecsSinceEpoch(seconds * 1000);
    d->specType = UTC;
    d->setDateOnly(false);
    d->setDtWithSpec(dt);
}

void KADateTime::setTime_t(qint64 seconds)
{
    setSecsSinceEpoch(seconds);
}

void KADateTime::setDateOnly(bool dateOnly)
{
    d->setDateOnly(dateOnly);
}

void KADateTime::setDate(const QDate &date)
{
    d->setDate(date);
}

void KADateTime::setTime(const QTime &time)
{
    d->setTime(time);
}

void KADateTime::setTimeSpec(const Spec &other)
{
    d->setSpec(other);
}

void KADateTime::setSecondOccurrence(bool second)
{
    if (d->specType == KADateTime::TimeZone  &&  second != d->m2ndOccurrence) {
        d->m2ndOccurrence = second;
        d->clearCache();
        if (second) {
            // Check whether a second occurrence is actually possible, and
            // if not, reset m2ndOccurrence.
            QTimeZone local;
            d->timeZoneOffset(local);   // check, and cache UTC value
        }
    }
}

KADateTime KADateTime::addMSecs(qint64 msecs) const
{
    if (!msecs)
        return *this;    // retain cache - don't create another instance
    if (!isValid())
        return KADateTime();
    if (d->dateOnly()) {
        KADateTime result(*this);
        result.d->setDate(d->date().addDays(msecs / 86400000));
        return result;
    }
    QTimeZone local;
    return KADateTime(d->toUtc(local).addMSecs(msecs), d->spec());
}

KADateTime KADateTime::addSecs(qint64 secs) const
{
    return addMSecs(secs * 1000);
}

KADateTime KADateTime::addDays(qint64 days) const
{
    if (!days)
        return *this;    // retain cache - don't create another instance
    KADateTime result(*this);
    result.d->setDate(d->date().addDays(days));
    return result;
}

KADateTime KADateTime::addMonths(int months) const
{
    if (!months)
        return *this;    // retain cache - don't create another instance
    KADateTime result(*this);
    result.d->setDate(d->date().addMonths(months));
    return result;
}

KADateTime KADateTime::addYears(int years) const
{
    if (!years)
        return *this;    // retain cache - don't create another instance
    KADateTime result(*this);
    result.d->setDate(d->date().addYears(years));
    return result;
}

qint64 KADateTime::msecsTo(const KADateTime &t2) const
{
    if (!isValid() || !t2.isValid())
        return 0;
    if (d->dateOnly()) {
        const QDate dat = t2.d->dateOnly() ? t2.d->date() : t2.toTimeSpec(d->spec()).d->date();
        return d->date().daysTo(dat) * 86400*1000;
    }
    if (t2.d->dateOnly())
        return toTimeSpec(t2.d->spec()).d->date().daysTo(t2.d->date()) * 86400*1000;
    QTimeZone local;
    return d->toUtc(local).msecsTo(t2.d->toUtc(local));
}

qint64 KADateTime::secsTo(const KADateTime &t2) const
{
    if (!isValid() || !t2.isValid())
        return 0;
    if (d->dateOnly()) {
        const QDate dat = t2.d->dateOnly() ? t2.d->date() : t2.toTimeSpec(d->spec()).d->date();
        return d->date().daysTo(dat) * 86400;
    }
    if (t2.d->dateOnly())
        return toTimeSpec(t2.d->spec()).d->date().daysTo(t2.d->date()) * 86400;
    QTimeZone local;
    return d->toUtc(local).secsTo(t2.d->toUtc(local));
}

qint64 KADateTime::daysTo(const KADateTime &t2) const
{
    if (!isValid() || !t2.isValid())
        return 0;
    if (d->dateOnly()) {
        const QDate dat = t2.d->dateOnly() ? t2.d->date() : t2.toTimeSpec(d->spec()).d->date();
        return d->date().daysTo(dat);
    }
    if (t2.d->dateOnly())
        return toTimeSpec(t2.d->spec()).d->date().daysTo(t2.d->date());

    QDate dat;
    QTimeZone local;
    switch (d->specType) {
        case UTC:
            dat = t2.d->toUtc(local).date();
            break;
        case OffsetFromUTC:
            dat = t2.d->toUtc(local).addSecs(d->spec().utcOffset()).date();
            break;
        case TimeZone:
            dat = t2.d->toZone(d->timeZone(), local).date();   // this caches the converted time in t2
            break;
        case LocalZone:
            local = QTimeZone::systemTimeZone();
            dat = t2.d->toZone(local, local).date();   // this caches the converted time in t2
            break;
        default:    // invalid
            return 0;
    }
    return d->date().daysTo(dat);
}

KADateTime KADateTime::currentLocalDateTime()
{
#ifdef SIMULATION
#ifndef NDEBUG
    if (KADateTimePrivate::simulationLocalZone.isValid()) {
        KADateTime dt = currentUtcDateTime().toZone(KADateTimePrivate::simulationLocalZone);
        dt.setSpec(LocalZone);
        return dt;
    }
    if (KADateTimePrivate::simulationOffset) {
        KADateTime dt = currentUtcDateTime().toZone(QTimeZone::systemTimeZone());
        dt.setSpec(LocalZone);
        return dt;
    }
#endif
#endif
    return KADateTime(QDateTime::currentDateTime(), LocalZone);
}

KADateTime KADateTime::currentUtcDateTime()
{
    const KADateTime result(QDateTime::currentDateTimeUtc(), UTC);
#ifndef NDEBUG
#ifdef SIMULATION
    return result.addSecs(KADateTimePrivate::simulationOffset);
#else
    return result;
#endif
#else
    return result;
#endif
}

KADateTime KADateTime::currentDateTime(const Spec &spec)
{
    switch (spec.type()) {
        case UTC:
            return currentUtcDateTime();
        case TimeZone:
            if (spec.timeZone() != QTimeZone::systemTimeZone())
                break;
            // fall through to LocalZone
            Q_FALLTHROUGH();
        case LocalZone:
            return currentLocalDateTime();
        default:
            break;
    }
    return currentUtcDateTime().toTimeSpec(spec);
}

QDate KADateTime::currentLocalDate()
{
    return currentLocalDateTime().date();
}

QTime KADateTime::currentLocalTime()
{
    return currentLocalDateTime().time();
}

KADateTime::Comparison KADateTime::compare(const KADateTime &other) const
{
    QDateTime start1, start2;
    QTimeZone local;
    const bool conv = (!d->equalSpec(*other.d) || d->secondOccurrence() != other.d->secondOccurrence());
    if (conv) {
        // Different time specs or one is a time which occurs twice,
        // so convert to UTC before comparing
        start1 = d->toUtc(local);
        start2 = other.d->toUtc(local);
    } else {
        // Same time specs, so no need to convert to UTC
        start1 = d->dt();
        start2 = other.d->dt();
    }
    if (d->dateOnly() || other.d->dateOnly()) {
        // At least one of the instances is date-only, so we need to compare
        // time periods rather than just times.
        QDateTime end1, end2;
        if (conv) {
            if (d->dateOnly()) {
                KADateTime kdt(*this);
                kdt.setTime(QTime(23, 59, 59, 999));
                end1 = kdt.d->toUtc(local);
            }
            else
                end1 = start1;
            if (other.d->dateOnly()) {
                KADateTime kdt(other);
                kdt.setTime(QTime(23, 59, 59, 999));
                end2 = kdt.d->toUtc(local);
            }
            else
                end2 = start2;
        } else {
            end1 = d->dt();
            if (d->dateOnly())
                end1.setTime(QTime(23, 59, 59, 999));
            end2 = other.d->dt();
            if (other.d->dateOnly())
                end2.setTime(QTime(23, 59, 59, 999));
        }
        if (start1 == start2)
            return !d->dateOnly() ? AtStart
                 : (end1 == end2) ? Equal
                 : (end1 < end2) ? static_cast<Comparison>(AtStart | Inside)
                 : static_cast<Comparison>(AtStart | Inside | AtEnd | After);
        if (start1 < start2)
            return (end1 < start2) ? Before
                 : (end1 == end2) ? static_cast<Comparison>(Before | AtStart | Inside | AtEnd)
                 : (end1 == start2) ? static_cast<Comparison>(Before | AtStart)
                 : (end1 < end2) ? static_cast<Comparison>(Before | AtStart | Inside) : Outside;
        else
            return (start1 > end2) ? After
                 : (start1 == end2) ? (end1 == end2 ? AtEnd : static_cast<Comparison>(AtEnd | After))
                 : (end1 == end2) ? static_cast<Comparison>(Inside | AtEnd)
                 : (end1 < end2) ? Inside : static_cast<Comparison>(Inside | AtEnd | After);
    }
    return (start1 == start2) ? Equal : (start1 < start2) ? Before : After;
}

bool KADateTime::operator==(const KADateTime &other) const
{
    if (d == other.d)
        return true;    // the two instances share the same data
    if (d->dateOnly() != other.d->dateOnly())
        return false;
    if (d->equalSpec(*other.d)) {
        // Both instances are in the same time zone, so compare directly
        if (d->dateOnly())
            return d->date() == other.d->date();
        else
            return d->secondOccurrence() == other.d->secondOccurrence()
               &&  d->dt() == other.d->dt();
    }
    // Don't waste time converting to UTC if the dates aren't close enough.
    if (qAbs(d->date().daysTo(other.d->date())) > 2)
        return false;
    QTimeZone local;
    if (d->dateOnly()) {
        // Date-only values are equal if both the start and end of day times are equal.
        if (d->toUtc(local) != other.d->toUtc(local))
            return false;    // start-of-day times differ
        KADateTime end1(*this);
        end1.setTime(QTime(23, 59, 59, 999));
        KADateTime end2(other);
        end2.setTime(QTime(23, 59, 59, 999));
        return end1.d->toUtc(local) == end2.d->toUtc(local);
    }
    return d->toUtc(local) == other.d->toUtc(local);
}

bool KADateTime::operator<(const KADateTime &other) const
{
    if (d == other.d)
        return false;    // the two instances share the same data
    if (d->equalSpec(*other.d)) {
        // Both instances are in the same time zone, so compare directly
        if (d->dateOnly() || other.d->dateOnly())
            return d->date() < other.d->date();
        if (d->secondOccurrence() == other.d->secondOccurrence())
            return d->dt() < other.d->dt();
        // One is the second occurrence of a date/time, during a change from
        // daylight saving to standard time, so only do a direct comparison
        // if the dates are more than 1 day apart.
        const int dayDiff = d->date().daysTo(other.d->date());
        if (dayDiff > 1)
            return true;
        if (dayDiff < -1)
            return false;
    } else {
        // Don't waste time converting to UTC if the dates aren't close enough.
        const int dayDiff = d->date().daysTo(other.d->date());
        if (dayDiff > 2)
            return true;
        if (dayDiff < -2)
            return false;
    }
    QTimeZone local;
    if (d->dateOnly()) {
        // This instance is date-only, so we need to compare the end of its
        // day with the other value. Note that if the other value is date-only,
        // we want to compare with the start of its day, which will happen
        // automatically.
        KADateTime kdt(*this);
        kdt.setTime(QTime(23, 59, 59, 999));
        return kdt.d->toUtc(local) < other.d->toUtc(local);
    }
    return d->toUtc(local) < other.d->toUtc(local);
}

QString KADateTime::toString(const QString &format) const
{
    if (!isValid())
        return QString();

    enum { TZNone, UTCOffsetShort, UTCOffset, UTCOffsetColon, TZAbbrev, TZName };
    const QLocale locale;
    QString result;
    int num, numLength, zone;
    bool escape = false;
    ushort flag = 0;
    for (int i = 0, end = format.length();  i < end;  ++i) {
        zone = TZNone;
        num = NO_NUMBER;
        numLength = 0;    // no leading zeroes
        ushort ch = format[i].unicode();
        if (!escape) {
            if (ch == '%')
                escape = true;
            else
                result += format[i];
            continue;
        }
        if (!flag) {
            switch (ch) {
                case '%':
                    result += QLatin1Char('%');
                    break;
                case ':':
                    flag = ch;
                    break;
                case 'Y':     // year
                    num = d->date().year();
                    numLength = 4;
                    break;
                case 'y':     // year, 2 digits
                    num = d->date().year() % 100;
                    numLength = 2;
                    break;
                case 'm':     // month, 01 - 12
                    numLength = 2;
                    num = d->date().month();
                    break;
                case 'B':     // month name, translated
                    result += locale.monthName(d->date().month(), QLocale::LongFormat);
                    break;
                case 'b':     // month name, translated, short
                    result += locale.monthName(d->date().month(), QLocale::ShortFormat);
                    break;
                case 'd':     // day of month, 01 - 31
                    numLength = 2;
                    // fall through to 'e'
                    Q_FALLTHROUGH();
                case 'e':     // day of month, 1 - 31
                    num = d->date().day();
                    break;
                case 'A':     // week day name, translated
                    result += locale.dayName(d->date().dayOfWeek(), QLocale::LongFormat);
                    break;
                case 'a':     // week day name, translated, short
                    result += locale.dayName(d->date().dayOfWeek(), QLocale::ShortFormat);
                    break;
                case 'H':     // hour, 00 - 23
                    numLength = 2;
                    // fall through to 'k'
                    Q_FALLTHROUGH();
                case 'k':     // hour, 0 - 23
                    num = d->time().hour();
                    break;
                case 'I':     // hour, 01 - 12
                    numLength = 2;
                    // fall through to 'l'
                    Q_FALLTHROUGH();
                case 'l':     // hour, 1 - 12
                    num = (d->time().hour() + 11) % 12 + 1;
                    break;
                case 'M':     // minutes, 00 - 59
                    num = d->time().minute();
                    numLength = 2;
                    break;
                case 'S':     // seconds, 00 - 59
                    num = d->time().second();
                    numLength = 2;
                    break;
                case 'P': {   // am/pm
                    bool am = (d->time().hour() < 12);
                    QString text = am ? locale.amText() : locale.pmText();
                    if      (text == QLatin1String("a.m."))  text = QStringLiteral("am");
                    else if (text == QLatin1String("p.m."))  text = QStringLiteral("pm");
                    result += text.toLower();
                    break;
                }
                case 'p': {   // AM/PM
                    bool am = (d->time().hour() < 12);
                    QString text = am ? locale.amText() : locale.pmText();
                    if      (text == QLatin1String("a.m."))  text = QStringLiteral("am");
                    else if (text == QLatin1String("p.m."))  text = QStringLiteral("pm");
                    result += text.toUpper();
                    break;
                }
                case 'z':     // UTC offset in hours and minutes
                    zone = UTCOffset;
                    break;
                case 'Z':     // time zone abbreviation
                    zone = TZAbbrev;
                    break;
                default:
                    result += QLatin1Char('%');
                    result += format[i];
                    break;
            }
        }
        else if (flag == ':') {
            // It's a "%:" sequence
            switch (ch) {
                case 'A':     // week day name in English
                    result += longDay(d->date().dayOfWeek());
                    break;
                case 'a':     // week day name in English, short
                    result += shortDay(d->date().dayOfWeek());
                    break;
                case 'B':     // month name in English
                    result += longMonth(d->date().month());
                    break;
                case 'b':     // month name in English, short
                    result += shortMonth(d->date().month());
                    break;
                case 'm':     // month, 1 - 12
                    num = d->date().month();
                    break;
                case 'P':     // am/pm
                    result += (d->time().hour() < 12) ? QLatin1String("am") : QLatin1String("pm");
                    break;
                case 'p':     // AM/PM
                    result += (d->time().hour() < 12) ? QLatin1String("AM") : QLatin1String("PM");
                    break;
                case 'S': {   // seconds with ':' prefix, only if non-zero
                    int sec = d->time().second();
                    if (sec || d->time().msec()) {
                        result += QLatin1Char(':');
                        num = sec;
                        numLength = 2;
                    }
                    break;
                }
                case 's':     // milliseconds
                    result += numString(d->time().msec(), 3);
                    break;
                case 'u':     // UTC offset in hours
                    zone = UTCOffsetShort;
                    break;
                case 'z':     // UTC offset in hours and minutes, with colon
                    zone = UTCOffsetColon;
                    break;
                case 'Z':     // time zone name
                    zone = TZName;
                    break;
                default:
                    result += QLatin1String("%:");
                    result += format[i];
                    break;
            }
            flag = 0;
        }
        if (!flag)
            escape = false;

        // Append any required number or time zone information
        if (num != NO_NUMBER) {
            if (!numLength)
                result += QString::number(num);
            else if (numLength == 2 || numLength == 4) {
                if (num < 0) {
                    num = -num;
                    result += QLatin1Char('-');
                }
                result += numString(num, (numLength == 2 ? 2 : 4));
            }
        } else if (zone != TZNone) {
            QTimeZone tz;
            int offset;
            switch (d->specType) {
                case UTC:
                case TimeZone:
                case LocalZone:
                    switch (d->specType) {
                        case UTC:
                            tz = QTimeZone::utc();
                            break;
                        case TimeZone:
                            tz = d->timeZone();
                            break;
                        case LocalZone:
                            tz = QTimeZone::systemTimeZone();
                            break;
                        default:
                            break;
                    }
                    // fall through to OffsetFromUTC
                    Q_FALLTHROUGH();
                case OffsetFromUTC: {
                    QTimeZone local;
                    offset = (d->specType == TimeZone || d->specType == LocalZone) ? d->timeZoneOffset(local)
                           : (d->specType == OffsetFromUTC) ? d->spec().utcOffset() : 0;
                    if (offset == InvalidOffset)
                        return result + QLatin1String("+ERROR");
                    offset /= 60;
                    switch (zone) {
                        case UTCOffsetShort:  // UTC offset in hours
                        case UTCOffset:       // UTC offset in hours and minutes
                        case UTCOffsetColon: {  // UTC offset in hours and minutes, with colon
                            if (offset >= 0) {
                                result += QLatin1Char('+');
                            } else {
                                result += QLatin1Char('-');
                                offset = -offset;
                            }
                            result += numString(offset / 60, 2);
                            if (zone == UTCOffsetColon)
                                result += QLatin1Char(':');
                            if (ch != 'u'  ||  offset % 60)
                                result += numString(offset % 60, 2);
                            break;
                        }
                        case TZAbbrev:     // time zone abbreviation
                            if (tz.isValid()  &&  d->specType != OffsetFromUTC)
                                result += tz.abbreviation(d->toUtc(local));
                            break;
                        case TZName:       // time zone name
                            if (tz.isValid()  &&  d->specType != OffsetFromUTC)
                                result += QString::fromLatin1(tz.id());
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    return result;
}

QString KADateTime::toString(TimeFormat format) const
{
    QString result;
    if (!d->rawDt().isValid())
        return result;

    QString tzsign = QStringLiteral("+");
    int offset = 0;
    QString tzcolon;
    switch (format) {
        case RFCDateDay:
            result += shortDay(d->date().dayOfWeek());
            result += QLatin1String(", ");
            // fall through to RFCDate
            Q_FALLTHROUGH();
        case RFCDate: {
            QString seconds;
            if (d->time().second())
                seconds = QLatin1String(":") + numString(d->time().second(), 2);
            result += QStringLiteral("%1 %2 ").arg(numString(d->date().day(),   2))
                                              .arg(shortMonth(d->date().month()));
            int year = d->date().year();
            if (year < 0) {
                result += QLatin1Char('-');
                year = -year;
            }
            result += QStringLiteral("%1 %2:%3%4 ").arg(numString(year,                    4))
                                                   .arg(numString(d->time().hour(),   2))
                                                   .arg(numString(d->time().minute(), 2))
                                                   .arg(seconds);
            break;
        }
        case RFC3339Date: {
            result += QStringLiteral("%1-%2-%3T%4:%5:%6")
                                      .arg(numString(d->date().year(),        4))
                                      .arg(numString(d->date().month(),       2))
                                      .arg(numString(d->date().day(),         2))
                                      .arg(numString(d->time().hour(),   2))
                                      .arg(numString(d->time().minute(), 2))
                                      .arg(numString(d->time().second(), 2));
            int msec = d->time().msec();
            if (msec) {
                int digits = 3;
                if (!(msec % 10))
                    msec /= 10, --digits;
                if (!(msec % 10))
                    msec /= 10, --digits;
                result += QStringLiteral(".%1").arg(numString(d->time().msec(), digits));
            }
            if (d->specType == UTC)
                return result + QLatin1Char('Z');
            tzcolon = QStringLiteral(":");
            break;
        }
        case ISODate:
        case ISODateFull: {
            // QDateTime::toString(Qt::ISODate) doesn't output fractions of a second
            int year = d->date().year();
            if (year < 0) {
                result += QLatin1Char('-');
                year = -year;
            }
            result += QStringLiteral("%1-%2-%3").arg(numString(year,              4), numString(d->date().month(), 2), numString(d->date().day(),   2));
            if (!d->dateOnly()  ||  d->specType != LocalZone) {
                result += QStringLiteral("T%1:%2:%3").arg(numString(d->time().hour(),   2), numString(d->time().minute(), 2), numString(d->time().second(), 2));
                if (d->time().msec()) {
                    // Comma is preferred by ISO8601 as the decimal point symbol,
                    // so use it unless '.' is the symbol used in this locale.
                    result += (QLocale().decimalPoint() == QLatin1Char('.')) ? QLatin1Char('.') : QLatin1Char(',');
                    result += numString(d->time().msec(), 3);
                }
            }
            if (d->specType == UTC)
                return result + QLatin1Char('Z');
            if (format == ISODate && d->specType == LocalZone)
                return result;
            tzcolon = QStringLiteral(":");
            break;
        }
        case QtTextDate:
            if (d->dateOnly())
                result = toString(QStringLiteral("%a %b %e %Y"));
            else
                result = toString(QStringLiteral("%a %b %e %H:%M:%S %Y"));
            if (result.isEmpty()  ||  d->specType == LocalZone)
                return result;
            result += QLatin1Char(' ');
            break;

        case LocalDate: {
            QLocale l;
            if (d->dateOnly())
                result = l.toString(d->date(), QLocale::ShortFormat);
            else
                result = l.toString(d->dt(), QLocale::ShortFormat);
            if (result.isEmpty()  ||  d->specType == LocalZone)
                return result;
            result += QLatin1Char(' ');
            break;
        }
        default:
            return result;
    }

    // Return the string with UTC offset ±hhmm appended
    if (d->specType == OffsetFromUTC)
        offset =  d->spec().utcOffset();
    else if (d->specType == TimeZone || d->specType == LocalZone) {
        QTimeZone local;
        offset = d->timeZoneOffset(local);   // calculate offset and cache UTC value
    }
    if (d->specType == Invalid || offset == InvalidOffset)
        return result + QLatin1String("+ERROR");
    if (offset < 0) {
        offset = -offset;
        tzsign = QStringLiteral("-");
    }
    offset /= 60;
    return result + tzsign + numString(offset / 60, 2)
                  + tzcolon + numString(offset % 60, 2);
}

KADateTime KADateTime::fromString(const QString &string, TimeFormat format, bool *negZero)
{
    if (negZero)
        *negZero = false;
    const QString str = string.trimmed();
    if (str.isEmpty())
        return KADateTime();

    switch (format) {
        case RFCDateDay: // format is Wdy, DD Mon YYYY hh:mm:ss ±hhmm
        case RFCDate: {  // format is [Wdy,] DD Mon YYYY hh:mm[:ss] ±hhmm
            int nyear  = 6;   // indexes within string to values
            int nmonth = 4;
            int nday   = 2;
            int nwday  = 1;
            int nhour  = 7;
            int nmin   = 8;
            int nsec   = 9;
            // Also accept obsolete form "Weekday, DD-Mon-YY HH:MM:SS ±hhmm"
            QRegExp rx(QLatin1String("^(?:([A-Z][a-z]+),\\s*)?(\\d{1,2})(\\s+|-)([^-\\s]+)(\\s+|-)(\\d{2,4})\\s+(\\d\\d):(\\d\\d)(?::(\\d\\d))?\\s+(\\S+)$"));
            QStringList parts_;
            if (!rx.indexIn(str)) {
                // Check that if date has '-' separators, both separators are '-'.
                parts_ = rx.capturedTexts();
                bool h1 = (parts_.at(3) == QLatin1String("-"));
                bool h2 = (parts_.at(5) == QLatin1String("-"));
                if (h1 != h2)
                    break;
            } else {
                // Check for the obsolete form "Wdy Mon DD HH:MM:SS YYYY"
                rx = QRegExp(QLatin1String("^([A-Z][a-z]+)\\s+(\\S+)\\s+(\\d\\d)\\s+(\\d\\d):(\\d\\d):(\\d\\d)\\s+(\\d\\d\\d\\d)$"));
                if (rx.indexIn(str))
                    break;
                nyear  = 7;
                nmonth = 2;
                nday   = 3;
                nwday  = 1;
                nhour  = 4;
                nmin   = 5;
                nsec   = 6;
                parts_ = rx.capturedTexts();
            }
            bool ok[4];
            const QStringList& parts(parts_);
            int day    = parts[nday].toInt(&ok[0]);
            int year   = parts[nyear].toInt(&ok[1]);
            int hour   = parts[nhour].toInt(&ok[2]);
            int minute = parts[nmin].toInt(&ok[3]);
            if (!ok[0] || !ok[1] || !ok[2] || !ok[3])
                break;
            int second = 0;
            if (!parts[nsec].isEmpty()) {
                second = parts[nsec].toInt(&ok[0]);
                if (!ok[0])
                    break;
            }
            bool leapSecond = (second == 60);
            if (leapSecond)
                second = 59;    // apparently a leap second - validate below, once time zone is known
            int month = 0;
            for (;  month < 12  && parts[nmonth] != shortMonth(month + 1);  ++month);
            int dayOfWeek = -1;
            if (!parts[nwday].isEmpty()) {
                // Look up the weekday name
                while (++dayOfWeek < 7  && shortDay(dayOfWeek + 1) != parts[nwday]);
                if (dayOfWeek >= 7)
                    for (dayOfWeek = 0;  dayOfWeek < 7  &&  longDay(dayOfWeek + 1) != parts[nwday];  ++dayOfWeek);
            }
            if (month >= 12 || dayOfWeek >= 7
            ||  (dayOfWeek < 0  &&  format == RFCDateDay))
                break;
            int i = parts[nyear].size();
            if (i < 4) {
                // It's an obsolete year specification with less than 4 digits
                year += (i == 2  &&  year < 50) ? 2000 : 1900;
            }

            // Parse the UTC offset part
            int offset = 0;           // set default to '-0000'
            bool negOffset = false;
            if (parts.count() > 10) {
                rx = QRegExp(QLatin1String("^([+-])(\\d\\d)(\\d\\d)$"));
                if (!rx.indexIn(parts[10])) {
                    // It's a UTC offset ±hhmm
                    const QStringList partsu = rx.capturedTexts();
                    offset = partsu[2].toInt(&ok[0]) * 3600;
                    int offsetMin = partsu[3].toInt(&ok[1]);
                    if (!ok[0] || !ok[1] || offsetMin > 59)
                        break;
                    offset += offsetMin * 60;
                    negOffset = (partsu[1] == QLatin1String("-"));
                    if (negOffset)
                        offset = -offset;
                } else {
                    // Check for an obsolete time zone name
                    const QByteArray zone = parts[10].toLatin1();
                    if (zone.length() == 1  &&  isalpha(zone[0])  &&  toupper(zone[0]) != 'J')
                        negOffset = true;    // military zone: RFC 2822 treats as '-0000'
                    else if (zone != "UT" && zone != "GMT") { // treated as '+0000'
                        offset = (zone == "EDT")                  ? -4 * 3600
                               : (zone == "EST" || zone == "CDT") ? -5 * 3600
                               : (zone == "CST" || zone == "MDT") ? -6 * 3600
                               : (zone == "MST" || zone == "PDT") ? -7 * 3600
                               : (zone == "PST")                  ? -8 * 3600
                               : 0;
                        if (!offset) {
                            // Check for any other alphabetic time zone
                            bool nonalpha = false;
                            for (int i = 0, end = zone.size();  i < end && !nonalpha;  ++i)
                                nonalpha = !isalpha(zone[i]);
                            if (nonalpha)
                                break;
                            // TODO: Attempt to recognize the time zone abbreviation?
                            negOffset = true;    // unknown time zone: RFC 2822 treats as '-0000'
                        }
                    }
                }
            }
            const QDate qdate(year, month + 1, day);
            if (!qdate.isValid())
                break;
            KADateTime result(qdate, QTime(hour, minute, second), Spec(OffsetFromUTC, offset));
            if (!result.isValid()
            ||  (dayOfWeek >= 0  &&  result.date().dayOfWeek() != dayOfWeek + 1))
                break;    // invalid date/time, or weekday doesn't correspond with date
            if (!offset) {
                if (negOffset && negZero)
                    *negZero = true;    // UTC offset given as "-0000"
                result.setTimeSpec(UTC);
            }
            if (leapSecond) {
                // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
                // Convert the time to UTC and check that it is 00:00:00.
                if ((hour * 3600 + minute * 60 + 60 - offset + 86400 * 5) % 86400)  // (max abs(offset) is 100 hours)
                    break;    // the time isn't the last second of the day
            }
            return result;
        }
        case RFC3339Date: { // format is YYYY-MM-DDThh:mm:ss[.s]TZ
            const QRegExp rx(QLatin1String("^(\\d{4})-(\\d\\d)-(\\d\\d)[Tt](\\d\\d):(\\d\\d):(\\d\\d)(?:\\.(\\d+))?([Zz]|([+-])(\\d\\d):(\\d\\d))$"));
            if (rx.indexIn(str))
                break;
            const QStringList parts = rx.capturedTexts();
            bool ok, ok1, ok2;
            int msecs  = 0;
            bool leapSecond = false;
            int year = parts[1].toInt(&ok);
            int month = parts[2].toInt(&ok1);
            int day = parts[3].toInt(&ok2);
            if (!ok || !ok1 || !ok2)
                break;
            const QDate d(year, month, day);
            if (!d.isValid())
                break;
            int hour = parts[4].toInt(&ok);
            int minute = parts[5].toInt(&ok1);
            int second = parts[6].toInt(&ok2);
            if (!ok || !ok1 || !ok2)
                break;
            leapSecond = (second == 60);
            if (leapSecond)
                second = 59;    // apparently a leap second - validate below, once time zone is known
            if (!parts[7].isEmpty()) {
                QString ms = parts[7] + QLatin1String("00");
                ms.truncate(3);
                msecs = ms.toInt(&ok);
                if (!ok)
                    break;
                if (msecs && leapSecond)
                    break;    // leap second only valid if 23:59:60.000
            }
            const QTime t(hour, minute, second, msecs);
            if (!t.isValid())
                break;
            int offset = 0;
            SpecType spec = (parts[8].toUpper() == QLatin1Char('Z')) ? UTC : OffsetFromUTC;
            if (spec == OffsetFromUTC) {
                offset = parts[10].toInt(&ok) * 3600;
                offset += parts[11].toInt(&ok1) * 60;
                if (!ok || !ok1)
                    break;
                if (parts[9] == QLatin1String("-")) {
                    if (!offset && leapSecond)
                        break;    // leap second only valid if known time zone
                    offset = -offset;
                    if (!offset && negZero)
                        *negZero = true;
                }
            }
            if (leapSecond) {
                // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
                // Convert the time to UTC and check that it is 00:00:00.
                if ((hour * 3600 + minute * 60 + 60 - offset + 86400 * 5) % 86400)  // (max abs(offset) is 100 hours)
                    break;    // the time isn't the last second of the day
            }
            return KADateTime(d, t, Spec(spec, offset));
        }
        case ISODate: {
            /*
             * Extended format: [±]YYYY-MM-DD[Thh[:mm[:ss.s]][TZ]]
             * Basic format:    [±]YYYYMMDD[Thh[mm[ss.s]][TZ]]
             * Extended format: [±]YYYY-DDD[Thh[:mm[:ss.s]][TZ]]
             * Basic format:    [±]YYYYDDD[Thh[mm[ss.s]][TZ]]
             * In the first three formats, the year may be expanded to more than 4 digits.
             *
             * QDateTime::fromString(Qt::ISODate) is a rather limited implementation
             * of parsing ISO 8601 format date/time strings, so it isn't used here.
             * This implementation isn't complete either, but it's better.
             *
             * ISO 8601 allows truncation, but for a combined date & time, the date part cannot
             * be truncated from the right, and the time part cannot be truncated from the left.
             * In other words, only the outer parts of the string can be omitted.
             * The standard does not actually define how to interpret omitted parts - it is up
             * to those interchanging the data to agree on a scheme.
             */
            bool dateOnly = false;
            // Check first for the extended format of ISO 8601
            QRegExp rx(QLatin1String("^([+-])?(\\d{4,})-(\\d\\d\\d|\\d\\d-\\d\\d)[T ](\\d\\d)(?::(\\d\\d)(?::(\\d\\d)(?:(?:\\.|,)(\\d+))?)?)?(Z|([+-])(\\d\\d)(?::(\\d\\d))?)?$"));
            if (rx.indexIn(str)) {
                // It's not the extended format - check for the basic format
                rx = QRegExp(QLatin1String("^([+-])?(\\d{4,})(\\d{4})[T ](\\d\\d)(?:(\\d\\d)(?:(\\d\\d)(?:(?:\\.|,)(\\d+))?)?)?(Z|([+-])(\\d\\d)(\\d\\d)?)?$"));
                if (rx.indexIn(str)) {
                    rx = QRegExp(QLatin1String("^([+-])?(\\d{4})(\\d{3})[T ](\\d\\d)(?:(\\d\\d)(?:(\\d\\d)(?:(?:\\.|,)(\\d+))?)?)?(Z|([+-])(\\d\\d)(\\d\\d)?)?$"));
                    if (rx.indexIn(str)) {
                        // Check for date-only formats
                        dateOnly = true;
                        rx = QRegExp(QLatin1String("^([+-])?(\\d{4,})-(\\d\\d\\d|\\d\\d-\\d\\d)$"));
                        if (rx.indexIn(str)) {
                            // It's not the extended format - check for the basic format
                            rx = QRegExp(QLatin1String("^([+-])?(\\d{4,})(\\d{4})$"));
                            if (rx.indexIn(str)) {
                                rx = QRegExp(QLatin1String("^([+-])?(\\d{4})(\\d{3})$"));
                                if (rx.indexIn(str))
                                    break;
                            }
                        }
                    }
                }
            }
            const QStringList parts = rx.capturedTexts();
            bool ok, ok1;
            QDate d;
            int hour   = 0;
            int minute = 0;
            int second = 0;
            int msecs  = 0;
            bool leapSecond = false;
            int year = parts[2].toInt(&ok);
            if (!ok)
                break;
            if (parts[1] == QLatin1String("-"))
                year = -year;
            if (!dateOnly) {
                hour = parts[4].toInt(&ok);
                if (!ok)
                    break;
                if (!parts[5].isEmpty()) {
                    minute = parts[5].toInt(&ok);
                    if (!ok)
                        break;
                }
                if (!parts[6].isEmpty()) {
                    second = parts[6].toInt(&ok);
                    if (!ok)
                        break;
                }
                leapSecond = (second == 60);
                if (leapSecond)
                    second = 59;    // apparently a leap second - validate below, once time zone is known
                if (!parts[7].isEmpty()) {
                    QString ms = parts[7] + QLatin1String("00");
                    ms.truncate(3);
                    msecs = ms.toInt(&ok);
                    if (!ok)
                        break;
                }
            }
            int month, day;
            if (parts[3].length() == 3) {
                // A day of the year is specified
                day = parts[3].toInt(&ok);
                if (!ok || day < 1 || day > 366)
                    break;
                d = QDate(year, 1, 1).addDays(day - 1);
                if (!d.isValid()  || (d.year() != year))
                    break;
                day   = d.day();
                month = d.month();
            } else {
                // A month and day are specified
                month = parts[3].leftRef(2).toInt(&ok);
                day   = parts[3].rightRef(2).toInt(&ok1);
                if (!ok || !ok1)
                    break;
                d = QDate(year, month, day);
                if (!d.isValid())
                    break;
            }
            if (dateOnly)
                return KADateTime(d, Spec(LocalZone));
            if (hour == 24  && !minute && !second && !msecs) {
                // A time of 24:00:00 is allowed by ISO 8601, and means midnight at the end of the day
                d = d.addDays(1);
                hour = 0;
            }

            QTime t(hour, minute, second, msecs);
            if (!t.isValid())
                break;
            if (parts[8].isEmpty()) {
                // No UTC offset is specified. Don't try to validate leap seconds.
                return KADateTime(d, t, KADateTimePrivate::fromStringDefault());
            }
            int offset = 0;
            SpecType spec = (parts[8] == QLatin1Char('Z')) ? UTC : OffsetFromUTC;
            if (spec == OffsetFromUTC) {
                offset = parts[10].toInt(&ok) * 3600;
                if (!ok)
                    break;
                if (!parts[11].isEmpty()) {
                    offset += parts[11].toInt(&ok) * 60;
                    if (!ok)
                        break;
                }
                if (parts[9] == QLatin1String("-")) {
                    offset = -offset;
                    if (!offset && negZero)
                        *negZero = true;
                }
            }
            if (leapSecond) {
                // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
                // Convert the time to UTC and check that it is 00:00:00.
                if ((hour * 3600 + minute * 60 + 60 - offset + 86400 * 5) % 86400)  // (max abs(offset) is 100 hours)
                    break;    // the time isn't the last second of the day
            }
            return KADateTime(d, t, Spec(spec, offset));
        }
        case QtTextDate: {  // format is Wdy Mth DD [hh:mm:ss] YYYY [±hhmm]
            int offset = 0;
            QRegExp rx(QLatin1String("^(\\S+\\s+\\S+\\s+\\d\\d\\s+(\\d\\d:\\d\\d:\\d\\d\\s+)?\\d\\d\\d\\d)\\s*(.*)$"));
            if (rx.indexIn(str) < 0)
                break;
            const QStringList parts = rx.capturedTexts();
            QDate     qd;
            QDateTime qdt;
            bool dateOnly = parts[2].isEmpty();
            if (dateOnly) {
                qd = QDate::fromString(parts[1], Qt::TextDate);
                if (!qd.isValid())
                    break;
            } else {
                qdt = QDateTime::fromString(parts[1], Qt::TextDate);
                if (!qdt.isValid())
                    break;
            }
            if (parts[3].isEmpty()) {
                // No time zone offset specified, so return a local clock time
                if (dateOnly) {
                    return KADateTime(qd, KADateTimePrivate::fromStringDefault());
                } else {
                    // Do it this way to prevent UTC conversions changing the time
                    return KADateTime(qdt.date(), qdt.time(), KADateTimePrivate::fromStringDefault());
                }
            }
            rx = QRegExp(QLatin1String("([+-])([\\d][\\d])(?::?([\\d][\\d]))?$"));
            if (rx.indexIn(parts[3]) < 0)
                break;

            // Extract the UTC offset at the end of the string
            bool ok;
            const QStringList parts2 = rx.capturedTexts();
            offset = parts2[2].toInt(&ok) * 3600;
            if (!ok)
                break;
            if (parts2.count() > 3) {
                offset += parts2[3].toInt(&ok) * 60;
                if (!ok)
                    break;
            }
            if (parts2[1] == QLatin1String("-")) {
                offset = -offset;
                if (!offset && negZero)
                    *negZero = true;
            }
            if (dateOnly)
                return KADateTime(qd, Spec((offset ? OffsetFromUTC : UTC), offset));
            return KADateTime(qdt.date(), qdt.time(), Spec((offset ? OffsetFromUTC : UTC), offset));
        }
        case LocalDate:
        default:
            break;
    }
    return KADateTime();
}

KADateTime KADateTime::fromString(const QString &string, const QString &format,
                                const QList<QTimeZone>* zones, bool offsetIfAmbiguous)
{
    int     utcOffset = 0;    // UTC offset in seconds
    bool    dateOnly = false;
    QString zoneName;
    QString zoneAbbrev;
    QDateTime qdt = fromStr(string, format, utcOffset, zoneName, zoneAbbrev, dateOnly);
    if (!qdt.isValid())
        return KADateTime();
    if (zones) {
        // Try to find a time zone match
        bool zname = false;
        const QList<QTimeZone>& zoneList = *zones;
        QTimeZone zone;
        if (!zoneName.isEmpty()) {
            // A time zone name has been found.
            // Use the time zone with that name.
            const QByteArray name = zoneName.toLatin1();
            for (const QTimeZone& tz : zoneList) {
                if (tz.id() == name) {
                    zone = tz;
                    zname = true;
                    break;
                }
            }
        } else if (!zoneAbbrev.isEmpty()) {
            // A time zone abbreviation has been found.
            // Use the time zone which contains it, if any, provided that the
            // abbreviation applies at the specified date/time.
            bool useUtcOffset = false;
            for (const QTimeZone& tz : zoneList) {
                qdt.setTimeZone(tz);
                // TODO: This may not find abbreviations for second occurrence of
                // the time zone time, after a daylight savings time shift.
                if (tz.abbreviation(qdt) == zoneAbbrev) {
                    int offset2;
                    int offset = offsetAtZoneTime(tz, qdt, &offset2);
                    if (offset == InvalidOffset)
                        return KADateTime();
                    // Found a time zone which uses this abbreviation at the specified date/time
                    if (zone.isValid()) {
                        // Abbreviation is used by more than one time zone
                        if (!offsetIfAmbiguous  ||  offset != utcOffset)
                            return KADateTime();
                        useUtcOffset = true;
                    } else {
                        zone = tz;
                        utcOffset = offset;
                    }
                }
            }
            if (useUtcOffset) {
                zone = QTimeZone();
                if (!utcOffset)
                    qdt.setTimeSpec(Qt::UTC);
            }
            else
                zname = true;
        }
        else if (utcOffset  ||  qdt.timeSpec() == Qt::UTC) {
            // A UTC offset has been found.
            // Use the time zone which contains it, if any.
            // For a date-only value, use the start of the day.
            QDateTime dtUTC = qdt;
            dtUTC.setTimeSpec(Qt::UTC);
            dtUTC = dtUTC.addSecs(-utcOffset);
            for (const QTimeZone& tz : zoneList) {
                if (tz.offsetFromUtc(dtUTC) == utcOffset) {
                    // Found a time zone which uses this offset at the specified time
                    if (zone.isValid()  ||  !utcOffset) {
                        // UTC offset is used by more than one time zone
                        if (!offsetIfAmbiguous)
                            return KADateTime();
                        if (dateOnly)
                            return KADateTime(qdt.date(), Spec(OffsetFromUTC, utcOffset));
                        return KADateTime(qdt.date(), qdt.time(), Spec(OffsetFromUTC, utcOffset));
                    }
                    zone = tz;
                }
            }
        }
        if (!zone.isValid() && zname)
            return KADateTime();    // an unknown zone name or abbreviation was found
        if (zone.isValid()) {
            if (dateOnly)
                return KADateTime(qdt.date(), Spec(zone));
            return KADateTime(qdt.date(), qdt.time(), Spec(zone));
        }
    } else {
        // Try to find a time zone match
        bool zname = false;
        QTimeZone zone;
        if (!zoneName.isEmpty()) {
            // A time zone name has been found.
            // Use the time zone with that name.
            zone = QTimeZone(zoneName.toLatin1());
            zname = true;
        } else if (!zoneAbbrev.isEmpty()) {
            // A time zone abbreviation has been found.
            // Use the time zone which contains it, if any, provided that the
            // abbreviation applies at the specified date/time.
            bool useUtcOffset = false;
            const QList<QByteArray> zoneIds = QTimeZone::availableTimeZoneIds();
            for (const QByteArray& zoneId : zoneIds) {
                const QTimeZone z(zoneId);
                qdt.setTimeZone(z);
                if (z.abbreviation(qdt) == zoneAbbrev) {
                    // TODO: This may not find abbreviations for second occurrence of
                    // the time zone time, after a daylight savings time shift.
                    int offset2;
                    int offset = offsetAtZoneTime(z, qdt, &offset2);
                    if (offset == InvalidOffset)
                        return KADateTime();
                    // Found a time zone which uses this abbreviation at the specified date/time
                    if (zone.isValid()) {
                        // Abbreviation is used by more than one time zone
                        if (!offsetIfAmbiguous  ||  offset != utcOffset)
                            return KADateTime();
                        useUtcOffset = true;
                    } else {
                        zone = z;
                        utcOffset = offset;
                    }
                }
            }
            if (useUtcOffset) {
                zone = QTimeZone();
                if (!utcOffset)
                    qdt.setTimeSpec(Qt::UTC);
            }
            else
                zname = true;
        }
        else if (utcOffset  ||  qdt.timeSpec() == Qt::UTC) {
            // A UTC offset has been found.
            // Use the time zone which contains it, if any.
            // For a date-only value, use the start of the day.
            QDateTime dtUTC = qdt;
            dtUTC.setTimeSpec(Qt::UTC);
            dtUTC = dtUTC.addSecs(-utcOffset);
            const QList<QByteArray> zoneIds = QTimeZone::availableTimeZoneIds();
            for (const QByteArray& zoneId : zoneIds) {
                const QTimeZone z(zoneId);
                if (z.offsetFromUtc(dtUTC) == utcOffset) {
                    // Found a time zone which uses this offset at the specified time
                    if (zone.isValid()  ||  !utcOffset) {
                        // UTC offset is used by more than one time zone
                        if (!offsetIfAmbiguous)
                            return KADateTime();
                        if (dateOnly)
                            return KADateTime(qdt.date(), Spec(OffsetFromUTC, utcOffset));
                        return KADateTime(qdt.date(), qdt.time(), Spec(OffsetFromUTC, utcOffset));
                    }
                    zone = z;
                }
            }
        }
        if (!zone.isValid() && zname)
            return KADateTime();    // an unknown zone name or abbreviation was found
        if (zone.isValid()) {
            if (dateOnly)
                return KADateTime(qdt.date(), Spec(zone));
            return KADateTime(qdt.date(), qdt.time(), Spec(zone));
        }
    }

    // No time zone match was found
    KADateTime result;
    if (utcOffset) {
        result = KADateTime(qdt.date(), qdt.time(), Spec(OffsetFromUTC, utcOffset));
    } else if (qdt.timeSpec() == Qt::UTC) {
        result = KADateTime(qdt.date(), qdt.time(), UTC);
    } else {
        result = KADateTime(qdt.date(), qdt.time(), Spec(LocalZone));
        result.setTimeSpec(KADateTimePrivate::fromStringDefault());
    }
    if (dateOnly)
        result.setDateOnly(true);
    return result;
}

void KADateTime::setFromStringDefault(const Spec &spec)
{
    KADateTimePrivate::fromStringDefault() = spec;
}

void KADateTime::setSimulatedSystemTime(const KADateTime &newTime)
{
    Q_UNUSED(newTime);
#ifdef SIMULATION
#ifndef NDEBUG
    if (newTime.isValid()) {
        KADateTimePrivate::simulationOffset = realCurrentLocalDateTime().secsTo_long(newTime);
        KADateTimePrivate::simulationLocalZone = newTime.timeZone();
    } else {
        KADateTimePrivate::simulationOffset = 0;
        KADateTimePrivate::simulationLocalZone = QTimeZone();
    }
#endif
#endif
}

KADateTime KADateTime::realCurrentLocalDateTime()
{
    return KADateTime(QDateTime::currentDateTime(), Spec(QTimeZone::systemTimeZone()));
}

QDataStream &operator<<(QDataStream &s, const KADateTime &dt)
{
    s << dt.date() << dt.time() << dt.timeSpec() << quint8(dt.isDateOnly() ? 0x01 : 0x00);
    return s;
}

QDataStream &operator>>(QDataStream &s, KADateTime &kdt)
{
    QDate d;
    QTime t;
    KADateTime::Spec spec;
    quint8 flags;
    s >> d >> t >> spec >> flags;
    if (flags & 0x01)
        kdt = KADateTime(d, spec);
    else
        kdt = KADateTime(d, t, spec);
    return s;
}

} // namespace KAlarmCal

using KAlarmCal::KADateTime;

namespace
{

/*
 * Extracts a QDateTime from a string, given a format string.
 * The date/time is set to Qt::UTC if a zero UTC offset is found,
 * otherwise it is Qt::LocalTime. If Qt::LocalTime is returned and
 * utcOffset == 0, that indicates that no UTC offset was found.
 */
QDateTime fromStr(const QString &string, const QString &format, int &utcOffset,
                  QString &zoneName, QString &zoneAbbrev, bool &dateOnly)
{
    const QString str = string.simplified();
    int year      = NO_NUMBER;
    int month     = NO_NUMBER;
    int day       = NO_NUMBER;
    int dayOfWeek = NO_NUMBER;
    int hour      = NO_NUMBER;
    int minute    = NO_NUMBER;
    int second    = NO_NUMBER;
    int millisec  = NO_NUMBER;
    int ampm      = NO_NUMBER;
    int tzoffset  = NO_NUMBER;
    zoneName.clear();
    zoneAbbrev.clear();

    enum { TZNone, UTCOffset, UTCOffsetColon, TZAbbrev, TZName };
    int zone;
    int s = 0;
    int send = str.length();
    bool escape = false;
    ushort flag = 0;
    for (int f = 0, fend = format.length();  f < fend && s < send;  ++f) {
        zone = TZNone;
        ushort ch = format[f].unicode();
        if (!escape) {
            if (ch == '%')
                escape = true;
            else if (format[f].isSpace()) {
                if (str[s].isSpace())
                    ++s;
            }
            else if (format[f] == str[s])
                ++s;
            else
                return QDateTime();
            continue;
        }
        if (!flag) {
            switch (ch) {
                case '%':
                    if (str[s++] != QLatin1Char('%'))
                        return QDateTime();
                    break;
                case ':':
                    flag = ch;
                    break;
                case 'Y':     // full year, 4 digits
                    if (!getNumber(str, s, 4, 4, NO_NUMBER, -1, year))
                        return QDateTime();
                    break;
                case 'y':     // year, 2 digits
                    if (!getNumber(str, s, 2, 2, 0, 99, year))
                        return QDateTime();
                    year += (year <= 50) ? 2000 : 1999;
                    break;
                case 'm':     // month, 2 digits, 01 - 12
                    if (!getNumber(str, s, 2, 2, 1, 12, month))
                        return QDateTime();
                    break;
                case 'B':
                case 'b': {   // month name, translated or English
                    int m = matchMonth(str, s, true);
                    if (m <= 0  || (month != NO_NUMBER && month != m))
                        return QDateTime();
                    month = m;
                    break;
                }
                case 'd':     // day of month, 2 digits, 01 - 31
                    if (!getNumber(str, s, 2, 2, 1, 31, day))
                        return QDateTime();
                    break;
                case 'e':     // day of month, 1 - 31
                    if (!getNumber(str, s, 1, 2, 1, 31, day))
                        return QDateTime();
                    break;
                case 'A':
                case 'a': {   // week day name, translated or English
                    int dow = matchDay(str, s, true);
                    if (dow <= 0  || (dayOfWeek != NO_NUMBER && dayOfWeek != dow))
                        return QDateTime();
                    dayOfWeek = dow;
                    break;
                }
                case 'H':     // hour, 2 digits, 00 - 23
                    if (!getNumber(str, s, 2, 2, 0, 23, hour))
                        return QDateTime();
                    break;
                case 'k':     // hour, 0 - 23
                    if (!getNumber(str, s, 1, 2, 0, 23, hour))
                        return QDateTime();
                    break;
                case 'I':     // hour, 2 digits, 01 - 12
                    if (!getNumber(str, s, 2, 2, 1, 12, hour))
                        return QDateTime();
                    break;
                case 'l':     // hour, 1 - 12
                    if (!getNumber(str, s, 1, 2, 1, 12, hour))
                        return QDateTime();
                    break;
                case 'M':     // minutes, 2 digits, 00 - 59
                    if (!getNumber(str, s, 2, 2, 0, 59, minute))
                        return QDateTime();
                    break;
                case 'S':     // seconds, 2 digits, 00 - 59
                    if (!getNumber(str, s, 2, 2, 0, 59, second))
                        return QDateTime();
                    break;
                case 's':     // seconds, 0 - 59
                    if (!getNumber(str, s, 1, 2, 0, 59, second))
                        return QDateTime();
                    break;
                case 'P':
                case 'p': {   // am/pm
                    int ap = getAmPm(str, s, true);
                    if (!ap  || (ampm != NO_NUMBER && ampm != ap))
                        return QDateTime();
                    ampm = ap;
                    break;
                }
                case 'z':     // UTC offset in hours and optionally minutes
                    zone = UTCOffset;
                    break;
                case 'Z':     // time zone abbreviation
                    zone = TZAbbrev;
                    break;
                case 't':     // whitespace
                    if (str[s++] != QLatin1Char(' '))
                        return QDateTime();
                    break;
                default:
                    if (s + 2 > send
                    ||  str[s++] != QLatin1Char('%')
                    ||  str[s++] != format[f])
                        return QDateTime();
                    break;
            }
        }
        else if (flag == ':') {
            // It's a "%:" sequence
            switch (ch) {
                case 'Y':     // full year, >= 4 digits
                    if (!getNumber(str, s, 4, 100, NO_NUMBER, -1, year))
                        return QDateTime();
                    break;
                case 'A':
                case 'a': {   // week day name in English
                    int dow = matchDay(str, s, false);
                    if (dow <= 0  || (dayOfWeek != NO_NUMBER && dayOfWeek != dow))
                        return QDateTime();
                    dayOfWeek = dow;
                    break;
                }
                case 'B':
                case 'b': {   // month name in English
                    int m = matchMonth(str, s, false);
                    if (m <= 0  || (month != NO_NUMBER && month != m))
                        return QDateTime();
                    month = m;
                    break;
                }
                case 'm':     // month, 1 - 12
                    if (!getNumber(str, s, 1, 2, 1, 12, month))
                        return QDateTime();
                    break;
                case 'P':
                case 'p': {   // am/pm in English
                    int ap = getAmPm(str, s, false);
                    if (!ap  || (ampm != NO_NUMBER && ampm != ap))
                        return QDateTime();
                    ampm = ap;
                    break;
                }
                case 'M':     // minutes, 0 - 59
                    if (!getNumber(str, s, 1, 2, 0, 59, minute))
                        return QDateTime();
                    break;
                case 'S':     // seconds with ':' prefix, defaults to zero
                    if (str[s] != QLatin1Char(':')) {
                        second = 0;
                        break;
                    }
                    ++s;
                    if (!getNumber(str, s, 1, 2, 0, 59, second))
                        return QDateTime();
                    break;
                case 's': {   // milliseconds, with decimal point prefix
                    if (str[s] != QLatin1Char('.')) {
                        // If no locale, try comma, it is preferred by ISO8601 as the decimal point symbol
                        const QChar dpt = QLocale().decimalPoint();
                        if (!str.midRef(s).startsWith(dpt))
                            return QDateTime();
                    }
                    ++s;
                    if (s >= send)
                        return QDateTime();
                    QString val = str.mid(s);
                    int i = 0;
                    for (int end = val.length();  i < end && val.at(i).isDigit();  ++i);
                    if (!i)
                        return QDateTime();
                    val.truncate(i);
                    val += QLatin1String("00");
                    val.truncate(3);
                    int ms = val.toInt();
                    if (millisec != NO_NUMBER && millisec != ms) 
                        return QDateTime();
                    millisec = ms;
                    s += i;
                    break;
                }
                case 'u':     // UTC offset in hours and optionally minutes
                    zone = UTCOffset;
                    break;
                case 'z':     // UTC offset in hours and minutes, with colon
                    zone = UTCOffsetColon;
                    break;
                case 'Z':     // time zone name
                    zone = TZName;
                    break;
                default:
                    if (s + 3 > send
                    ||  str[s++] != QLatin1Char('%')
                    ||  str[s++] != QLatin1Char(':')
                    ||  str[s++] != format[f])
                        return QDateTime();
                    break;
            }
            flag = 0;
        }
        if (!flag)
            escape = false;

        if (zone != TZNone) {
            // Read time zone or UTC offset
            switch (zone) {
                case UTCOffset:
                case UTCOffsetColon:
                    if (!zoneAbbrev.isEmpty() || !zoneName.isEmpty())
                        return QDateTime();
                    if (!getUTCOffset(str, s, (zone == UTCOffsetColon), tzoffset))
                        return QDateTime();
                    break;
                case TZAbbrev: {   // time zone abbreviation
                    if (tzoffset != NO_NUMBER || !zoneName.isEmpty())
                        return QDateTime();
                    int start = s;
                    while (s < send && str[s].isLetterOrNumber())
                        ++s;
                    if (s == start)
                        return QDateTime();
                    const QString z = str.mid(start, s - start);
                    if (!zoneAbbrev.isEmpty()  &&  z != zoneAbbrev)
                        return QDateTime();
                    zoneAbbrev = z;
                    break;
                }
                case TZName: {     // time zone name
                    if (tzoffset != NO_NUMBER || !zoneAbbrev.isEmpty())
                        return QDateTime();
                    QString z;
                    if (f + 1 >= fend) {
                        z = str.mid(s);
                        s = send;
                    } else {
                        // Get the terminating character for the zone name
                        QChar endchar = format[f + 1];
                        if (endchar == QLatin1Char('%')  &&  f + 2 < fend) {
                            const QChar endchar2 = format[f + 2];
                            if (endchar2 == QLatin1Char('n') || endchar2 == QLatin1Char('t'))
                                endchar = QLatin1Char(' ');
                        }
                        // Extract from the input string up to the terminating character
                        int start = s;
                        for (;  s < send && str[s] != endchar;  ++s);
                        if (s == start)
                            return QDateTime();
                        z = str.mid(start, s - start);
                    }
                    if (!zoneName.isEmpty()  &&  z != zoneName)
                        return QDateTime();
                    zoneName = z;
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (year == NO_NUMBER)
        year = KADateTime::currentLocalDate().year();
    if (month == NO_NUMBER)
        month = 1;
    QDate d = QDate(year, month, (day > 0 ? day : 1));
    if (!d.isValid())
        return QDateTime();
    if (dayOfWeek != NO_NUMBER) {
        if (day == NO_NUMBER) {
            day = 1 + dayOfWeek - QDate(year, month, 1).dayOfWeek();
            if (day <= 0)
                day += 7;
        } else {
            if (QDate(year, month, day).dayOfWeek() != dayOfWeek)
                return QDateTime();
        }
    }
    if (day == NO_NUMBER)
        day = 1;
    dateOnly = (hour == NO_NUMBER && minute == NO_NUMBER && second == NO_NUMBER && millisec == NO_NUMBER);
    if (hour == NO_NUMBER)
        hour = 0;
    if (minute == NO_NUMBER)
        minute = 0;
    if (second == NO_NUMBER)
        second = 0;
    if (millisec == NO_NUMBER)
        millisec = 0;
    if (ampm != NO_NUMBER) {
        if (!hour || hour > 12)
            return QDateTime();
        if (ampm == 1  &&  hour == 12)
            hour = 0;
        else if (ampm == 2  &&  hour < 12)
            hour += 12;
    }

    QDateTime dt(d, QTime(hour, minute, second, millisec), (tzoffset == 0 ? Qt::UTC : Qt::LocalTime));

    utcOffset = (tzoffset == NO_NUMBER) ? 0 : tzoffset * 60;

    return dt;
}

/*
 * Find which day name matches the specified part of a string.
 * 'offset' is incremented by the length of the match.
 * Reply = day number (1 - 7), or <= 0 if no match.
 */
int matchDay(const QString &string, int &offset, bool localised)
{
    int dayOfWeek;
    const QString part = string.mid(offset);
    if (part.isEmpty())
        return -1;
    if (localised) {
        // Check for localised day name first
        const QLocale locale;
        for (dayOfWeek = 1;  dayOfWeek <= 7;  ++dayOfWeek) {
            const QString name = locale.dayName(dayOfWeek, QLocale::LongFormat);
            if (part.startsWith(name, Qt::CaseInsensitive)) {
                offset += name.length();
                return dayOfWeek;
            }
        }
        for (dayOfWeek = 1;  dayOfWeek <= 7;  ++dayOfWeek) {
            const QString name = locale.dayName(dayOfWeek, QLocale::ShortFormat);
            if (part.startsWith(name, Qt::CaseInsensitive)) {
                offset += name.length();
                return dayOfWeek;
            }
        }
    }

    // Check for English day name
    dayOfWeek = findString(part, longDay, 7, offset);
    if (dayOfWeek <= 0)
        dayOfWeek = findString(part, shortDay, 7, offset);
    return dayOfWeek;
}

/*
 * Find which month name matches the specified part of a string.
 * 'offset' is incremented by the length of the match.
 * Reply = month number (1 - 12), or <= 0 if no match.
 */
int matchMonth(const QString &string, int &offset, bool localised)
{
    int month;
    const QString part = string.mid(offset);
    if (part.isEmpty())
        return -1;
    if (localised) {
        // Check for localised month name first
        const QLocale locale;
        for (month = 1;  month <= 12;  ++month) {
            const QString name = locale.monthName(month, QLocale::LongFormat);
            if (part.startsWith(name, Qt::CaseInsensitive)) {
                offset += name.length();
                return month;
            }
        }
        for (month = 1;  month <= 12;  ++month) {
            const QString name = locale.monthName(month, QLocale::ShortFormat);
            if (part.startsWith(name, Qt::CaseInsensitive)) {
                offset += name.length();
                return month;
            }
        }
    }
    // Check for English month name
    month = findString(part, longMonth, 12, offset);
    if (month <= 0)
        month = findString(part, shortMonth, 12, offset);
    return month;
}

/*
 * Read a UTC offset from the input string.
 */
bool getUTCOffset(const QString &string, int &offset, bool colon, int &result)
{
    int sign;
    int len = string.length();
    if (offset >= len)
        return false;
    switch (string[offset++].unicode()) {
        case '+':
            sign = 1;
            break;
        case '-':
            sign = -1;
            break;
        default:
            return false;
    }
    int tzhour = NO_NUMBER;
    int tzmin  = NO_NUMBER;
    if (!getNumber(string, offset, 2, 2, 0, 99, tzhour))
        return false;
    if (colon) {
        if (offset >= len  ||  string[offset++] != QLatin1Char(':'))
            return false;
    }
    if (offset >= len  ||  !string[offset].isDigit()) {
        tzmin = 0;
    } else {
        if (!getNumber(string, offset, 2, 2, 0, 59, tzmin))
            return false;
    }
    tzmin += tzhour * 60;
    if (result != NO_NUMBER  &&  result != tzmin)
        return false;
    result = sign * tzmin;
    return true;
}

/*
 * Read an am/pm indicator from the input string.
 * 'offset' is incremented by the length of the match.
 * Reply = 1 (am), 2 (pm), or 0 if no match.
 */
int getAmPm(const QString &string, int &offset, bool localised)
{
    QString part = string.mid(offset);
    int ap = 0;
    int n = 2;
    if (localised) {
        // Check localised form first
        const QLocale locale;
        QString aps = locale.amText();
        if (part.startsWith(aps, Qt::CaseInsensitive)) {
            ap = 1;
            n = aps.length();
        } else {
            aps = locale.pmText();
            if (part.startsWith(aps, Qt::CaseInsensitive)) {
                ap = 2;
                n = aps.length();
            }
        }
    }
    if (!ap) {
        if (part.startsWith(QLatin1String("am"), Qt::CaseInsensitive))
            ap = 1;
        else if (part.startsWith(QLatin1String("pm"), Qt::CaseInsensitive))
            ap = 2;
    }
    if (ap)
        offset += n;
    return ap;
}

/* Convert part of 'string' to a number.
 * If converted number differs from any current value in 'result', the function fails.
 * Reply = true if successful.
 */
bool getNumber(const QString &string, int &offset, int mindigits, int maxdigits, int minval, int maxval, int &result)
{
    int end = string.size();
    bool neg = false;
    if (minval == NO_NUMBER  &&  offset < end  &&  string[offset] == QLatin1Char('-')) {
        neg = true;
        ++offset;
    }
    if (offset + maxdigits > end)
        maxdigits = end - offset;
    int ndigits;
    for (ndigits = 0;  ndigits < maxdigits && string[offset + ndigits].isDigit();  ++ndigits);
    if (ndigits < mindigits)
        return false;
    bool ok;
    int n = string.midRef(offset, ndigits).toInt(&ok);
    if (neg)
        n = -n;
    if (!ok  || (result != NO_NUMBER && n != result)  || (minval != NO_NUMBER && n < minval)  || (n > maxval && maxval >= 0))
        return false;
    result = n;
    offset += ndigits;
    return true;
}

int findString(const QString &string, DayMonthName func, int count, int &offset)
{
    for (int i = 1;  i <= count;  ++i)
        if (string.startsWith(func(i), Qt::CaseInsensitive)) {
            offset += func(i).size();
            return i;
        }
    return -1;
}

QString numString(int n, int width)
{
  return QStringLiteral("%1").arg(n, width, 10, QLatin1Char('0'));
}

// Return the UTC offset in a given time zone, for a specified date/time.
int offsetAtZoneTime(const QTimeZone &tz, const QDateTime &zoneDateTime, int *secondOffset)
{
    if (!zoneDateTime.isValid()  // check for invalid time
    ||  (zoneDateTime.timeSpec() != Qt::LocalTime && zoneDateTime.timeSpec() != Qt::TimeZone)) {
        if (secondOffset)
            *secondOffset = InvalidOffset;
        return InvalidOffset;
    }
    int offset = tz.offsetFromUtc(zoneDateTime);
    if (secondOffset) {
        // Check if there is a daylight savings shift around zoneDateTime.
        QDateTime utc1(zoneDateTime.date(), zoneDateTime.time(), Qt::UTC);
        QDateTime utc = utc1.addSecs(-offset);
        const QVector<QTimeZone::OffsetData> transitions = tz.transitions(utc.addSecs(-7200), utc.addSecs(7200));
        if (!transitions.isEmpty()) {
            // Assume that there will only be one transition in a 4 hour period.
            const QTimeZone::OffsetData before = tz.previousTransition(transitions[0].atUtc);
            if (before.atUtc.isValid()  &&  transitions[0].atUtc.isValid()) {
                int step = before.offsetFromUtc - transitions[0].offsetFromUtc;
                if (step > 0) {
                    // The transition steps the local time backwards, so check for
                    // a second occurrence of the local time.
                    // Find the local time when the transition occurs.
                    const QDateTime changeStart = transitions[0].atUtc.addSecs(transitions[0].offsetFromUtc);
                    const QDateTime changeEnd   = transitions[0].atUtc.addSecs(before.offsetFromUtc);
                    if (utc1 >= changeStart  &&  utc1 < changeEnd) {
                        // The local time occurs twice.
                        *secondOffset = transitions[0].offsetFromUtc;
                        return before.offsetFromUtc;
                    }
                }
            }
        }
        *secondOffset = offset;
    }
    return offset;
}

// Convert a UTC date/time to a time zone date/time.
QDateTime toZoneTime(const QTimeZone &tz, const QDateTime &utcDateTime, bool *secondOccurrence)
{
    if (secondOccurrence)
        *secondOccurrence = false;
    if (!utcDateTime.isValid()  ||  utcDateTime.timeSpec() != Qt::UTC)
        return QDateTime();
    const QDateTime dt = utcDateTime.toTimeZone(tz);
    if (secondOccurrence) {
        // Check if there is a daylight savings shift around zoneDateTime.
        const QVector<QTimeZone::OffsetData> transitions = tz.transitions(utcDateTime.addSecs(-7200), utcDateTime.addSecs(7200));
        if (!transitions.isEmpty()) {
            // Assume that there will only be one transition in a 4 hour period.
            const QTimeZone::OffsetData before = tz.previousTransition(transitions[0].atUtc);
            if (before.atUtc.isValid()  &&  transitions[0].atUtc.isValid()) {
                int step = before.offsetFromUtc - transitions[0].offsetFromUtc;
                if (step > 0) {
                    // The transition steps the local time backwards, so check for
                    // a second occurrence of the local time.
                    // Find the local time when the transition occurs.
                    const QDateTime changeStart = transitions[0].atUtc.addSecs(transitions[0].offsetFromUtc);
                    const QDateTime changeEnd   = transitions[0].atUtc.addSecs(before.offsetFromUtc);
                    QDateTime dtTz = dt;
                    dtTz.setTimeSpec(Qt::UTC);
                    if (dtTz >= changeStart  &&  dtTz < changeEnd) {
                        // The local time occurs twice.
                        *secondOccurrence = (utcDateTime >= transitions[0].atUtc);
                        return dt;
                    }
                }
            }
        }
        *secondOccurrence = false;
    }
    return dt;
}

void initDayMonthNames()
{
    if (shortDayNames.isEmpty()) {
        QLocale locale(QStringLiteral("C"));   // US English locale

        for (int i = 1;  i <= 7;  ++i)
            shortDayNames.push_back(locale.dayName(i, QLocale::ShortFormat));
        for (int i = 1;  i <= 7;  ++i)
            longDayNames.push_back(locale.dayName(i, QLocale::LongFormat));
        for (int i = 1;  i <= 12;  ++i)
            shortMonthNames.push_back(locale.monthName(i, QLocale::ShortFormat));
        for (int i = 1;  i <= 12;  ++i)
            longMonthNames.push_back(locale.monthName(i, QLocale::LongFormat));
    }
}

// Short day name, in English
const QString &shortDay(int day)   // Mon = 1, ...
{
    static QString error;
    initDayMonthNames();
    return (day >= 1 && day <= 7) ? shortDayNames.at(day - 1) : error;
}

// Long day name, in English
const QString &longDay(int day)   // Mon = 1, ...
{
    static QString error;
    initDayMonthNames();
    return (day >= 1 && day <= 7) ? longDayNames.at(day - 1) : error;
}

// Short month name, in English
const QString &shortMonth(int month)   // Jan = 1, ...
{
    static QString error;
    initDayMonthNames();
    return (month >= 1 && month <= 12) ? shortMonthNames.at(month - 1) : error;
}

// Long month name, in English
const QString &longMonth(int month)   // Jan = 1, ...
{
    static QString error;
    initDayMonthNames();
    return (month >= 1 && month <= 12) ? longMonthNames.at(month - 1) : error;
}

} // namespace

// vim: et sw=4:
