/*
 *  checkbox.cpp  -  check box with read-only option
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
 */

#include "checkbox.moc"


CheckBox::CheckBox(QWidget* parent, const char* name)
	: QCheckBox(parent, name),
	  mFocusPolicy(focusPolicy()),
	  mFocusWidget(0),
	  mReadOnly(false)
{ }

CheckBox::CheckBox(const QString& text, QWidget* parent, const char* name)
	: QCheckBox(text, parent, name),
	  mFocusPolicy(focusPolicy()),
	  mFocusWidget(0),
	  mReadOnly(false)
{ }

/******************************************************************************
*  Set the read-only status. If read-only, the checkbox can be toggled by the
*  application, but not by the user.
*/
void CheckBox::setReadOnly(bool ro)
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
*  Specify a widget to receive focus when the checkbox is clicked on.
*/
void CheckBox::setFocusWidget(QWidget* w, bool enable)
{
	mFocusWidget = w;
	mFocusWidgetEnable = enable;
	if (w)
		connect(this, SIGNAL(clicked()), SLOT(slotClicked()));
	else
		disconnect(this, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

/******************************************************************************
*  Called when the checkbox is clicked.
*  If it is now checked, focus is transferred to any specified focus widget.
*/
void CheckBox::slotClicked()
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
*  Any events which could change the checkbox state are discarded.
*/
void CheckBox::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QCheckBox::mousePressEvent(e);
}

void CheckBox::mouseReleaseEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QCheckBox::mouseReleaseEvent(e);
}

void CheckBox::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QCheckBox::mouseMoveEvent(e);
}

void CheckBox::keyPressEvent(QKeyEvent* e)
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
	QCheckBox::keyPressEvent(e);
}

void CheckBox::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QCheckBox::keyReleaseEvent(e);
}
