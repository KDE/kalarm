/*
 *  timeperiod.cpp  -  time period data entry widget
 *  Program:  kalarm
 *  (C) 2003, 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef TIMEPERIOD_H
#define TIMEPERIOD_H

#include <qhbox.h>
#include <qstring.h>

class QWidgetStack;
class ComboBox;
class SpinBox;
class TimeSpinBox;


/**
 *  The TimePeriod class provides a widget for entering a time period as a number of
 *  weeks, days, or hours and minutes.
 *
 *  It displays a combo box to select the time units (weeks, days or hours and minutes)
 *  alongside a spin box to enter the number of units. The type of spin box displayed
 *  alters according to the units selection: day and week values are entered in a normal
 *  spin box, while hours and minutes are entered in a time spin box (with two pairs of
 *  spin buttons, one for hours and one for minutes).
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short Time period entry widget.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class TimePeriod : public QHBox
{
		Q_OBJECT
	public:
		/** Units for the time period.
		 *  @li HOURS_MINUTES - the time period is entered as an hours/minutes value.
		 *  @li DAYS - the time period is entered as a number of days.
		 *  @li WEEKS - the time period is entered as a number of weeks.
		 */
		enum Units { HOURS_MINUTES, DAYS, WEEKS };

		/** Constructor.
		 *  @param allowHourMinute Set false to prevent hours/minutes from being allowed
		 *         as units; only days and weeks can ever be used, regardless of other
		 *         method calls. Set true to allow hours/minutes, days or weeks as units.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		TimePeriod(bool allowHourMinute, QWidget* parent, const char* name = 0);
		/** Returns true if the widget is read only. */
		bool          isReadOnly() const             { return mReadOnly; }
		/** Sets whether the widget is read-only for the user. If read-only,
		 *  the time period cannot be edited and the units combo box is inactive.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void          setReadOnly(bool readOnly);
		/** Gets the entered time period expressed in minutes. */
		int           minutes() const;
		/** Initialises the time period value.
		 *  @param minutes The value of the time period to set, expressed as a number of minutes.
		 *  @param dateOnly True to restrict the units available in the combo box to days or weeks.
		 *  @param defaultUnits The units to display initially in the combo box.
		 */
		void          setMinutes(int minutes, bool dateOnly, Units defaultUnits);
		/** Enables or disables hours/minutes units in the combo box. To disable hours/minutes,
		 *  set @p dateOnly true; to enable hours/minutes, set @p dateOnly false. But note that
		 *  hours/minutes cannot be enabled if it was disallowed in the constructor.
		 */
		void          setDateOnly(bool dateOnly)     { setDateOnly(minutes(), dateOnly, true); }
		/** Sets the maximum values for the hours/minutes and days/weeks spin boxes.
		 *  Set @p hourmin = 0 to leave the hours/minutes maximum unchanged.
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
		void          setWhatsThis(const QString& units, const QString& dayWeek, const QString& hourMin = QString::null);

		static QString i18n_hours_mins();  // text of 'hours/minutes' units, lower case
		static QString i18n_Hours_Mins();  // text of 'Hours/Minutes' units, initial capitals
		static QString i18n_days();        // text of 'days' units, lower case
		static QString i18n_Days();        // text of 'Days' units, initial capital
		static QString i18n_weeks();       // text of 'weeks' units, lower case
		static QString i18n_Weeks();       // text of 'Weeks' units, initial capital

	signals:
		/** This signal is emitted whenever the value held in the widget changes.
		 *  @param minutes The current value of the time period, expressed in minutes.
		 */
		void          valueChanged(int minutes);   // value has changed

	private slots:
		void          slotUnitsSelected(int index);
		void          slotDaysChanged(int);
		void          slotTimeChanged(int minutes);

	private:
		Units         setDateOnly(int minutes, bool dateOnly, bool signal);
		void          setUnitRange();
		void          showHourMin(bool hourMin);
		void          adjustDayWeekShown();

		QWidgetStack* mSpinStack;          // displays either the days/weeks or hours:minutes spinbox
		SpinBox*      mSpinBox;            // the days/weeks value spinbox
		TimeSpinBox*  mTimeSpinBox;        // the hours:minutes value spinbox
		ComboBox*     mUnitsCombo;
		int           mMaxDays;            // maximum day count
		int           mDateOnlyOffset;     // for mUnitsCombo: 1 if hours/minutes is disabled, else 0
		Units         mMaxUnitShown;       // for mUnitsCombo: maximum units shown
		bool          mNoHourMinute;       // hours/minutes cannot be displayed, ever
		bool          mReadOnly;           // the widget is read only
		bool          mHourMinuteRaised;   // hours:minutes spinbox is currently displayed
};

#endif // TIMEPERIOD_H
