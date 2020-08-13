/*
 *  stackedwidgets.cpp  -  group of stacked widgets
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stackedwidgets.h"

#include "desktop.h"
#include "kalarm_debug.h"

#include <QDialog>
#include <QStyle>


StackedScrollWidget::StackedScrollWidget(StackedScrollGroup* group, QWidget* parent)
    : StackedWidgetT<QScrollArea>(group, parent)
{
    setFrameStyle(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setWidgetResizable(true);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

StackedScrollGroup::StackedScrollGroup(QDialog* dlg, QObject* tabParent)
    : StackedGroupT<QScrollArea>(tabParent),
      mDialog(dlg),
      mMinHeight(-1),
      mHeightReduction(0),
      mSized(false)
{
}

/******************************************************************************
* Return the minimum size for the tab, adjusted if necessary to a height that
* fits the screen.
* In order to make the QStackedWidget containing the tabs take the correct
* size, the value returned is actually the minimum size of the largest tab.
* Otherwise, only the currently visible tab would be taken into account with
* the result that the dialog would initially be displayed too small.
*/
QSize StackedScrollGroup::minimumSizeHint() const
{
    QSize s = maxMinimumSizeHint();
    if (!s.isEmpty()  &&  mMinHeight > 0  &&  mMinHeight < s.height())
        return QSize(s.width() + mWidgets[0]->style()->pixelMetric(QStyle::PM_ScrollBarExtent), mMinHeight);
    return s;
}

/******************************************************************************
* Return the maximum minimum size for any instance.
*/
QSize StackedScrollGroup::maxMinimumSizeHint() const
{
    QSize sz;
    for (int i = 0, count = mWidgets.count();  i < count;  ++i)
    {
        QWidget* w = static_cast<StackedScrollWidget*>(mWidgets[i])->widget();
        if (!w)
            return QSize();
        QSize s = w->minimumSizeHint();
        if (!s.isValid())
            return QSize();
        sz = sz.expandedTo(s);
    }
    return sz;
}

/******************************************************************************
* Return the minimum size for the dialog.
* If the minimum size would be too high to fit the desktop, the tab contents
* are made scrollable.
*/
QSize StackedScrollGroup::adjustSize(bool force)
{
    if (force)
        mSized = false;
    if (mSized)
        return QSize();

    // Cancel any previous minimum height and set the height of the
    // scroll widget contents widgets.
    mMinHeight = -1;
    mHeightReduction = 0;
    QSize s = maxMinimumSizeHint();
    if (s.isEmpty())
        return QSize();
    int maxTabHeight = s.height();
    for (int i = 0, count = mWidgets.count();  i < count;  ++i)
    {
        mWidgets[i]->setMinimumHeight(maxTabHeight);
        QWidget* w = static_cast<StackedScrollWidget*>(mWidgets[i])->widget();
        if (w)
            w->resize(s);
    }
    for (QWidget* w = mWidgets[0]->parentWidget();  w && w != mDialog;  w = w->parentWidget())
    {
        w->setMinimumHeight(0);
        w->adjustSize();
    }
    mDialog->setMinimumHeight(0);

    int decoration = mDialog->frameGeometry().height() - mDialog->geometry().height();
    if (!decoration)
    {
        // On X11 at least, the window decoration height may not be
        // available, so use a guess of 25 pixels.
        decoration = 25;
    }
    int desk = Desktop::workArea().height();
    // There is no stored size, or the deferral group is visible.
    // Allow the tab contents to be scrolled vertically if that is necessary
    // to avoid the dialog exceeding the screen height.
    QSize dlgsize = mDialog->QDialog::minimumSizeHint();
    int y = dlgsize.height() + decoration - desk;
    if (y > 0)
    {
        mHeightReduction = y;
        mMinHeight = maxTabHeight - y;
        qCDebug(KALARM_LOG) << "StackedScrollGroup::adjustSize: Scrolling: max tab height=" << maxTabHeight << ", reduction=" << mHeightReduction << "-> min tab height=" << mMinHeight;
        if (mMinHeight > 0)
        {
            for (int i = 0, count = mWidgets.count();  i < count;  ++i)
            {
                mWidgets[i]->setMinimumHeight(mMinHeight);
                mWidgets[i]->resize(QSize(mWidgets[i]->width(), mMinHeight));
            }
        }
        mSized = true;
        QSize s = mWidgets[0]->parentWidget()->sizeHint();
        if (s.height() < mMinHeight)
            s.setHeight(mMinHeight);
        mWidgets[0]->parentWidget()->resize(s);
        for (QWidget* w = mWidgets[0]->parentWidget();  w && w != mDialog;  w = w->parentWidget())
            w->setMinimumHeight(qMin(w->minimumSizeHint().height(), w->sizeHint().height()));
        dlgsize.setHeight(dlgsize.height() - mHeightReduction);
        s = mDialog->QDialog::minimumSizeHint();
        if (s.height() > dlgsize.height())
            dlgsize.setHeight(s.height());
        mDialog->setMinimumHeight(dlgsize.height());
    }
    mSized = true;
    mDialog->resize(dlgsize);
    return s;
}

// vim: et sw=4:
