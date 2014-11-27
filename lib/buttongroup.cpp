/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal
 *  Program:  kalarm
 *  Copyright © 2002,2004,2005,2008 by David Jarvie <djarvie@kde.org>
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

#include "buttongroup.h"
#include <QAbstractButton>


ButtonGroup::ButtonGroup(QObject* parent)
    : QButtonGroup(parent)
{
    connect(this, SIGNAL(buttonClicked(QAbstractButton*)), SIGNAL(buttonSet(QAbstractButton*)));
}

/******************************************************************************
 * Inserts a button into the group.
 */
void ButtonGroup::addButton(QAbstractButton* button)
{
    QButtonGroup::addButton(button);
    connect(button, SIGNAL(toggled(bool)), SLOT(slotButtonToggled(bool)));
}

/******************************************************************************
 * Inserts a button into the group.
 */
void ButtonGroup::addButton(QAbstractButton* button, int id)
{
    addButton(button);
    mIds[id] = button;
}

/******************************************************************************
 * Returns the ID of the specified button.
 * Reply = -1 if not found.
 */
int ButtonGroup::id(QAbstractButton* button) const
{
    for (QMap<int, QAbstractButton*>::ConstIterator it = mIds.constBegin();  it != mIds.constEnd();  ++it)
        if (it.value() == button)
            return it.key();
    return -1;
}

/******************************************************************************
 * Returns the button with the specified ID.
 */
QAbstractButton* ButtonGroup::find(int id) const
{
    QMap<int, QAbstractButton*>::ConstIterator it = mIds.find(id);
    if (it == mIds.constEnd())
        return 0;
    return it.value();
}

/******************************************************************************
 * Returns the ID of the currently selected button.
 */
int ButtonGroup::selectedId() const
{
    return id(checkedButton());
}

/******************************************************************************
 * Returns the ID of the currently selected button.
 */
void ButtonGroup::setButton(int id)
{
    QAbstractButton* button = find(id);
    if (button)
        button->setChecked(true);
}

/******************************************************************************
 * Called when one of the member buttons is toggled.
 */
void ButtonGroup::slotButtonToggled(bool)
{
    emit buttonSet(checkedButton());
}

// vim: et sw=4:
