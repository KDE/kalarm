/*
 *  timespinbox.cpp  -  time spinbox widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "timespinbox.h"

#include <KLocalizedString>

/*=============================================================================
= Class TimeSpinBox
= This is a spin box displaying a time in the format hh:mm, with a pair of
= spin buttons for each of the hour and minute values.
= It can operate in 3 modes:
=  1) a time of day using the 24-hour clock.
=  2) a time of day using the 12-hour clock. The value is held as 0:00 - 23:59,
=     but is displayed as 12:00 - 11:59. This is for use in a TimeEdit widget.
=  3) a length of time, not restricted to the length of a day.
=============================================================================*/

/******************************************************************************
 * Construct a wrapping 00:00 - 23:59, or 12:00 - 11:59 time spin box.
 */
TimeSpinBox::TimeSpinBox(bool use24hour, QWidget* parent)
    : SpinBox2(0, 1439, 60, parent)
    , mMinimumValue(0)
    , m12Hour(!use24hour)
    , mPm(false)
{
    setWrapping(true);
    setShiftSteps(5, 360, 60, false);    // shift-left button increments 5 min / 6 hours
    setAlignment(Qt::AlignHCenter);
    init();
    connect(this, &TimeSpinBox::valueChanged, this, &TimeSpinBox::slotValueChanged);
}

/******************************************************************************
 * Construct a non-wrapping time spin box.
 */
TimeSpinBox::TimeSpinBox(int minMinute, int maxMinute, QWidget* parent)
    : SpinBox2(minMinute, maxMinute, 60, parent)
    , mMinimumValue(minMinute)
    , m12Hour(false)
{
    setShiftSteps(5, 300, 60, false);    // shift-left button increments 5 min / 5 hours
    setAlignment(Qt::AlignRight);
    init();
}

void TimeSpinBox::init()
{
    setReverseWithLayout(false);   // keep buttons the same way round even if right-to-left language
    setSelectOnStep(false);

    // Determine the time format, including only hours and minutes.
    // Find the separator between hours and minutes, for the current locale.
    const QString timeFormat = QLocale().timeFormat(QLocale::ShortFormat);
    bool done = false;
    bool quote = false;
    int searching = 0;   // -1 for hours, 1 for minutes
    for (int i = 0;  i < timeFormat.size() && !done;  ++i)
    {
        const QChar qch = timeFormat.at(i);
        const char ch = qch.toLatin1();
        if (quote  &&  ch != '\'')
        {
            if (searching)
                mSeparator += qch;
            continue;
        }
        switch (ch)
        {
            case 'h':
            case 'H':
                if (searching == 0)
                    searching = 1;
                else if (searching == 1)   // searching for minutes
                    mSeparator.clear();
                else if (searching == -1)   // searching for hours
                {
                    mReversed = true;   // minutes are before hours
                    done = true;
                }
                if (i < timeFormat.size() - 1  &&  timeFormat.at(i + 1) == qch)
                    ++i;
                break;

            case 'm':
                if (searching == 0)
                    searching = -1;
                else if (searching == -1)   // searching for minutes
                    mSeparator.clear();
                else if (searching == 1)    // searching for hours
                    done = true;
                if (i < timeFormat.size() - 1  &&  timeFormat.at(i + 1) == qch)
                    ++i;
                break;

            case '\'':
                if (!quote  &&  searching
                &&  i < timeFormat.size() - 1  &&  timeFormat.at(i + 1) == qch)
                {
                    mSeparator += qch;    // two consecutive single quotes means a literal quote
                    ++i;
                }
                else
                    quote = !quote;
                break;

            default:
                if (searching)
                    mSeparator += qch;
                break;
        }
    }
}

QString TimeSpinBox::shiftWhatsThis()
{
    return i18nc("@info:whatsthis", "Press the Shift key while clicking the spin buttons to adjust the time by a larger step (6 hours / 5 minutes).");
}

QTime TimeSpinBox::time() const
{
    return {value() / 60, value() % 60};
}

QString TimeSpinBox::textFromValue(int v) const
{
    if (m12Hour)
    {
        if (v < 60)
            v += 720;      // convert 0:nn to 12:nn
        else if (v >= 780)
            v -= 720;      // convert 13 - 23 hours to 1 - 11
    }
    QLocale locale;
    QString hours = locale.toString(v / 60);
    if (wrapping()  &&  hours.size() == 1)
        hours.prepend(locale.zeroDigit());
    QString mins = locale.toString(v % 60);
    if (mins.size() == 1)
        mins.prepend(locale.zeroDigit());
    return mReversed ? mins + mSeparator + hours : hours + mSeparator + mins;
}

/******************************************************************************
 * Convert the user-entered text to a value in minutes.
 * The allowed formats are:
 *    [hour]:[minute], where minute must be non-blank, or
 *    hhmm, 4 digits, where hour < 24.
 * Reply = 0 if error.
 */
int TimeSpinBox::valueFromText(const QString&) const
{
    QLocale locale;
    const QString text = cleanText();
    const int colon = text.indexOf(mSeparator);
    if (colon >= 0)
    {
        // [h]:m format for any time value
        const QString first  = text.left(colon).trimmed();
        const QString second = text.mid(colon + mSeparator.size()).trimmed();
        const QString hour   = mReversed ? second : first;
        const QString minute = mReversed ? first : second;
        if (!minute.isEmpty())
        {
            bool okmin;
            bool okhour = true;
            const int m = locale.toUInt(minute, &okmin);
            int h = 0;
            if (!hour.isEmpty())
                h = locale.toUInt(hour, &okhour);
            if (okhour  &&  okmin  &&  m < 60)
            {
                if (m12Hour)
                {
                    if (h == 0  ||  h > 12)
                        h = 100;     // error
                    else if (h == 12)
                        h = 0;       // convert 12:nn to 0:nn
                    if (mPm)
                        h += 12;     // convert to PM
                }
                const int t = h * 60 + m;
                if (t >= mMinimumValue  &&  t <= maximum())
                    return t;
            }
        }
    }
    else if (text.length() == 4  &&  !mReversed)
    {
        // hhmm format for time of day
        bool okn;
        const int mins = locale.toUInt(text, &okn);
        if (okn)
        {
            const int m = mins % 100;
            int h = mins / 100;
            if (m12Hour)
            {
                if (h == 0  ||  h > 12)
                    h = 100;    // error
                else if (h == 12)
                    h = 0;      // convert 12:nn to 0:nn
                if (mPm)
                    h += 12;    // convert to PM
            }
            const int t = h * 60 + m;
            if (h < 24  &&  m < 60  &&  t >= mMinimumValue  &&  t <= maximum())
                return t;
        }

    }
    return 0;
}

/******************************************************************************
 * Set the spin box as valid or invalid.
 * If newly invalid, the value is displayed as asterisks.
 * If newly valid, the value is set to the minimum value.
 */
void TimeSpinBox::setValid(bool valid)
{
    if (valid  &&  mInvalid)
    {
        mInvalid = false;
        if (value() < mMinimumValue)
            SpinBox2::setValue(mMinimumValue);
        setSpecialValueText(QString());
        SpinBox2::setMinimum(mMinimumValue);
    }
    else if (!valid  &&  !mInvalid)
    {
        mInvalid = true;
        SpinBox2::setMinimum(mMinimumValue - 1);
        setSpecialValueText(QStringLiteral("**%1**").arg(mSeparator));
        SpinBox2::setValue(mMinimumValue - 1);
    }
}

/******************************************************************************
* Set the spin box's minimum value.
*/
void TimeSpinBox::setMinimum(int minutes)
{
    mMinimumValue = minutes;
    SpinBox2::setMinimum(mMinimumValue - (mInvalid ? 1 : 0));
}

/******************************************************************************
 * Set the spin box's value.
 */
void TimeSpinBox::setValue(int minutes)
{
    if (!mEnteredSetValue)
    {
        mEnteredSetValue = true;
        mPm = (minutes >= 720);
        if (minutes > maximum())
            setValid(false);
        else
        {
            if (mInvalid)
            {
                mInvalid = false;
                setSpecialValueText(QString());
                SpinBox2::setMinimum(mMinimumValue);
            }
            SpinBox2::setValue(minutes);
            mEnteredSetValue = false;
        }
    }
}

/******************************************************************************
 * Step the spin box value.
 * If it was invalid, set it valid and set the value to the minimum.
 */
void TimeSpinBox::stepBy(int increment)
{
    if (mInvalid)
        setValid(true);
    else
        SpinBox2::stepBy(increment);
}

bool TimeSpinBox::isValid() const
{
    return value() >= mMinimumValue;
}

void TimeSpinBox::slotValueChanged(int value)
{
    mPm = (value >= 720);
}

QSize TimeSpinBox::sizeHint() const
{
    const QSize sz = SpinBox2::sizeHint();
    const QFontMetrics fm(font());
    return {sz.width() + fm.horizontalAdvance(mSeparator), sz.height()};
}

QSize TimeSpinBox::minimumSizeHint() const
{
    const QSize sz = SpinBox2::minimumSizeHint();
    const QFontMetrics fm(font());
    return {sz.width() + fm.horizontalAdvance(mSeparator), sz.height()};
}

/******************************************************************************
 * Validate the time spin box input.
 * The entered time must either be 4 digits, or it must contain a colon, but
 * hours may be blank.
 */
QValidator::State TimeSpinBox::validate(QString& text, int&) const
{
    const QString cleanText = text.trimmed();
    if (cleanText.isEmpty())
        return QValidator::Intermediate;
    QValidator::State state = QValidator::Acceptable;
    const int maxMinute = maximum();
    QLocale locale;
    QString hour;
    bool ok;
    int hr = 0;
    int mn = 0;
    const int colon = cleanText.indexOf(mSeparator);
    if (colon >= 0)
    {
        const QString first  = cleanText.left(colon);
        const QString second = cleanText.mid(colon + mSeparator.size());

        const QString minute = mReversed ? first : second;
        if (minute.isEmpty())
            state = QValidator::Intermediate;
        else if ((mn = locale.toUInt(minute, &ok)) >= 60  ||  !ok)
            return QValidator::Invalid;

        hour = mReversed ? second : first;
    }
    else if (!wrapping())
    {
        // It's a time duration, so the hhmm form of entry is not allowed.
        hour = cleanText;
        state = QValidator::Intermediate;
    }
    else if (!mReversed)
    {
        // It's a time of day, where the hhmm form of entry is allowed as long
        // as the order is hours followed by minutes.
        if (cleanText.length() > 4)
            return QValidator::Invalid;
        if (cleanText.length() < 4)
            state = QValidator::Intermediate;
        hour = cleanText.left(2);
        const QString minute = cleanText.mid(2);
        if (!minute.isEmpty()
        &&  ((mn = locale.toUInt(minute, &ok)) >= 60  ||  !ok))
            return QValidator::Invalid;
    }

    if (!hour.isEmpty())
    {
        hr = locale.toUInt(hour, &ok);
        if (ok  &&  m12Hour)
        {
            if (hr == 0)
                return QValidator::Intermediate;
            if (hr > 12)
                hr = 100;    // error;
            else if (hr == 12)
                hr = 0;      // convert 12:nn to 0:nn
            if (mPm)
                hr += 12;    // convert to PM
        }
        if (!ok  ||  hr > maxMinute/60)
            return QValidator::Invalid;
    }
    else if (m12Hour)
        return QValidator::Intermediate;

    if (state == QValidator::Acceptable)
    {
        const int t = hr * 60 + mn;
        if (t < minimum()  ||  t > maxMinute)
            return QValidator::Invalid;
    }
    return state;
}

#include "moc_timespinbox.cpp"

// vim: et sw=4:
