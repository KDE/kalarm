/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "buttongroup.h"

#include <QAbstractButton>


ButtonGroup::ButtonGroup(QObject* parent)
    : QButtonGroup(parent)
{
}

/******************************************************************************
* Inserts a button into the group.
*/
void ButtonGroup::insertButton(QAbstractButton* button, int id)
{
    QButtonGroup::addButton(button, id);
    connect(button, &QAbstractButton::toggled, this, &ButtonGroup::slotButtonToggled);
}

/******************************************************************************
* Checks the button with the specified ID.
*/
void ButtonGroup::setButton(int id)
{
    QAbstractButton* btn = button(id);
    if (btn)
        btn->setChecked(true);
}

/******************************************************************************
* Called when one of the member buttons is toggled.
*/
void ButtonGroup::slotButtonToggled(bool)
{
    if (checkedButton() != mSelectedButton)
    {
        QAbstractButton* old = mSelectedButton;
        mSelectedButton = checkedButton();
        Q_EMIT selectionChanged(old, mSelectedButton);
    }
}

#include "moc_buttongroup.cpp"

// vim: et sw=4:
