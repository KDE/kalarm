/*
 *  groupbox.cpp  -  checkable group box with read-only option
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#include "groupbox.h"

#include <QMouseEvent>
#include <QKeyEvent>


GroupBox::GroupBox(QWidget* parent)
    : QGroupBox(parent),
      mReadOnly(false)
{ }

GroupBox::GroupBox(const QString& title, QWidget* parent)
    : QGroupBox(title, parent),
      mReadOnly(false)
{ }

void GroupBox::setReadOnly(bool ro)
{
    mReadOnly = ro;
}

void GroupBox::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QGroupBox::mousePressEvent(e);
}

void GroupBox::mouseReleaseEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QGroupBox::mouseReleaseEvent(e);
}

void GroupBox::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QGroupBox::mouseMoveEvent(e);
}

void GroupBox::keyPressEvent(QKeyEvent* e)
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
    QGroupBox::keyPressEvent(e);
}

void GroupBox::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        QGroupBox::keyReleaseEvent(e);
}

// vim: et sw=4:
