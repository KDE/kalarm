/*
 *  combobox.cpp  -  combo box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002, 2005, 2007 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "combobox.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QLineEdit>

ComboBox::ComboBox(QWidget* parent)
    : KComboBox(parent)
{ }

void ComboBox::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        if (lineEdit())
            lineEdit()->setReadOnly(ro);
    }
}

void ComboBox::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    KComboBox::mousePressEvent(e);
}

void ComboBox::mouseReleaseEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KComboBox::mouseReleaseEvent(e);
}

void ComboBox::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KComboBox::mouseMoveEvent(e);
}

void ComboBox::keyPressEvent(QKeyEvent* e)
{
    if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
        KComboBox::keyPressEvent(e);
}

void ComboBox::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        KComboBox::keyReleaseEvent(e);
}

#include "moc_combobox.cpp"

// vim: et sw=4:
