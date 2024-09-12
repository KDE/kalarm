/*
 *  buttongroup.h  -  QButtonGroup with an extra signal
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QButtonGroup>
//class QAbstractButton;


/**
 *  @short A QButtonGroup with signal on new selection.
 *
 *  The ButtonGroup class provides an enhanced version of the QButtonGroup class.
 *
 *  It emits an additional signal, selectionChanged(QAbstractButton*,QAbstractButton*),
 *  whenever the checked button changes. This allows just a single signal to be
 *  processed instead of two at a time (first the old selection being unchecked,
 *  and then the new selection being checked).
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ButtonGroup : public QButtonGroup
{
    Q_OBJECT
public:
    /** Constructor.
     *  @param parent The parent object of this widget
     */
    explicit ButtonGroup(QObject* parent = nullptr);

    /** Adds a button with a specified ID to the group.
     *  @param button The button to insert
     *  @param id     Button ID
     */
    void insertButton(QAbstractButton* button, int id = -1);

    void addButton(QAbstractButton* button, int id = -1) = delete;  // hide QButtonGroup method

    /** Checks the button with the specified ID.
     *  @param id Button ID
     */
    void setButton(int id);

Q_SIGNALS:
    /** Signal emitted when the selected button in the group changes.
     *  @param oldButton The button which was previously selected
     *  @param newButton The button which is now selected
     */
    void selectionChanged(QAbstractButton* oldButton, QAbstractButton* newButton);

private Q_SLOTS:
    void slotButtonToggled(bool);

private:
    QAbstractButton* mSelectedButton {nullptr};
};

// vim: et sw=4:
