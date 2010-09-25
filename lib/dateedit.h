/*
 *  dateedit.h  -  date entry widget
 *  Program:  kalarm
 *  Copyright © 2002-2009 by David Jarvie <djarvie@kde.org>
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
#ifndef DATEEDIT_H
#define DATEEDIT_H

#include <libkdepim/kdateedit.h>

class QMouseEvent;
class QKeyEvent;


/**
 *  @short Date edit widget with range limits.
 *
 *  The DateEdit class provides a date editor with the ability to set limits on the
 *  dates which can be entered.
 *
 *  Minimum and/or maximum permissible dates may be set, together with corresponding
 *  error messages. If the user tries to enter a date outside the allowed range, the
 *  appropriate error message (if any) is output using KMessageBox::sorry().
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class DateEdit : public KPIM::KDateEdit
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 */
		explicit DateEdit(QWidget* parent = 0);
		/** Returns true if the widget contains a valid date. */
		bool         isValid() const              { return date().isValid(); }
		/** Sets the date held in the widget to an invalid date. */
		void         setInvalid();

	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
};

#endif // DATEEDIT_H
