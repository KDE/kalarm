/*
 *  colourcombo.cpp  -  colour selection combo box
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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

#include "colourcombo.h"


ColourCombo::ColourCombo(QWidget* parent, const char* name, const QColor& defaultColour)
	: KColorCombo(parent, name),
	  mSelection(defaultColour),
	  mReadOnly(false),
	  mDisabled(false)
{
	deleteColours();
}

void ColourCombo::setColour(const QColor& col)
{
	mSelection = col;
	KColorCombo::setColor(col);
	deleteColours();
}

/******************************************************************************
*  This function removes the unwanted colours which KColorCombo inserts,
*  and resets the current selection to the correct item.
*/
void ColourCombo::deleteColours()
{
	// Remove all colours except bright ones.
	// Also leave the "Custom..." item, which is the first in the list.
	int selitem = 0;
	for (int i = count();  --i > 0;  )
	{
		setCurrentItem(i);
		QColor colour = color();
		if (colour.red() != 255  &&  colour.green() != 255  &&  colour.blue() != 255)
		{
			removeItem(i);
			if (selitem)
				--selitem;
		}
		else if (colour == mSelection)
			selitem = i;
	}
	if (mDisabled)
		addDisabledColour();
	else
		setCurrentItem(selitem);
}

void ColourCombo::setEnabled(bool enable)
{
	if (enable  &&  mDisabled)
	{
		mDisabled = false;
		setColour(mSelection);
	}
	else if (!enable  &&  !mDisabled)
	{
		mSelection = color();
		addDisabledColour();
		mDisabled = true;
	}
	KColorCombo::setEnabled(enable);
}

void ColourCombo::addDisabledColour()
{
	int end = count();
	if (end > 1)
	{
		QPixmap pm = *pixmap(1);
		pm.fill(backgroundColor());
		insertItem(pm);
		setCurrentItem(end);
	}
}

void ColourCombo::resizeEvent(QResizeEvent* re)
{
	KColorCombo::resizeEvent(re);
	deleteColours();
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
		if (e->button() == LeftButton)
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
	if (!mReadOnly)
		KColorCombo::keyPressEvent(e);
}

void ColourCombo::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		KColorCombo::keyReleaseEvent(e);
}
