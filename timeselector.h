/*
 *  timeselector.h  -  widget to optionally set a time period
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef TIMESELECTOR_H
#define TIMESELECTOR_H

#include <qframe.h>

class QLabel;
class CheckBox;
class ComboBox;
class TimePeriod;


class TimeSelector : public QFrame
{
		Q_OBJECT
	public:
		enum Units { HOURS_MINUTES, DAYS, WEEKS };

		TimeSelector(const QString& selectText, const QString& postfix, const QString& selectWhatsThis,
		             const QString& valueWhatsThis, bool allowHourMinute, QWidget* parent, const char* name = 0);
		bool         isChecked() const;
		void         setChecked(bool on);
		int          minutes() const;
		void         setMinutes(int minutes, bool dateOnly, Units defaultUnits);
		void         setReadOnly(bool);
		void         setDateOnly(bool dateOnly)   { setDateOnly(minutes(), dateOnly); }
		void         setMaximum(int hourmin, int days);
		void         setFocusOnCount();

		static QString i18n_hours_mins();  // text of 'hours/minutes' units, lower case
		static QString i18n_Hours_Mins();  // text of 'Hours/Minutes' units, initial capitals
		static QString i18n_days();        // text of 'days' units, lower case
		static QString i18n_Days();        // text of 'Days' units, initial capital
		static QString i18n_weeks();       // text of 'weeks' units, lower case
		static QString i18n_Weeks();       // text of 'Weeks' units, initial capital

	signals:
		void         toggled(bool);        // selection checkbox has been toggled

	protected slots:
		void         slotSelectToggled(bool);
		void         slotUnitsSelected(int index);

	private:
		Units        setDateOnly(int minutes, bool dateOnly);
		void         setUnitRange();

		CheckBox*    mSelect;
		TimePeriod*  mCount;
		ComboBox*    mUnitsCombo;
		QLabel*      mLabel;
		int          mMaxDays;            // maximum day count
		bool         mNoHourMinute;       // hours/minutes cannot be displayed, ever
		bool         mReadOnly;           // the widget is read only
		int          mDateOnlyOffset;     // for mUnitsCombo: 1 if hours/minutes is disabled, else 0
};

#endif // TIMESELECTOR_H
