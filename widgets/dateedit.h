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

class DateEdit : public KDateEdit
{
		Q_OBJECT
	public:
		DateEdit(QWidget* parent = 0, const char* name = 0);
		virtual bool validate(const QDate&);
		bool         isValid() const              { return inputIsValid(); }
		const QDate& minDate() const              { return mMinDate; }
		const QDate& maxDate() const              { return mMaxDate; }
		void         setMinDate(const QDate&, const QString& errorDate = QString::null);
		void         setMaxDate(const QDate&, const QString& errorDate = QString::null);
		void         setValid(bool);

	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);

	protected slots:
		void         slotDateChanged(QDate);

	private:
		void         pastLimitMessage(const QDate& limit, const QString& error, const QString& defaultError);

		QDate        mMinDate;             // minimum allowed date, or invalid for no minimum
		QDate        mMaxDate;             // maximum allowed date, or invalid for no maximum
		QString      mMinDateErrString;    // error message when entered date < mMinDate
		QString      mMaxDateErrString;    // error message when entered date > mMaxDate
};

#endif // DATEEDIT_H
