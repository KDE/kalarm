/*
 *  stackedwidgets.h  -  classes implementing stacked widgets
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QScrollArea>
#include <QStackedWidget>
class QDialog;

/**
 *  A QStackedWidget, whose size hint is that of the largest widget in the stack.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class StackedWidget : public QStackedWidget
{
public:
    /** Constructor.
     *  @param parent The parent object of this widget.
     */
    explicit StackedWidget(QWidget* parent = nullptr)
          : QStackedWidget(parent)  {}
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

template <class T> class StackedGroupT;

/**
 *  A widget contained in a stack, whose minimum size hint is that of the largest
 *  widget in the stack. This class works together with StackedGroup.
 *
 *  Do not use this class for widgets contained in a QStackedWidget or
 *  StackedWidget.
 *
 *  @tparam T  The base class for this widget. Must be derived from QWidget.

 *  @author David Jarvie <djarvie@kde.org>
 */
template <class T>
class StackedGroupWidgetT : public T
{
public:
    /** Constructor.
     *  @param group  The stack group to insert this widget into.
     *  @param parent The parent object of this widget.
     */
    explicit StackedGroupWidgetT(StackedGroupT<T>* group, QWidget* parent = nullptr)
          : T(parent),
            mGroup(group)
    {
        mGroup->addWidget(this);
    }
    ~StackedGroupWidgetT() override  { mGroup->removeWidget(this); }
    QSize sizeHint() const         override { return minimumSizeHint(); }
    QSize minimumSizeHint() const  override { return mGroup->minimumSizeHint(); }

private:
    StackedGroupT<T>* mGroup;
};

/**
 *  A group of stacked widgets whose minimum size hints are all equal to the
 *  largest widget's minimum size hint. Use this alongside the widgets' container,
 *  e.g. QTabWidget.
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
    /** Constructor.
     *  @param container  The parent widget. This should be set to the container
     *                    for the stacked widgets, which will ensure that this
     *                    object is deleted when the container is deleted.
     */
    explicit StackedGroupT(QWidget* container) : QObject(container) {}

    void  addWidget(StackedGroupWidgetT<T>* w)     { mWidgets += w; }
    void  removeWidget(StackedGroupWidgetT<T>* w)  { mWidgets.removeAll(w); }
    virtual QSize minimumSizeHint() const;

protected:
  QList<StackedGroupWidgetT<T> *> mWidgets;
};

template <class T>
QSize StackedGroupT<T>::minimumSizeHint() const
{
    QSize sz;
    for (const auto& w : mWidgets)
        sz = sz.expandedTo(w->T::minimumSizeHint());
    return sz;
}

/** A non-scrollable stacked QWidget. */
using StackedGroupWidget = StackedGroupWidgetT<QWidget>;
/** A group of non-scrollable stacked widgets which are each derived from QWidget. */
using StackedGroup = StackedGroupT<QWidget>;


class StackedScrollGroup;

/**
 *  A stacked QScrollArea widget, which becomes scrollable when necessary to
 *  fit the height of the screen.
 *
 *  Do not use this class for widgets contained in a QStackedWidget or
 *  StackedWidget.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class StackedScrollWidget : public StackedGroupWidgetT<QScrollArea>
{
public:
    explicit StackedScrollWidget(StackedScrollGroup* group, QWidget* parent = nullptr);
    QWidget* widget() const  { return viewport()->findChild<QWidget*>(); }
};

/**
 *  A group of stacked StackedScrollWidget widgets, which individually become
 *  scrollable when necessary to fit the height of the screen.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class StackedScrollGroup : public StackedGroupT<QScrollArea>
{
public:
    /** Constructor.
     *  @param dialog     The dialog which contains the widgets.
     *  @param container  The parent widget. This should be set to the container
     *                    for the stacked widgets, which will ensure that this
     *                    object is deleted when the container is deleted.
     */
    StackedScrollGroup(QDialog* dialog, QWidget* container);

    /** Return the minimum size for the tabs, constrained if necessary to a height
     *  that fits the dialog into the screen. The dialog height must have been
     *  previously evaluated by calling adjustSize().
     */
    QSize minimumSizeHint() const override;

    /** Return the reduction in dialog height which adjustSize() performed in
     *  order to fit the dialog to the desktop.
     */
    int heightReduction() const  { return mHeightReduction; }

    /** Set the minimum height for the dialog, so as to accommodate the tabs,
     *  but constrained to fit the desktop. If necessary, the tab contents are
     *  made scrollable.
     *  @param force  If false, this method will only evaluate and set the
     *                minimum dialog height the first time it is called. Set
     *                true to force re-evaluation.
     *  @return The minimum size for the dialog, or null if the size was not evaluated.
     */
    QSize adjustSize(bool force = false);

    /** Prevent adjustSize(false) from evaluating or setting the dialog height. */
    void setSized()              { mSized = true; }

    /** Return whether the dialog size has already been set, e.g. by adjustSize(). */
    bool sized() const           { return mSized; }

private:
    QSize    maxMinimumSizeHint() const;

    QDialog* mDialog;
    int      mMinHeight {-1};
    int      mHeightReduction {0};
    bool     mSized {false};
};

// vim: et sw=4:
