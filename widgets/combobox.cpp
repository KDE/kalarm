/*
 *  combobox.cpp  -  combo box with read-only option
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include <qlineedit.h>
#include "combobox.moc"


ComboBox::ComboBox(QWidget* parent, const char* name)
	: QComboBox(parent, name),
	  mReadOnly(false)
{ }

ComboBox::ComboBox(bool rw, QWidget* parent, const char* name)
	: QComboBox(rw, parent, name),
	  mReadOnly(false)
{ }

void ComboBox::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		if (lineEdit())
			lineEdit()->setReadOnly(ro);
	}
}

void ComboBox::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QComboBox::mousePressEvent(e);
}

void ComboBox::mouseReleaseEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QComboBox::mouseReleaseEvent(e);
}

void ComboBox::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QComboBox::mouseMoveEvent(e);
}

void ComboBox::keyPressEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QComboBox::keyPressEvent(e);
}

void ComboBox::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QComboBox::keyReleaseEvent(e);
}
