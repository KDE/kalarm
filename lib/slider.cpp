/*
 *  slider.cpp  -  slider control with read-only option
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie  software@astrojar.org.uk
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

#include "slider.moc"


Slider::Slider(QWidget* parent, const char* name)
	: QSlider(parent, name),
	  mReadOnly(false)
{ }

Slider::Slider(Orientation o, QWidget* parent, const char* name)
	: QSlider(o, parent, name),
	  mReadOnly(false)
{ }

Slider::Slider(int minval, int maxval, int pageStep, int value, Orientation o, QWidget* parent, const char* name)
	: QSlider(minval, maxval, pageStep, value, o, parent, name),
	  mReadOnly(false)
{ }

/******************************************************************************
*  Set the read-only status. If read-only, the slider can be moved by the
*  application, but not by the user.
*/
void Slider::setReadOnly(bool ro)
{
	mReadOnly = ro;
}

/******************************************************************************
*  Event handlers to intercept events if in read-only mode.
*  Any events which could change the slider value are discarded.
*/
void Slider::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QSlider::mousePressEvent(e);
}

void Slider::mouseReleaseEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QSlider::mouseReleaseEvent(e);
}

void Slider::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QSlider::mouseMoveEvent(e);
}

void Slider::keyPressEvent(QKeyEvent* e)
{
	if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
		QSlider::keyPressEvent(e);
}

void Slider::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QSlider::keyReleaseEvent(e);
}
