/*
 *  timespinbox.h  -  time spinbox widget
 *  Program:  kalarm
 *  Copyright © 2001-2019 David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef TIMESPINBOX_H
#define TIMESPINBOX_H

#include "spinbox2.h"
#include <QTime>

/**
 *  @short Hours/minutes time entry widget.
 *
 *  The TimeSpinBox class provides a widget to enter a time consisting of an hours/minutes
 *  value. It can hold a time in any of 3 modes: a time of day using the 24-hour clock; a
 *  time of day using the 12-hour clock; or a length of time not restricted to 24 hours.
 *
 *  Derived from SpinBox2, it displays a spin box with two pairs of spin buttons, one
 *  for hours and one for minutes. It provides accelerated stepping using the spin
 *  buttons, when the shift key is held down (inherited from SpinBox2). The default
 *  shift steps are 5 minutes and 6 hours.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class TimeSpinBox : public SpinBox2
{
        Q_OBJECT
    public:
        /** Constructor for a wrapping time spin box which can be used to enter a time of day.
         *  @param use24hour True for entry of 24-hour clock times (range 00:00 to 23:59).
         *                   False for entry of 12-hour clock times (range 12:00 to 11:59).
         *  @param parent The parent object of this widget.
         */
        explicit TimeSpinBox(bool use24hour, QWidget* parent = nullptr);
        /** Constructor for a non-wrapping time spin box which can be used to enter a length of time.
         *  @param minMinute The minimum value which the spin box can hold, in minutes.
         *  @param maxMinute  The maximum value which the spin box can hold, in minutes.
         *  @param parent The parent object of this widget.
         */
        TimeSpinBox(int minMinute, int maxMinute, QWidget* parent = nullptr);
        /** Returns true if the spin box holds a valid value.
         *  An invalid value is displayed as asterisks.
         */
        bool            isValid() const;
        /** Sets the spin box as holding a valid or invalid value.
         *  If newly invalid, the value is displayed as asterisks.
         *  If newly valid, the value is set to the minimum value.
         */
        void            setValid(bool);
        /** Determine whether the current input is valid. */
        QValidator::State validate(QString&, int& pos) const override;
        /** Returns the current value held in the spin box.
         *  If an invalid value is displayed, returns a value lower than the minimum value.
         */
        QTime           time() const;
        /** Sets the maximum value which can be held in the spin box.
         *  @param minutes The maximum value expressed in minutes.
         */
        void            setMinimum(int minutes) override;
        /** Sets the maximum value which can be held in the spin box.
         *  @param minutes The maximum value expressed in minutes.
         */
        void            setMaximum(int minutes) override      { SpinBox2::setMaximum(minutes); }
        /** Sets the maximum value which can be held in the spin box. */
        void            setMaximum(const QTime& t)   { SpinBox2::setMaximum(t.hour()*60 + t.minute()); }
        /** Returns the maximum value which can be held in the spin box. */
        QTime           maxTime() const              { int mv = maximum();  return QTime(mv/60, mv%60); }
        /** Called whenever the user triggers a step, to adjust the value of the spin box.
         *  If the value was previously invalid, the spin box is set to the minimum value.
         */
        void            stepBy(int increment) override;
        /** Returns a text describing use of the shift key as an accelerator for
         *  the spin buttons, designed for incorporation into WhatsThis texts.
         */
        static QString  shiftWhatsThis();

        QSize           sizeHint() const override;
        QSize           minimumSizeHint() const override;

    public Q_SLOTS:
        /** Sets the value of the spin box.
         *  @param minutes The new value of the spin box, expressed in minutes.
         */
        virtual void    setValue(int minutes);
        /** Sets the value of the spin box. */
        void            setValue(const QTime& t)     { setValue(t.hour()*60 + t.minute()); }

    protected:
        QString         textFromValue(int v) const override;
        int             valueFromText(const QString&) const override;

    private Q_SLOTS:
        void            slotValueChanged(int value);

    private:
        int             mMinimumValue;       // real minimum value, excluding special value for "**:**"
        bool            m12Hour;             // use 12-hour clock
        bool            mPm;                 // use PM for manually entered values (with 12-hour clock)
        bool            mInvalid {false};          // value is currently invalid (asterisks)
        bool            mEnteredSetValue {false};  // to prevent infinite recursion in setValue()
};

#endif // TIMESPINBOX_H

// vim: et sw=4:
