/*
 *  slider.cpp  -  slider control with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "slider.h"

#include <QMouseEvent>


Slider::Slider(QWidget* parent)
    : QSlider(parent)
{ }

Slider::Slider(Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent)
{ }

Slider::Slider(int minval, int maxval, int pageStep, Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent)
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
