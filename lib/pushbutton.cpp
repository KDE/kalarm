/*
 *  pushbutton.cpp  -  push button with read-only option
 *  Program:  kalarm
 *  Copyright © 2002,2005,2009 by David Jarvie <djarvie@kde.org>
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

#include "pushbutton.h"

#include <QMouseEvent>
#include <QKeyEvent>


PushButton::PushButton(QWidget* parent)
    : KPushButton(parent),
      mFocusPolicy(focusPolicy()),
      mReadOnly(false),
      mNoHighlight(false)
{ }

PushButton::PushButton(const KGuiItem& text, QWidget* parent)
    : KPushButton(text, parent),
      mFocusPolicy(focusPolicy()),
      mReadOnly(false),
      mNoHighlight(false)
{ }

PushButton::PushButton(const QString& text, QWidget* parent)
    : KPushButton(text, parent),
      mFocusPolicy(focusPolicy()),
      mReadOnly(false),
      mNoHighlight(false)
{ }

PushButton::PushButton(const KIcon& icon, const QString& text, QWidget* parent)
    : KPushButton(icon, text, parent),
      mFocusPolicy(focusPolicy()),
      mReadOnly(false),
      mNoHighlight(false)
{ }

void PushButton::setReadOnly(bool ro, bool noHighlight)
{
    mNoHighlight = noHighlight;
    if ((int)ro != (int)mReadOnly)
    {
        mReadOnly = ro;
        setFocusPolicy(ro ? Qt::NoFocus : mFocusPolicy);
        if (ro)
            clearFocus();
    }
}

void PushButton::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    KPushButton::mousePressEvent(e);
}

void PushButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    KPushButton::mouseReleaseEvent(e);
}

void PushButton::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KPushButton::mouseMoveEvent(e);
}

void PushButton::keyPressEvent(QKeyEvent* e)
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
    KPushButton::keyPressEvent(e);
}

void PushButton::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        KPushButton::keyReleaseEvent(e);
}

bool PushButton::event(QEvent* e)
{
    if (mReadOnly  &&  mNoHighlight)
    {
        // Don't highlight the button on mouse hover
        if (e->type() == QEvent::HoverEnter)
            return true;
    }
    return KPushButton::event(e);
}
#include "moc_pushbutton.cpp"
// vim: et sw=4:
