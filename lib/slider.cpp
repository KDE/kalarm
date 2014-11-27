/*
 *  slider.cpp  -  slider control with read-only option
 *  Program:  kalarm
 *  Copyright © 2004-2006 by David Jarvie <djarvie@kde.org>
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

#include "slider.h"
#include <QMouseEvent>


Slider::Slider(QWidget* parent)
    : QSlider(parent),
      mReadOnly(false)
{ }

Slider::Slider(Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent),
      mReadOnly(false)
{ }

Slider::Slider(int minval, int maxval, int pageStep, Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent),
      mReadOnly(false)
{ 
    setRange(minval, maxval);
    setPageStep(pageStep);
}

/******************************************************************************
*  Set the read-only status. If read-only, the slider can be moved by the
*  application, but not by the user.
*/
void Slider::setReadOnly(bool ro)
{
    mReadOnly = ro;
}

/******************************************************************************
*  Event handlers to intercept events if in read-only mode.
*  Any events which could change the slider value are discarded.
*/
void Slider::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QSlider::mousePressEvent(e);
}

void Slider::mouseReleaseEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QSlider::mouseReleaseEvent(e);
}

void Slider::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QSlider::mouseMoveEvent(e);
}

void Slider::keyPressEvent(QKeyEvent* e)
{
    if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
        QSlider::keyPressEvent(e);
}

void Slider::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        QSlider::keyReleaseEvent(e);
}

// vim: et sw=4:
