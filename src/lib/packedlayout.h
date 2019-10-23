/*
 *  packedlayout.h  -  layout to pack items into rows
 *  Program:  kalarm
 *  Copyright © 2007 David Jarvie <djarvie@kde.org>
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
 *  @author David Jarvie <djarvie@kde.org>
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
        ~PackedLayout() override;
        // Override QLayout methods
        bool             hasHeightForWidth() const override  { return true; }
        int              heightForWidth(int w) const override;
        int              count() const override  { return mItems.count(); }
        void             addItem(QLayoutItem* item) override;
        QLayoutItem*     itemAt(int index) const override;
        QLayoutItem*     takeAt(int index) override;
        void             setGeometry(const QRect& r) override;
        QSize            sizeHint() const  override { return minimumSize(); }
        QSize            minimumSize() const override;
        Qt::Orientations expandingDirections() const override  { return Qt::Vertical | Qt::Horizontal; }
        void             invalidate() override  { mWidthCached = mHeightCached = false; }

    private:
        int              arrange(const QRect&, bool set) const;

        QList<QLayoutItem*> mItems;
        Qt::Alignment       mAlignment;
        mutable int         mWidthCached{0};
        mutable int         mHeightCached;
};

#endif // PACKEDLAYOUT_H

// vim: et sw=4:
