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


class TimePeriod : public QHBox
{
		Q_OBJECT
	public:
		enum Units { HOURS_MINUTES, DAYS, WEEKS };

		TimePeriod(bool allowHourMinute, QWidget* parent, const char* name = 0);
		bool          isReadOnly() const             { return mReadOnly; }
		void          setReadOnly(bool);
		int           minutes() const;
		void          setMinutes(int minutes, bool dateOnly, Units defaultUnits);
		void          setDateOnly(bool dateOnly)     { setDateOnly(minutes(), dateOnly, true); }
		void          setMaximum(int hourmin, int days);
		void          setSelectOnStep(bool);
		void          setFocusOnCount();
		void          setWhatsThis(const QString& units, const QString& dayWeek, const QString& hourMin = QString::null);

		static QString i18n_hours_mins();  // text of 'hours/minutes' units, lower case
		static QString i18n_Hours_Mins();  // text of 'Hours/Minutes' units, initial capitals
		static QString i18n_days();        // text of 'days' units, lower case
		static QString i18n_Days();        // text of 'Days' units, initial capital
		static QString i18n_weeks();       // text of 'weeks' units, lower case
		static QString i18n_Weeks();       // text of 'Weeks' units, initial capital

	signals:
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
