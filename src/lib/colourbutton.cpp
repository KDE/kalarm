/*
 *  colourbutton.cpp  -  colour selection button
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "colourbutton.h"

#include <QMouseEvent>
#include <QKeyEvent>


ColourButton::ColourButton(QWidget* parent)
    : KColorButton(parent)
{
}

void ColourButton::setReadOnly(bool ro)
{
    mReadOnly = ro;
}

void ColourButton::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    KColorButton::mousePressEvent(e);
}

void ColourButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KColorButton::mouseReleaseEvent(e);
}

void ColourButton::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KColorButton::mouseMoveEvent(e);
}

void ColourButton::keyPressEvent(QKeyEvent* e)
{
    if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
        KColorButton::keyPressEvent(e);
}

void ColourButton::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        KColorButton::keyReleaseEvent(e);
}

// vim: et sw=4:

#include "moc_colourbutton.cpp"
