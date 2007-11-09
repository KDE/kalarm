/*
 *  pushbutton.cpp  -  push button with read-only option
 *  Program:  kalarm
 *  Copyright (c) 2002 by David Jarvie <software@astrojar.org.uk>
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

#include "pushbutton.moc"


PushButton::PushButton(QWidget* parent, const char* name)
	: QPushButton(parent, name),
	  mFocusPolicy(focusPolicy()),
	  mReadOnly(false)
{ }

PushButton::PushButton(const QString& text, QWidget* parent, const char* name)
	: QPushButton(text, parent, name),
	  mFocusPolicy(focusPolicy()),
	  mReadOnly(false)
{ }

PushButton::PushButton(const QIconSet& icon, const QString& text, QWidget* parent, const char* name)
	: QPushButton(icon, text, parent, name),
	  mFocusPolicy(focusPolicy()),
	  mReadOnly(false)
{ }

void PushButton::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		setFocusPolicy(ro ? QWidget::NoFocus : mFocusPolicy);
		if (ro)
			clearFocus();
	}
}

void PushButton::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == Qt::LeftButton)
			return;
	}
	QPushButton::mousePressEvent(e);
}

void PushButton::mouseReleaseEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == Qt::LeftButton)
			return;
	}
	QPushButton::mouseReleaseEvent(e);
}

void PushButton::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QPushButton::mouseMoveEvent(e);
}

void PushButton::keyPressEvent(QKeyEvent* e)
{
	if (mReadOnly)
		switch (e->key())
		{
			case Qt::Key_Up:
			case Qt::Key_Left:
			case Qt::Key_Right:
			case Qt::Key_Down:
				// Process keys which shift the focus
				break;
			default:
				return;
		}
	QPushButton::keyPressEvent(e);
}

void PushButton::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QPushButton::keyReleaseEvent(e);
}
