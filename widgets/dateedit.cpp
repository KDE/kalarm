/*
 *  dateedit.cpp  -  date entry widget
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

#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>

#include "dateedit.moc"


// Check a new date against any minimum date.
bool DateEdit::validate(const QDate& newDate)
{
	if (!newDate.isValid())
		return false;
	if (minDate.isValid() && newDate < minDate)
	{
		QString minString;
		if (minDate == QDate::currentDate())
			minString = i18n("today");
		else
			minString = KGlobal::locale()->formatDate(minDate, true);
		KMessageBox::sorry(this, i18n("Date cannot be earlier than %1").arg(minString));
		return false;
	}
	return true;
}

void DateEdit::mousePressEvent(QMouseEvent *e)
{
	if (isReadOnly())
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
#if QT_VERSION < 300
	if ( e->button() != LeftButton )
		return;
	QRect editRect = style().comboButtonRect(0, 0, width(), height());
	int xborder = editRect.left();
	int yborder = editRect.top();
	int left = editRect.width() + xborder;
	QRect arrowRect(left, 0, width() - left, height());
	if (arrowRect.contains(e->pos()))
		popup();
#else
	KDateEdit::mousePressEvent(e);
#endif
}

void DateEdit::mouseReleaseEvent(QMouseEvent* e)
{
	if (!isReadOnly())
		KDateEdit::mouseReleaseEvent(e);
}

void DateEdit::mouseMoveEvent(QMouseEvent* e)
{
	if (!isReadOnly())
		KDateEdit::mouseMoveEvent(e);
}

void DateEdit::keyPressEvent(QKeyEvent* e)
{
	if (!isReadOnly())
		KDateEdit::keyPressEvent(e);
}

void DateEdit::keyReleaseEvent(QKeyEvent* e)
{
	if (!isReadOnly())
		KDateEdit::keyReleaseEvent(e);
}
