/*
 *  stackedwidgets.h  -  group of stacked widgets
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

#ifndef STACKEDWIDGETS_H
#define STACKEDWIDGETS_H

#include <QList>
#include <QScrollArea>
class QDialog;

template <class T> class StackedGroupT;

/**
 *  A widget contained in a stack, whose minimum size hint is that of the largest
 *  widget in the stack. This class works together with StackedGroup.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
template <class T>
class StackedWidgetT : public T
{
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         *  @param name The name of this widget.
         */
        explicit StackedWidgetT(StackedGroupT<T>* group, QWidget* parent = 0)
              : T(parent),
                mGroup(group)
        {
            mGroup->addWidget(this);
        }
        ~StackedWidgetT()  { mGroup->removeWidget(this); }
        virtual QSize sizeHint() const         { return minimumSizeHint(); }
        virtual QSize minimumSizeHint() const  { return mGroup->minimumSizeHint(); }

    private:
        StackedGroupT<T>* mGroup;
};

/**
 *  A group of stacked widgets whose minimum size hints are all equal to the
 *  largest widget's minimum size hint.
 *
 *  It is inherited from QObject solely to ensure automatic deletion when its
 *  parent widget is deleted.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
template <class T>
class StackedGroupT : public QObject
{
    public:
        explicit StackedGroupT(QObject* parent = 0) : QObject(parent) {}
        void  addWidget(StackedWidgetT<T>* w)     { mWidgets += w; }
        void  removeWidget(StackedWidgetT<T>* w)  { mWidgets.removeAll(w); }
        virtual QSize minimumSizeHint() const;

    protected:
        QList<StackedWidgetT<T>*> mWidgets;
};

template <class T>
QSize StackedGroupT<T>::minimumSizeHint() const
{
    QSize sz;
    for (int i = 0, count = mWidgets.count();  i < count;  ++i)
        sz = sz.expandedTo(mWidgets[i]->T::minimumSizeHint());
    return sz;
}

/** A non-scrollable stacked widget. */
typedef StackedWidgetT<QWidget> StackedWidget;
/** A group of non-scrollable stacked widgets. */
typedef StackedGroupT<QWidget> StackedGroup;


class StackedScrollGroup;

/**
 *  A stacked widget which becomes scrollable when necessary to fit the height
 *  of the screen.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class StackedScrollWidget : public StackedWidgetT<QScrollArea>
{
    public:
        explicit StackedScrollWidget(StackedScrollGroup* group, QWidget* parent = 0);
        QWidget* widget() const  { return viewport()->findChild<QWidget*>(); }
};

/**
 *  A group of stacked widgets which individually become scrollable when necessary
 *  to fit the height of the screen.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class StackedScrollGroup : public StackedGroupT<QScrollArea>
{
    public:
        explicit StackedScrollGroup(QDialog*, QObject* tabParent);
        virtual QSize minimumSizeHint() const;
        int           heightReduction() const { return mHeightReduction; }
        QSize         adjustSize(bool force = false);
        void          setSized()              { mSized = true; }
        bool          sized() const           { return mSized; }

    private:
        QSize         maxMinimumSizeHint() const;

        QDialog* mDialog;
        int      mMinHeight;
        int      mHeightReduction;
        bool     mSized;
};

#endif // STACKEDWIDGETS_H

// vim: et sw=4:
