/*
 *  stackedwidgets.h  -  group of stacked widgets
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008, 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QVector>
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
     *  @param group The stack group to insert this widget into.
     */
    explicit StackedWidgetT(StackedGroupT<T>* group, QWidget* parent = nullptr)
          : T(parent),
            mGroup(group)
    {
        mGroup->addWidget(this);
    }
    ~StackedWidgetT() override  { mGroup->removeWidget(this); }
    QSize sizeHint() const         override { return minimumSizeHint(); }
    QSize minimumSizeHint() const  override { return mGroup->minimumSizeHint(); }

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
    explicit StackedGroupT(QObject* parent = nullptr) : QObject(parent) {}
    void  addWidget(StackedWidgetT<T>* w)     { mWidgets += w; }
    void  removeWidget(StackedWidgetT<T>* w)  { mWidgets.removeAll(w); }
    virtual QSize minimumSizeHint() const;

protected:
    QVector<StackedWidgetT<T>*> mWidgets;
};

template <class T>
QSize StackedGroupT<T>::minimumSizeHint() const
{
    QSize sz;
    for (const auto& w : mWidgets)
        sz = sz.expandedTo(w->T::minimumSizeHint());
    return sz;
}

/** A non-scrollable stacked widget. */
using StackedWidget = StackedWidgetT<QWidget>;
/** A group of non-scrollable stacked widgets. */
using StackedGroup = StackedGroupT<QWidget>;


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
    explicit StackedScrollWidget(StackedScrollGroup* group, QWidget* parent = nullptr);
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
    StackedScrollGroup(QDialog*, QObject* tabParent);
    QSize    minimumSizeHint() const override;
    int      heightReduction() const { return mHeightReduction; }
    QSize    adjustSize(bool force = false);
    void     setSized()              { mSized = true; }
    bool     sized() const           { return mSized; }

private:
    QSize    maxMinimumSizeHint() const;

    QDialog* mDialog;
    int      mMinHeight {-1};
    int      mHeightReduction {0};
    bool     mSized {false};
};


// vim: et sw=4:
