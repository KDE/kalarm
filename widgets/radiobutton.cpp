/*
 *  radiobutton.cpp  -  radio button with read-only option
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

#include "radiobutton.moc"


RadioButton::RadioButton(QWidget* parent, const char* name)
	: QRadioButton(parent, name),
	  mFocusPolicy(focusPolicy()),
	  mReadOnly(false)
{ }

RadioButton::RadioButton(const QString& text, QWidget* parent, const char* name)
	: QRadioButton(text, parent, name),
	  mFocusPolicy(focusPolicy()),
	  mReadOnly(false)
{ }

void RadioButton::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		setFocusPolicy(ro ? QWidget::NoFocus : mFocusPolicy);
		if (ro)
			clearFocus();
	}
}

void RadioButton::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QRadioButton::mousePressEvent(e);
}

void RadioButton::mouseReleaseEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QRadioButton::mouseReleaseEvent(e);
}

void RadioButton::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QRadioButton::mouseMoveEvent(e);
}

void RadioButton::keyPressEvent(QKeyEvent* e)
{
	if (mReadOnly)
		switch (e->key())
		{
			case Key_Up:
			case Key_Left:
			case Key_Right:
			case Key_Down:
				// Process keys which shift the focus
				break;
			default:
				return;
		}
	QRadioButton::keyPressEvent(e);
}

void RadioButton::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QRadioButton::keyReleaseEvent(e);
}
