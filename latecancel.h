/*
 *  latecancel.h  -  widget to specify cancellation if late
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

#ifndef LATECANCEL_H
#define LATECANCEL_H

#include <qframe.h>

#include "timeperiod.h"
#include "timeselector.h"
class QBoxLayout;
class QWidgetStack;
class CheckBox;


class LateCancelSelector : public QFrame
{
		Q_OBJECT
	public:
		LateCancelSelector(bool allowHourMinute, QWidget* parent, const char* name = 0);
		int            minutes() const;
		void           setMinutes(int Minutes, bool dateOnly, TimePeriod::Units defaultUnits);
		void           setDateOnly(bool dateOnly);
		void           showAutoClose(bool show);
		bool           isAutoClose() const;
		void           setAutoClose(bool autoClose);
		bool           isReadOnly() const     { return mReadOnly; }
		void           setReadOnly(bool);

		static QString i18n_CancelIfLate();     // plain text of 'Cancel if late' checkbox
		static QString i18n_n_CancelIfLate();   // text of 'Cancel if late' checkbox, with 'N' shortcut
		static QString i18n_AutoCloseWin();     // plain text of 'Auto-close window after this time' checkbox
		static QString i18n_AutoCloseWinLC();   // plain text of 'Auto-close window after late-cancelation time' checkbox
		static QString i18n_i_AutoCloseWinLC(); // text of 'Auto-close window after late-cancelation time' checkbox, with 'I' shortcut

	private slots:
		void           slotToggled(bool);

	private:
		QBoxLayout*    mLayout;            // overall layout for the widget
		QWidgetStack*  mStack;             // contains mCheckboxFrame and mTimeSelectorFrame
		QFrame*        mCheckboxFrame;
		CheckBox*      mCheckbox;          // displayed when late cancellation is not selected
		QFrame*        mTimeSelectorFrame;
		TimeSelector*  mTimeSelector;      // displayed when late cancellation is selected
		CheckBox*      mAutoClose;
		bool           mDateOnly;          // hours/minutes units not allowed
		bool           mReadOnly;          // widget is read-only
		bool           mAutoCloseShown;    // auto-close checkbox is visible
};

#endif // LATECANCEL_H
