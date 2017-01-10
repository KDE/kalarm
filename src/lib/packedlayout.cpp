/*
 *  packedlayout.cpp  -  layout to pack items into rows
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
#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "packedlayout.h"


PackedLayout::PackedLayout(QWidget* parent, Qt::Alignment alignment)
    : QLayout(parent),
      mAlignment(alignment),
      mWidthCached(0)
{
}

PackedLayout::PackedLayout(Qt::Alignment alignment)
    : QLayout(),
      mAlignment(alignment),
      mWidthCached(0)
{
}

PackedLayout::~PackedLayout()
{
    while (!mItems.isEmpty())
        delete mItems.takeFirst();
}

/******************************************************************************
 * Inserts a button into the group.
 */
void PackedLayout::addItem(QLayoutItem* item)
{
    mWidthCached = 0;
    mItems.append(item);
}

QLayoutItem* PackedLayout::itemAt(int index) const
{
    return (index >= 0 && index < mItems.count()) ? mItems[index] : nullptr;
}

QLayoutItem* PackedLayout::takeAt(int index)
{
    if (index < 0  ||  index >= mItems.count())
        return nullptr;
    return mItems.takeAt(index);
}

int PackedLayout::heightForWidth(int w) const
{
    if (w != mWidthCached)
    {
        mHeightCached = arrange(QRect(0, 0, w, 0), false);
        mWidthCached = w;
    }
    return mHeightCached;
}

void PackedLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

// Return the maximum size of any widget.
QSize PackedLayout::minimumSize() const
{
    QSize size;
    for (int i = 0, end = mItems.count();  i < end;  ++i)
        size = size.expandedTo(mItems[i]->minimumSize());
    int m = margin() * 2;
    return QSize(size.width() + m, size.height() + m);
}

// Arranges widgets and returns height required.
int PackedLayout::arrange(const QRect& rect, bool set) const
{
    int x = rect.x();
    int y = rect.y();
    int yrow = 0;
    QList<QRect> posn;
    int end = mItems.count();
    QList<QLayoutItem*> items;
    for (int i = 0;  i < end;  ++i)
    {
        QLayoutItem* item = mItems[i];
        if (item->isEmpty())
            continue;
        QSize size = item->sizeHint();
        int right = x + size.width();
        if (right > rect.right()  &&  x > rect.x())
        {
            x = rect.x();
            y = y + yrow + spacing();
            right = x + size.width();
            yrow = size.height();
        }
        else
            yrow = qMax(yrow, size.height());
        items.append(item);
        posn.append(QRect(QPoint(x, y), size));
        x = right + spacing();
    }
    if (set)
    {
        int count = items.count();
        if (mAlignment == Qt::AlignLeft)
        {
            // Left aligned: no position adjustment needed
            // Set the positions of all the layout items
            for (int i = 0;  i < count;  ++i)
                items[i]->setGeometry(posn[i]);
        }
        else
        {
            // Set the positions of all the layout items
            for (int i = 0;  i < count; )
            {
                // Adjust item positions a row at a time
                y = posn[i].y();
                int last;   // after last item in this row
                for (last = i + 1;  last < count && posn[last].y() == y;  ++last) ;
                int n = last - i;   // number of items in this row
                int free = rect.right() - posn[last - 1].right();
                switch (mAlignment)
                {
                    case Qt::AlignJustify:
                        if (n == 1)
                        {
                            items[i]->setGeometry(posn[i]);
                            ++i;
                        }
                        else if (n > 1)
                        {
                            for (int j = 0;  i < last;  ++j, ++i)
                                items[i]->setGeometry(QRect(QPoint(posn[i].x() + (free * j)/(n - 1), y), posn[i].size()));
                        }
                        break;
                    case Qt::AlignHCenter:
                        free /= 2;
                        // fall through to AlignRight
                    case Qt::AlignRight:
                        for ( ;  i < last;  ++i)
                            items[i]->setGeometry(QRect(QPoint(posn[i].x() + free, y), posn[i].size()));
                        break;
                    default:
                        break;
                }
            }
        }
    }
    return y + yrow - rect.y();
}

// vim: et sw=4:
