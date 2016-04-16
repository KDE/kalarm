/*
 *  colourbutton.cpp  -  colour selection button
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

#include "kalarm.h"
#include "colourbutton.h"

#include <QMouseEvent>
#include <QKeyEvent>


ColourButton::ColourButton(QWidget* parent)
    : KColorButton(parent),
      mReadOnly(false)
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
