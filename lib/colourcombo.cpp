/*
 *  colourcombo.cpp  -  colour selection combo box
 *  Program:  kalarm
 *  Copyright (c) 2001-2003,2005-2007 by David Jarvie <software@astrojar.org.uk>
 *
 *  Some code taken from kdelibs/kdeui/kcolorcombo.cpp in the KDE libraries:
 *  Copyright (C) 1997 Martin Jones (mjones@kde.org)
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

#include <QMouseEvent>
#include <QKeyEvent>

#include "preferences.h"
#include "colourcombo.moc"


ColourCombo::ColourCombo(QWidget* parent, const QColor& defaultColour)
	: KColorCombo(parent),
	  mReadOnly(false)
{
	setColours(Preferences::messageColours());
	setColor(defaultColour);
	Preferences::connect(SIGNAL(messageColoursChanged()), this, SLOT(slotPreferencesChanged()));
}

/******************************************************************************
* Set a new colour selection.
*/
void ColourCombo::setColours(const ColourList& colours)
{
	setColors(colours.qcolorList());
}

/******************************************************************************
* Called when the user changes the colour list in the preference settings.
*/
void ColourCombo::slotPreferencesChanged()
{
	setColors(Preferences::messageColours().qcolorList());
}

void ColourCombo::setReadOnly(bool ro)
{
	mReadOnly = ro;
}

void ColourCombo::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == Qt::LeftButton)
			return;
	}
	KColorCombo::mousePressEvent(e);
}

void ColourCombo::mouseReleaseEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		KColorCombo::mouseReleaseEvent(e);
}

void ColourCombo::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		KColorCombo::mouseMoveEvent(e);
}

void ColourCombo::keyPressEvent(QKeyEvent* e)
{
	if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
		KColorCombo::keyPressEvent(e);
}

void ColourCombo::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		KColorCombo::keyReleaseEvent(e);
}
