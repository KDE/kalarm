/*
 *  colourlist.cpp  -  an ordered list of colours
 *  Program:  kalarm
 *  Copyright © 2003,2005,2007 by David Jarvie <software@astrojar.org.uk>
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
		mList << (*colours++).rgb();
	qSort(mList);
}

void ColourList::insert(const QColor& colour)
{
	QRgb rgb = colour.rgb();
	for (int i = 0, count = mList.count();  i < count;  ++i)
	{
		if (rgb <= mList[i])
		{
			if (rgb != mList[i])    // don't insert duplicates
				mList.insert(i, rgb);
			return;
		}
	}
	mList.append(rgb);
}

ColourList& ColourList::operator=(const QList<QColor>& list)
{
	mList.clear();
	for (int i = 0, count = list.count();  i < count;  ++i)
		mList << list[i].rgb();
	qSort(mList);
	return *this;
}

QList<QColor> ColourList::qcolorList() const
{
	QList<QColor> list;
	for (int i = 0, count = mList.count();  i < count;  ++i)
		list << QColor(mList[i]);
	return list;
}
