/*
 *  autodeletelist.h  -  pointer list with auto-delete on destruction
 *  Program:  kalarm
 *  Copyright © 2008,2009 by David Jarvie <djarvie@kde.org>
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

#ifndef AUTODELETELIST_H
#define AUTODELETELIST_H

#include <QList>


/**
 *  A list of pointers which are auto-deleted when the list is deleted.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
template <class T>
class AutoDeleteList : public QList<T*>
{
	public:
		AutoDeleteList() : QList<T*>() {}
		~AutoDeleteList()
		{
			// Remove from list first before deleting the pointer, in
			// case the pointer's destructor removes it from the list.
			while (!this->isEmpty())
				delete this->takeFirst();
		}
	private:
		// Prevent copying since that would create two owners of the pointers
		AutoDeleteList(const AutoDeleteList&);
		AutoDeleteList& operator=(const AutoDeleteList&);
};

#endif // AUTODELETELIST_H
