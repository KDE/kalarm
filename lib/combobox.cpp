/*
 *  combobox.cpp  -  combo box with read-only option
 *  Program:  kalarm
 *  Copyright © 2002,2005,2007 by David Jarvie <djarvie@kde.org>
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

#include "combobox.h"

#include <QLineEdit>
#include <QMouseEvent>
#include <QKeyEvent>


ComboBox::ComboBox(QWidget* parent)
    : KComboBox(parent),
      mReadOnly(false)
{ }

void ComboBox::setReadOnly(bool ro)
{
    if ((int)ro != (int)mReadOnly)
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
