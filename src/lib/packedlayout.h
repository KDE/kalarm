/*
 *  packedlayout.h  -  layout to pack items into rows
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

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
    void setHorizontalSpacing(int spacing);
    void setVerticalSpacing(int spacing);
    int horizontalSpacing() const;
    int verticalSpacing() const;

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
    QRect            alignRect(const QRect& rect, const QRect& itemRect) const;


    QList<QLayoutItem*> mItems;
    Qt::Alignment       mAlignment;
    int                 mHorizontalSpacing {-1};   // default = not set
    int                 mVerticalSpacing {-1};     // default = not set
    mutable int         mWidthCached {0};
    mutable int         mHeightCached;
};


// vim: et sw=4:
