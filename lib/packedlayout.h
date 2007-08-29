/*
 *  packedlayout.h  -  layout to pack items into rows
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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
#ifndef PACKEDLAYOUT_H
#define PACKEDLAYOUT_H

#include <QLayout>
#include <QList>


/**
 *  The PackedLayout class packs a group of widgets into rows.
 *  The widgets are arranged according to the total width available.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class PackedLayout : public QLayout
{
	public:
		/** Constructor.
		 *  @param parent the parent widget
		 *  @param alignment how to align the widgets horizontally within the layout
		 */
		PackedLayout(QWidget* parent, Qt::Alignment alignment);
		explicit PackedLayout(Qt::Alignment alignment);
		~PackedLayout();
		// Override QLayout methods
		virtual bool hasHeightForWidth() const  { return true; }
		virtual int heightForWidth(int w) const;
		virtual int count() const  { return mItems.count(); }
		virtual void addItem(QLayoutItem* item);
		virtual QLayoutItem* itemAt(int index) const;
		virtual QLayoutItem* takeAt(int index);
		virtual void setGeometry(const QRect& r);
		virtual QSize sizeHint() const  { return minimumSize(); }
		virtual QSize minimumSize() const;
		virtual Qt::Orientations expandingDirections() const  { return Qt::Vertical | Qt::Horizontal; }

	private:
		int arrange(const QRect&, bool set) const;
		QList<QLayoutItem*> mItems;
		Qt::Alignment mAlignment;
};

#endif // PACKEDLAYOUT_H
