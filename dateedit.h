/*
 *  dateedit.h  -  date entry widget
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */
#ifndef DATEEDIT_H
#define DATEEDIT_H

#include <kdateedit.h>

class DateEdit : public KDateEdit
{
		Q_OBJECT
	public:
		DateEdit(QWidget* parent = 0, const char* name = 0)  : KDateEdit(parent, name) { }
		virtual bool validate(const QDate&);
		void setMinDate(const QDate& d)   { minDate = d; }

#if QT_VERSION < 300
	protected:
		virtual void mousePressEvent(QMouseEvent*);
#endif
	private:
		QDate  minDate;
};

#endif // DATEEDIT_H
