/*
 *  colourlist.h  -  an ordered list of colours
 *  Program:  kalarm
 *  (C) 2003, 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef COLOURLIST_H
#define COLOURLIST_H

#include <qtl.h>
#include <qcolor.h>
#include <qvaluelist.h>


/**
 *  The ColourList class holds a list of colours, sorted in RGB value order.
 *
 *  It provides a sorted QValueList of colours in RGB value order, with iterators
 *  and other access methods which return either QRgb or QColor objects.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ColourList
{
	public:
		typedef size_t  size_type;
		typedef QValueListConstIterator<QRgb>  const_iterator;

		/** Constructs an empty list. */
		ColourList()  { }
		/** Copy constructor. */
		ColourList(const ColourList& l)       : mList(l.mList) { }
		/** Constructs a list whose values are preset to the colours in @p list. */
		ColourList(const QValueList<QRgb>& list) : mList(list) { qHeapSort(mList); }
		/** Constructs a list whose values are preset to the colours in the @p list.
		 *  Terminate @p list by an invalid colour.
		 */
		ColourList(const QColor* list);
		/** Assignment operator. */
		ColourList&    operator=(const ColourList& l)        { mList = l.mList;  return *this; }
		/** Sets the list to comprise the colours in @p list. */
		ColourList&    operator=(const QValueList<QRgb>& list) { mList = list;  qHeapSort(mList);  return *this; }
		/** Removes all values from the list. */
		void           clear()                               { mList.clear(); }
		/** Adds the specified colour @p c to the list. */
		void           insert(const QColor& c);
		/** Removes the colour @p c from the list. */
		void           remove(const QColor& c)               { mList.remove(c.rgb()); }
		/** Adds the specified colour @p c to the list. */
		ColourList&    operator+=(const QColor& c)           { insert(c);  return *this; }
		/** Adds the colours in @p list to this list. */
		ColourList&    operator+=(const ColourList& list)    { mList += list.mList;  qHeapSort(mList);  return *this; }
		/** Returns true if the colours in the two lists are the same. */
		bool           operator==(const ColourList& l) const { return mList == l.mList; }
		/** Returns true if the colours in the two lists differ. */
		bool           operator!=(const ColourList& l) const { return mList != l.mList; }
		/** Returns the number of colours in the list. */
		size_type      count() const                         { return mList.count(); }
		/** Returns true if the list is empty. */
		bool           isEmpty() const                       { return mList.isEmpty(); }
		/** Returns an iterator pointing to the first colour in the list. */
		const_iterator begin() const                         { return mList.begin(); }
		/** Returns an iterator pointing past the last colour in the list. */
		const_iterator end() const                           { return mList.end(); }
		/** Returns an iterator pointing to the last colour in the list, or end() if the list is empty. */
		const_iterator fromLast() const                      { return mList.fromLast(); }
		/** Returns an iterator pointing to the colour at position @p i in the list. */
		const_iterator at(size_type i) const                 { return mList.at(i); }
		/** Returns true if the list contains the colour @p c. */
		size_type      contains(const QColor& c) const       { return mList.contains(c.rgb()); }
		/** Returns an iterator pointing to the first occurrence of colour @p c in the list.
		 *  Returns end() if colour @p c is not in the list.
		 */
		const_iterator find(const QColor& c) const           { return mList.find(c.rgb()); }
		/** Returns an iterator pointing to the first occurrence of colour @p c in the list, starting.
		 *  from position @p it. Returns end() if colour @p c is not in the list.
		 */
		const_iterator find(const_iterator it, const QColor& c) const   { return mList.find(it, c.rgb()); }
		/** Returns the index to the first occurrence of colour @p c in the list.
		 *  Returns -1 if colour @p c is not in the list.
		 */
		int            findIndex(const QColor& c) const      { return mList.findIndex(c.rgb()); }
		/** Returns the first colour in the list. If the list is empty, the result is undefined. */
		QColor         first() const                         { return QColor(mList.first()); }
		/** Returns the last colour in the list. If the list is empty, the result is undefined. */
		QColor         last() const                          { return QColor(mList.last()); }
		/** Returns the colour at position @p i in the list. If the item does not exist, the result is undefined. */
		QColor         operator[](size_type i) const         { return QColor(mList[i]); }
	private:
		void              sort();
		QValueList<QRgb>  mList;
};

#endif // COLOURLIST_H
