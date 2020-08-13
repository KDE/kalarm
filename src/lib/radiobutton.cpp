/*
 *  radiobutton.cpp  -  radio button with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "radiobutton.h"

#include <QMouseEvent>
#include <QKeyEvent>


RadioButton::RadioButton(QWidget* parent)
    : QRadioButton(parent)
    , mFocusPolicy(focusPolicy())
{ }

RadioButton::RadioButton(const QString& text, QWidget* parent)
    : QRadioButton(text, parent)
    , mFocusPolicy(focusPolicy())
{ }

/******************************************************************************
*  Set the read-only status. If read-only, the button can be toggled by the
*  application, but not by the user.
*/
void RadioButton::setReadOnly(bool ro)
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
*  Specify a widget to receive focus when the button is clicked on.
*/
void RadioButton::setFocusWidget(QWidget* w, bool enable)
{
    mFocusWidget = w;
    mFocusWidgetEnable = enable;
    if (w)
        connect(this, &RadioButton::clicked, this, &RadioButton::slotClicked);
    else
        disconnect(this, &RadioButton::clicked, this, &RadioButton::slotClicked);
}

/******************************************************************************
*  Called when the button is clicked.
*  If it is now checked, focus is transferred to any specified focus widget.
*/
void RadioButton::slotClicked()
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
*  Any events which could change the button state are discarded.
*/
void RadioButton::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QRadioButton::mousePressEvent(e);
}

void RadioButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QRadioButton::mouseReleaseEvent(e);
}

void RadioButton::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QRadioButton::mouseMoveEvent(e);
}

void RadioButton::keyPressEvent(QKeyEvent* e)
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
            case Qt::Key_Escape:
                break;
            default:
                return;
        }
    }
    QRadioButton::keyPressEvent(e);
}

void RadioButton::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        QRadioButton::keyReleaseEvent(e);
}

// vim: et sw=4:
