/*
 *  packedlayout.h  -  layout to pack items into rows
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <djarvie@kde.org>
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
        ~PackedLayout();
        // Override QLayout methods
        virtual bool hasHeightForWidth() const Q_DECL_OVERRIDE  { return true; }
        int heightForWidth(int w) const Q_DECL_OVERRIDE;
        virtual int count() const Q_DECL_OVERRIDE  { return mItems.count(); }
        void addItem(QLayoutItem* item) Q_DECL_OVERRIDE;
        QLayoutItem* itemAt(int index) const Q_DECL_OVERRIDE;
        QLayoutItem* takeAt(int index) Q_DECL_OVERRIDE;
        void setGeometry(const QRect& r) Q_DECL_OVERRIDE;
        QSize sizeHint() const  Q_DECL_OVERRIDE { return minimumSize(); }
        QSize minimumSize() const Q_DECL_OVERRIDE;
        virtual Qt::Orientations expandingDirections() const Q_DECL_OVERRIDE  { return Qt::Vertical | Qt::Horizontal; }
        virtual void invalidate() Q_DECL_OVERRIDE  { mWidthCached = mHeightCached = false; }

    private:
        int arrange(const QRect&, bool set) const;
        QList<QLayoutItem*> mItems;
        Qt::Alignment mAlignment;
        mutable int mWidthCached;
        mutable int mHeightCached;
};

#endif // PACKEDLAYOUT_H

// vim: et sw=4:
