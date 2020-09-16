/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "buttongroup.h"

#include <QAbstractButton>


ButtonGroup::ButtonGroup(QObject* parent)
    : QButtonGroup(parent)
{
    connect(this, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &ButtonGroup::buttonSet);
}

/******************************************************************************
 * Inserts a button into the group.
 */
void ButtonGroup::addButton(QAbstractButton* button)
{
    QButtonGroup::addButton(button);
    connect(button, &QAbstractButton::toggled, this, &ButtonGroup::slotButtonToggled);
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
        return nullptr;
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
    Q_EMIT buttonSet(checkedButton());
}

// vim: et sw=4:
