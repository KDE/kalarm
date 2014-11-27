/*
 *  checkbox.cpp  -  check box with read-only option
 *  Program:  kalarm
 *  Copyright © 2002,2003,2005 by David Jarvie <djarvie@kde.org>
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

#include "checkbox.h"
#include <QMouseEvent>
#include <QKeyEvent>


CheckBox::CheckBox(QWidget* parent)
    : QCheckBox(parent),
      mFocusPolicy(focusPolicy()),
      mFocusWidget(0),
      mReadOnly(false)
{ }

CheckBox::CheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent),
      mFocusPolicy(focusPolicy()),
      mFocusWidget(0),
      mReadOnly(false)
{ }

/******************************************************************************
*  Set the read-only status. If read-only, the checkbox can be toggled by the
*  application, but not by the user.
*/
void CheckBox::setReadOnly(bool ro)
{
    if ((int)ro != (int)mReadOnly)
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

// vim: et sw=4:
