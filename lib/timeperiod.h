/*
 *  timeperiod.cpp  -  time period data entry widget
 *  Program:  kalarm
 *  Copyright © 2003-2005,2007,2008,2010 by David Jarvie <djarvie@kde.org>
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

#ifndef TIMEPERIOD_H
#define TIMEPERIOD_H

#ifdef USE_AKONADI
#include <KCalCore/duration.h>
#else
#include <kcal/duration.h>
#endif
#include <khbox.h>
#include <QString>

class QStackedWidget;
class ComboBox;
class SpinBox;
class TimeSpinBox;


/**
 *  @short Time period entry widget.
 *
 *  The TimePeriod class provides a widget for entering a time period as a number of
 *  weeks, days, hours and minutes, or minutes.
 *
 *  It displays a combo box to select the time units (weeks, days, hours and minutes, or
 *  minutes) alongside a spin box to enter the number of units. The type of spin box
 *  displayed alters according to the units selection: day, week and minute values are
 *  entered in a normal spin box, while hours and minutes are entered in a time spin box
 *  (with two pairs of spin buttons, one for hours and one for minutes).
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class TimePeriod : public KHBox
{
        Q_OBJECT
    public:
        /** Units for the time period.
         *  @li Minutes - the time period is entered as a number of minutes.
         *  @li HoursMinutes - the time period is entered as an hours/minutes value.
         *  @li Days - the time period is entered as a number of days.
         *  @li Weeks - the time period is entered as a number of weeks.
         */
        enum Units { Minutes, HoursMinutes, Days, Weeks };

        /** Constructor.
         *  @param allowMinute Set false to prevent hours/minutes or minutes from
         *         being allowed as units; only days and weeks can ever be used,
         *         regardless of other method calls. Set true to allow minutes,
         *         hours/minutes, days or weeks as units.
         *  @param parent The parent object of this widget.
         */
        TimePeriod(bool allowMinute, QWidget* parent);
        /** Returns true if the widget is read only. */
        bool          isReadOnly() const             { return mReadOnly; }
        /** Sets whether the widget is read-only for the user. If read-only,
         *  the time period cannot be edited and the units combo box is inactive.
         *  @param readOnly True to set the widget read-only, false to set it read-write.
         */
        virtual void  setReadOnly(bool readOnly);
        /** Gets the currently selected time units. */
        Units         units() const;
        /** Sets the time units.
         *  Note that this changes the value.
         */
        void          setUnits(Units units);
        /** Gets the entered time period. */
#ifdef USE_AKONADI
        KCalCore::Duration period() const;
#else
        KCal::Duration period() const;
#endif
        /** Initialises the time period value.
         *  @param period The value of the time period to set. If zero, the time period
         *                is left unchanged.
         *  @param dateOnly True to restrict the units available in the combo box to days or weeks.
         *  @param defaultUnits The units to display initially in the combo box.
         */
#ifdef USE_AKONADI
        void          setPeriod(const KCalCore::Duration& period, bool dateOnly, Units defaultUnits);
#else
        void          setPeriod(const KCal::Duration& period, bool dateOnly, Units defaultUnits);
#endif
        /** Returns true if minutes and hours/minutes units are disabled. */
        bool          isDateOnly() const             { return mDateOnlyOffset; }
        /** Enables or disables minutes and hours/minutes units in the combo box. To
         *  disable minutes and hours/minutes, set @p dateOnly true; to enable minutes
         *  and hours/minutes, set @p dateOnly false. But note that minutes and
         *  hours/minutes cannot be enabled if it was disallowed in the constructor.
         */
        void          setDateOnly(bool dateOnly)     { setDateOnly(period(), dateOnly, true); }
        /** Sets the maximum values for the minutes and hours/minutes, and days/weeks
         *  spin boxes.
         *  Set @p hourmin = 0 to leave the minutes and hours/minutes maximum unchanged.
         */
        void          setMaximum(int hourmin, int days);
        /** Sets whether the editor text is to be selected whenever spin buttons are
         *  clicked. The default is to select it.
         */
        void          setSelectOnStep(bool select);
        /** Sets the input focus to the count field. */
        void          setFocusOnCount();
        /** Sets separate WhatsThis texts for the count spin boxes and the units combo box.
         *  If @p hourMin is omitted, both spin boxes are set to the same WhatsThis text.
         */
        void          setWhatsThises(const QString& units, const QString& dayWeek, const QString& hourMin = QString());

    signals:
        /** This signal is emitted whenever the value held in the widget changes.
         *  @param period The current value of the time period.
         */
#ifdef USE_AKONADI
        void            valueChanged(const KCalCore::Duration& period);
#else
        void            valueChanged(const KCal::Duration& period);
#endif

    private slots:
        void            slotUnitsSelected(int index);
        void            slotDaysChanged(int);
        void            slotTimeChanged(int minutes);

    private:
#ifdef USE_AKONADI
        Units           setDateOnly(const KCalCore::Duration&, bool dateOnly, bool signal);
#else
        Units           setDateOnly(const KCal::Duration&, bool dateOnly, bool signal);
#endif
        void            setUnitRange();
        void            showHourMin(bool hourMin);
        void            adjustDayWeekShown();

        static QString i18n_minutes();       // text of 'minutes' units, lower case
        static QString i18n_hours_mins();    // text of 'hours/minutes' units
        static QString i18n_days();          // text of 'days' units
        static QString i18n_weeks();         // text of 'weeks' units

        QStackedWidget* mSpinStack;          // displays either the days/weeks or hours:minutes spinbox
        SpinBox*        mSpinBox;            // the minutes/days/weeks value spinbox
        TimeSpinBox*    mTimeSpinBox;        // the hours:minutes value spinbox
        ComboBox*       mUnitsCombo;
        int             mMaxDays;            // maximum day count
        int             mDateOnlyOffset;     // for mUnitsCombo: 2 if minutes & hours/minutes is disabled, else 0
        Units           mMaxUnitShown;       // for mUnitsCombo: maximum units shown
        bool            mNoHourMinute;       // hours/minutes cannot be displayed, ever
        bool            mReadOnly;           // the widget is read only
        bool            mHourMinuteRaised;   // hours:minutes spinbox is currently displayed
};

#endif // TIMEPERIOD_H

// vim: et sw=4:
