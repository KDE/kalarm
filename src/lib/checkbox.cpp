/*
 *  checkbox.cpp  -  check box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "checkbox.h"

#include "kalarm_debug.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyleOptionButton>


CheckBox::CheckBox(QWidget* parent)
    : QCheckBox(parent)
    , mFocusPolicy(focusPolicy())
{ }

CheckBox::CheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent)
    , mFocusPolicy(focusPolicy())
{ }

/******************************************************************************
*  Set the read-only status. If read-only, the checkbox can be toggled by the
*  application, but not by the user.
*/
void CheckBox::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        setFocusPolicy(ro ? Qt::NoFocus : mFocusPolicy);
        if (ro)
            clearFocus();
    }
}

/******************************************************************************
*  Specify a widget to receive focus when the checkbox is clicked on.
*/
void CheckBox::setFocusWidget(QWidget* w, bool enable)
{
    mFocusWidget = w;
    mFocusWidgetEnable = enable;
    if (w)
        connect(this, &CheckBox::clicked, this, &CheckBox::slotClicked);
    else
        disconnect(this, &CheckBox::clicked, this, &CheckBox::slotClicked);
}

/******************************************************************************
* Return the indentation from the left of a checkbox widget to the start of the
* checkbox text.
*/
int CheckBox::textIndent(QWidget* widget)
{
    QStyleOptionButton opt;
    QStyle* style;
    if (qobject_cast<QCheckBox*>(widget))
    {
        opt.initFrom(widget);
        style = widget->style();
    }
    else
    {
        QCheckBox cb(widget);
        opt.initFrom(&cb);
        style = cb.style();
    }
    const QRect contents = style->subElementRect(QStyle::SE_CheckBoxContents, &opt);
    return (widget->layoutDirection() == Qt::LeftToRight) ? contents.left() : opt.rect.right() - contents.right();
}

/******************************************************************************
*  Called when the checkbox is clicked.
*  If it is now checked, focus is transferred to any specified focus widget.
*/
void CheckBox::slotClicked()
{
    if (mFocusWidget  &&  isChecked())
    {
        if (mFocusWidgetEnable)
            mFocusWidget->setEnabled(true);
        mFocusWidget->setFocus();
    }
}

/******************************************************************************
*  Event handlers to intercept events if in read-only mode.
*  Any events which could change the checkbox state are discarded.
*/
void CheckBox::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QCheckBox::mousePressEvent(e);
}

void CheckBox::mouseReleaseEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QCheckBox::mouseReleaseEvent(e);
}

void CheckBox::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QCheckBox::mouseMoveEvent(e);
}

void CheckBox::keyPressEvent(QKeyEvent* e)
{
    if (mReadOnly)
    {
        switch (e->key())
        {
            case Qt::Key_Up:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Down:
                // Process keys which shift the focus
                break;
            default:
                return;
        }
    }
    QCheckBox::keyPressEvent(e);
}

void CheckBox::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        QCheckBox::keyReleaseEvent(e);
}

#include "moc_checkbox.cpp"

// vim: et sw=4:
