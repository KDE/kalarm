/*
 *  dateedit.h  -  date entry widget
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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
#ifndef DATEEDIT_H
#define DATEEDIT_H

#include <libkdepim/kdateedit.h>

/**
 *  The DateEdit class provides a date editor with the ability to set limits on the
 *  dates which can be entered.
 *
 *  Minimum and/or maximum permissible dates may be set, together with corresponding
 *  error messages. If the user tries to enter a date outside the allowed range, the
 *  appropriate error message (if any) is output using KMessageBox::sorry().
 *
 *  @short Date edit widget with range limits.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class DateEdit : public KDateEdit
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		DateEdit(QWidget* parent = 0, const char* name = 0);
		/** Returns true if the widget contains a valid date. */
		bool         isValid() const              { return date().isValid(); }
		/** Returns the earliest date which can be entered.
		 *  If there is no minimum date, returns an invalid date.
		 */
		const QDate& minDate() const              { return mMinDate; }
		/** Returns the latest date which can be entered.
		 *  If there is no maximum date, returns an invalid date.
		 */
		const QDate& maxDate() const              { return mMaxDate; }
		/** Sets the earliest date which can be entered.
		 *  @param date Earliest date allowed. If invalid, any minimum limit is removed.
		 *  @param errorDate Error message to be displayed when a date earlier than
		 *         @p date is entered. Set to QString::null to use the default error message.
		 */
		void         setMinDate(const QDate& date, const QString& errorDate = QString::null);
		/** Sets the latest date which can be entered.
		 *  @param date Latest date allowed. If invalid, any maximum limit is removed.
		 *  @param errorDate Error message to be displayed when a date later than
		 *         @p date is entered. Set to QString::null to use the default error message.
		 */
		void         setMaxDate(const QDate& date, const QString& errorDate = QString::null);
		/** Sets the date held in the widget to an invalid date. */
		void         setInvalid();

	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
		/** Checks whether @p date lies within the allowed range of values.
		 *  If so, sets the new value. If not, an error message is displayed.
		 */
		virtual bool assignDate(const QDate& date);

	private:
		void         pastLimitMessage(const QDate& limit, const QString& error, const QString& defaultError);

		QDate        mMinDate;             // minimum allowed date, or invalid for no minimum
		QDate        mMaxDate;             // maximum allowed date, or invalid for no maximum
		QString      mMinDateErrString;    // error message when entered date < mMinDate
		QString      mMaxDateErrString;    // error message when entered date > mMaxDate
};

#endif // DATEEDIT_H
