/*
 *  colourlist.h  -  an ordered list of colours
 *  Program:  kalarm
 *  Copyright (c) 2003, 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef COLOURLIST_H
#define COLOURLIST_H

#include <QtAlgorithms>
#include <QColor>
#include <QList>


/**
 *  @short Represents a sorted list of colours.
 *
 *  The ColourList class holds a list of colours, sorted in RGB value order.
 *
 *  It provides a sorted QList of colours in RGB value order, with iterators
 *  and other access methods which return either QRgb or QColor objects.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ColourList
{
	public:
		/** Constructs an empty list. */
		ColourList()  { }
		/** Copy constructor. */
		ColourList(const ColourList& l)       : mList(l.mList) { }
		/** Constructs a list whose values are preset to the colours in @p list. */
		ColourList(const QList<QRgb>& list)   : mList(list) { qSort(mList); }
		/** Constructs a list whose values are preset to the colours in the @p list.
		 *  Terminate @p list by an invalid colour.
		 */
		ColourList(const QColor* list);
		/** Assignment operator. */
		ColourList&    operator=(const ColourList& l)        { mList = l.mList;  return *this; }
		/** Sets the list to comprise the colours in @p list. */
		ColourList&    operator=(const QList<QRgb>& list)    { mList = list;  qSort(mList);  return *this; }
		/** Removes all values from the list. */
		void           clear()                               { mList.clear(); }
		/** Adds the specified colour @p c to the list, in the correct sorted position. */
		void           insert(const QColor& c);
		/** Removes the colour @p c from the list. */
		void           remove(const QColor& c)               { mList.removeAt(mList.indexOf(c.rgb())); }
		/** Adds the specified colour @p c to the list. */
		ColourList&    operator+=(const QColor& c)           { insert(c);  return *this; }
		/** Adds the colours in @p list to this list. */
		ColourList&    operator+=(const ColourList& list)    { mList += list.mList;  qSort(mList);  return *this; }
		/** Returns true if the colours in the two lists are the same. */
		bool           operator==(const ColourList& l) const { return mList == l.mList; }
		/** Returns true if the colours in the two lists differ. */
		bool           operator!=(const ColourList& l) const { return mList != l.mList; }
		/** Returns the number of colours in the list. */
		int            count() const                         { return mList.count(); }
		/** Returns true if the list is empty. */
		bool           isEmpty() const                       { return mList.isEmpty(); }
		/** Returns true if the list contains the colour @p c. */
		bool           contains(const QColor& c) const       { return mList.contains(c.rgb()); }
		/** Finds the first occurrence of colour @p c in the list.
		 *  @return index to first occurrence of @p c, or -1 if @p c is not in the list
		 */
		int            indexOf(const QColor& c, int from = 0) const     { return mList.indexOf(c.rgb(), from); }
		/** Returns the colour at position @p i in the list. The item must exist. */
		QColor         operator[](int i) const               { return QColor(mList[i]); }

	private:
		void           sort();
		QList<QRgb>    mList;
};

#endif // COLOURLIST_H
