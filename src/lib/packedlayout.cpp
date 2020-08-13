/*
 *  packedlayout.cpp  -  layout to pack items into rows
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "packedlayout.h"


PackedLayout::PackedLayout(QWidget* parent, Qt::Alignment alignment)
    : QLayout(parent)
    , mAlignment(alignment)
{
}

PackedLayout::PackedLayout(Qt::Alignment alignment)
    : QLayout()
    , mAlignment(alignment)
{
}

PackedLayout::~PackedLayout()
{
    while (!mItems.isEmpty())
        delete mItems.takeFirst();
}

void PackedLayout::setHorizontalSpacing(int spacing)
{
    mHorizontalSpacing = spacing;
}

void PackedLayout::setVerticalSpacing(int spacing)
{
    mVerticalSpacing = spacing;
}

int PackedLayout::horizontalSpacing() const
{
    return (mHorizontalSpacing >= 0) ? mHorizontalSpacing : spacing();
}

int PackedLayout::verticalSpacing() const
{
    return (mVerticalSpacing >= 0) ? mVerticalSpacing : spacing();
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
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    return QSize(size.width() + left + right, size.height() + top + bottom);
}

// Arranges widgets and returns height required.
int PackedLayout::arrange(const QRect& rect, bool set) const
{
    int x = rect.x();
    int y = rect.y();
    int yrow = 0;
    QList<QRect> posn;
    QList<QLayoutItem*> items;
    for (QLayoutItem* item : qAsConst(mItems))
    {
        if (item->isEmpty())
            continue;
        const QSize size = item->sizeHint();
        int right = x + size.width();
        if (right > rect.right()  &&  x > rect.x())
        {
            x = rect.x();
            y = y + yrow + verticalSpacing();
            right = x + size.width();
            yrow = size.height();
        }
        else
            yrow = qMax(yrow, size.height());
        items.append(item);
        posn.append(QRect(QPoint(x, y), size));
        x = right + horizontalSpacing();
    }
    if (set)
    {
        const int count = items.count();
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
                const int n = last - i;   // number of items in this row
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
                        Q_FALLTHROUGH();
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
