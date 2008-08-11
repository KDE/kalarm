/*
 *  item.h  -  horizontal left-aligned box
 *  Program:  kalarm
 *  Copyright © 2008 by David Jarvie <djarvie@kde.org>
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

#ifndef ITEMBOX_H
#define ITEMBOX_H

#include <khbox.h>
class QHBoxLayout;


/**
 *  @short A KHBox with left-adjustment
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ItemBox : public KHBox
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 */
		explicit ItemBox(QWidget* parent = 0);
		void leftAlign();

	private:
		QHBoxLayout* mLayout;
};

#endif // ITEMBOX_H
