/*
 *  radiobutton.cpp  -  radio button with read-only option
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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

#include "radiobutton.moc"


RadioButton::RadioButton(QWidget* parent, const char* name)
	: QRadioButton(parent, name),
	  mFocusPolicy(focusPolicy()),
	  mFocusWidget(0),
	  mReadOnly(false)
{ }

RadioButton::RadioButton(const QString& text, QWidget* parent, const char* name)
	: QRadioButton(text, parent, name),
	  mFocusPolicy(focusPolicy()),
	  mFocusWidget(0),
	  mReadOnly(false)
{ }

/******************************************************************************
*  Set the read-only status. If read-only, the button can be toggled by the
*  application, but not by the user.
*/
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

/******************************************************************************
*  Specify a widget to receive focus when the button is clicked on.
*/
void RadioButton::setFocusWidget(QWidget* w, bool enable)
{
	mFocusWidget = w;
	mFocusWidgetEnable = enable;
	if (w)
		connect(this, SIGNAL(clicked()), SLOT(slotClicked()));
	else
		disconnect(this, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

/******************************************************************************
*  Called when the button is clicked.
*  If it is now checked, focus is transferred to any specified focus widget.
*/
void RadioButton::slotClicked()
{
	if (mFocusWidget  &&  isChecked())
	{
		if (mFocusWidgetEnable)
			mFocusWidget->setEnabled(true);
		mFocusWidget->setFocus();
	}
}

/******************************************************************************
*  Event handlers to intercept events if in read-only mode.
*  Any events which could change the button state are discarded.
*/
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
