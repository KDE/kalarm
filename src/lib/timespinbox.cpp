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
    setReverseWithLayout(false);   // keep buttons the same way round even if right-to-left language
    setShiftSteps(5, 360);    // shift-left button increments 5 min / 6 hours
    setSelectOnStep(false);
    setAlignment(Qt::AlignHCenter);
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
    setReverseWithLayout(false);   // keep buttons the same way round even if right-to-left language
    setShiftSteps(5, 300);    // shift-left button increments 5 min / 5 hours
    setSelectOnStep(false);
    setAlignment(Qt::AlignRight);
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
    const QString s = QString::asprintf((wrapping() ? "%02d:%02d" : "%d:%02d"), v/60, v%60);
    return s;
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
    const QString text = cleanText();
    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon >= 0)
    {
        // [h]:m format for any time value
        const QString hour   = text.left(colon).trimmed();
        const QString minute = text.mid(colon + 1).trimmed();
        if (!minute.isEmpty())
        {
            bool okmin;
            bool okhour = true;
            const int m = minute.toUInt(&okmin);
            int h = 0;
            if (!hour.isEmpty())
                h = hour.toUInt(&okhour);
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
    else if (text.length() == 4)
    {
        // hhmm format for time of day
        bool okn;
        const int mins = text.toUInt(&okn);
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
        setSpecialValueText(QStringLiteral("**:**"));
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
    return {sz.width() + fm.horizontalAdvance(QLatin1Char(':')), sz.height()};
}

QSize TimeSpinBox::minimumSizeHint() const
{
    const QSize sz = SpinBox2::minimumSizeHint();
    const QFontMetrics fm(font());
    return {sz.width() + fm.horizontalAdvance(QLatin1Char(':')), sz.height()};
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
    QString hour;
    bool ok;
    int hr = 0;
    int mn = 0;
    const int colon = cleanText.indexOf(QLatin1Char(':'));
    if (colon >= 0)
    {
        const QString minute = cleanText.mid(colon + 1);
        if (minute.isEmpty())
            state = QValidator::Intermediate;
        else if ((mn = minute.toUInt(&ok)) >= 60  ||  !ok)
            return QValidator::Invalid;

        hour = cleanText.left(colon);
    }
    else if (maxMinute >= 1440)
    {
        // The hhmm form of entry is only allowed for time-of-day, i.e. <= 2359
        hour = cleanText;
        state = QValidator::Intermediate;
    }
    else
    {
        if (cleanText.length() > 4)
            return QValidator::Invalid;
        if (cleanText.length() < 4)
            state = QValidator::Intermediate;
        hour = cleanText.left(2);
        const QString minute = cleanText.mid(2);
        if (!minute.isEmpty()
        &&  ((mn = minute.toUInt(&ok)) >= 60  ||  !ok))
            return QValidator::Invalid;
    }

    if (!hour.isEmpty())
    {
        hr = hour.toUInt(&ok);
        if (ok  &&  m12Hour)
        {
            if (hr == 0)
                return QValidator::Intermediate;
            if (hr == 0  ||  hr > 12)
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

// vim: et sw=4:
