/*
 *  colourlist.h  -  an ordered list of colours
 *  Program:  kalarm
 *  (C) 2003 by David Jarvie  software@astrojar.org.uk
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


class ColourList
{
	public:
		typedef size_t  size_type;
		typedef QValueListConstIterator<QRgb>  const_iterator;

		ColourList()  { }
		ColourList(const ColourList& l)       : mList(l.mList) { }
		ColourList(const QValueList<QRgb>& l) : mList(l) { qHeapSort(mList); }
		ColourList(const QColor*);
		ColourList&    operator=(const ColourList& l)        { mList = l.mList;  return *this; }
		ColourList&    operator=(const QValueList<QRgb>& l)  { mList = l;  qHeapSort(mList);  return *this; }
		void           clear()                               { mList.clear(); }
		void           insert(const QColor&);
		void           remove(const QColor& c)               { mList.remove(c.rgb()); }
		ColourList&    operator+=(const QColor& c)           { insert(c);  return *this; }
		ColourList&    operator+=(const ColourList& l)       { mList += l.mList;  qHeapSort(mList);  return *this; }

		bool           operator==(const ColourList& l) const { return mList == l.mList; }
		bool           operator!=(const ColourList& l) const { return mList != l.mList; }
		size_type      count() const                         { return mList.count(); }
		bool           isEmpty() const                       { return mList.isEmpty(); }
		const_iterator begin() const                         { return mList.begin(); }
		const_iterator end() const                           { return mList.end(); }
		const_iterator fromLast() const                      { return mList.fromLast(); }
		const_iterator at(size_type i) const                 { return mList.at(i); }
		size_type      contains(const QColor& c) const       { return mList.contains(c.rgb()); }
		const_iterator find(const QColor& c) const           { return mList.find(c.rgb()); }
		const_iterator find(const_iterator it, const QColor& c) const   { return mList.find(it, c.rgb()); }
		int            findIndex(const QColor& c) const      { return mList.findIndex(c.rgb()); }
		QColor         first() const                         { return QColor(mList.first()); }
		QColor         last() const                          { return QColor(mList.last()); }
		QColor         operator[](size_type i) const         { return QColor(mList[i]); }
	private:
		void              sort();
		QValueList<QRgb>  mList;
};

#endif // COLOURLIST_H
