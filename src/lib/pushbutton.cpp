/*
 *  pushbutton.cpp  -  push button with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pushbutton.h"

#include <KGuiItem>
#include <QIcon>
#include <QMouseEvent>
#include <QKeyEvent>

PushButton::PushButton(QWidget* parent)
    : QPushButton(parent)
    , mFocusPolicy(focusPolicy())
{ }

PushButton::PushButton(const KGuiItem& gui, QWidget* parent)
    : QPushButton(parent)
    , mFocusPolicy(focusPolicy())
{
     KGuiItem::assign(this, gui);
}

PushButton::PushButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
    , mFocusPolicy(focusPolicy())
{ }

PushButton::PushButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QPushButton(icon, text, parent)
    , mFocusPolicy(focusPolicy())
{ }

void PushButton::setReadOnly(bool ro, bool noHighlight)
{
    mNoHighlight = noHighlight;
    if (ro != mReadOnly)
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
    QPushButton::mousePressEvent(e);
}

void PushButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QPushButton::mouseReleaseEvent(e);
}

void PushButton::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QPushButton::mouseMoveEvent(e);
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
    QPushButton::keyPressEvent(e);
}

void PushButton::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        QPushButton::keyReleaseEvent(e);
}

bool PushButton::event(QEvent* e)
{
    if (mReadOnly  &&  mNoHighlight)
    {
        // Don't highlight the button on mouse hover
        if (e->type() == QEvent::HoverEnter)
            return true;
    }
    return QPushButton::event(e);
}

// vim: et sw=4:

#include "moc_pushbutton.cpp"
