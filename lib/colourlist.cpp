/*
 *  colourlist.cpp  -  an ordered list of colours
 *  Program:  kalarm
 *  Copyright (C) 2003 by David Jarvie  software@astrojar.org.uk
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

#include "colourlist.h"


ColourList::ColourList(const QColor* colours)
{
	while (colours->isValid())
		mList.append((*colours++).rgb());
}

void ColourList::insert(const QColor& colour)
{
	QRgb rgb = colour.rgb();
	for (QValueListIterator<QRgb> it = mList.begin();  it != mList.end();  ++it)
	{
		if (rgb <= *it)
		{
			if (rgb != *it)    // don't insert duplicates
				mList.insert(it, rgb);
			return;
		}
	}
	mList.append(rgb);
}
