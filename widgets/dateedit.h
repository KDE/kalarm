/*
 *  dateedit.h  -  date entry widget
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#include <kdateedit.h>

class DateEdit : public KDateEdit
{
		Q_OBJECT
	public:
		DateEdit(QWidget* parent = 0, const char* name = 0);
		virtual bool validate(const QDate&);
		bool         isValid() const              { return inputIsValid(); }
		void         setMinDate(const QDate&, const QString& errorDate = QString::null);
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
		QDate    mMinDate;             // minimum allowed date, or invalid for no minimum
		QString  mErrorDateString;     // error message when entered date < mMinDate
};

#endif // DATEEDIT_H
