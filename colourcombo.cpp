/*
 *  colourcombo.cpp  -  colour selection combo box
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "colourcombo.h"


ColourCombo::ColourCombo(QWidget* parent, const char* name, const QColor& defaultColour)
   :  KColorCombo(parent, name),
      selection(defaultColour)
{
	deleteColours();
}

void ColourCombo::setColour(const QColor& col)
{
	selection = col;
	KColorCombo::setColor(col);
	deleteColours();
}

void ColourCombo::resizeEvent(QResizeEvent* re)
{
	KColorCombo::resizeEvent(re);
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
		else if (colour == selection)
			selitem = i;
	}
	setCurrentItem(selitem);
}
