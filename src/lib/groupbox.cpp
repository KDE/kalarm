/*
 *  groupbox.cpp  -  checkable group box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011, 2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "groupbox.h"

#include <QMouseEvent>
#include <QKeyEvent>


GroupBox::GroupBox(QWidget* parent)
    : QGroupBox(parent)
{ }

GroupBox::GroupBox(const QString& title, QWidget* parent)
    : QGroupBox(title, parent)
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

#include "moc_groupbox.cpp"

// vim: et sw=4:
