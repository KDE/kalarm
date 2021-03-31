/*
 *  buttongroup.h  -  QButtonGroup with an extra signal, and button IDs
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002, 2004, 2005, 2008 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QButtonGroup>
#include <QMap>
class QAbstractButton;


/**
 *  @short A QButtonGroup with signal on new selection, and button IDs.
 *
 *  The ButtonGroup class provides an enhanced version of the QButtonGroup class.
 *
 *  It emits an additional signal, buttonSet(QAbstractButton*), whenever any of its
 *  buttons changes state, for whatever reason, including programmatic control. (The
 *  QButtonGroup class only emits signals when buttons are clicked on by the user.)
 *
 *  It allows buttons to have associated ID numbers, which can be used to access the
 *  buttons.
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
        /** Adds a button to the group.
         *  The button is not given an ID.
         *  This overrides the addButton() method of QButtonGroup.
         *  @param button The button to insert
         */
        void         addButton(QAbstractButton* button);
        /** Adds a button with a specified ID to the group.
         *  @param button The button to insert
         *  @param id     Button ID
         */
        void         addButton(QAbstractButton* button, int id);
        /** Returns the identifier of the specified button.
         *  @return ID, or -1 if the button was not found
         */
        int          id(QAbstractButton* button) const;
        /** Returns the button with the specified identifier @p id.
         *  @return button, or 0 if the button was not found
         */
        QAbstractButton* find(int id) const;
        /** Returns the id of the selected button.
         *  @return button if exactly one is selected, or -1 otherwise
         */
        int          selectedId() const;
        /** Checks the button with the specified ID.
         *  @param id Button ID
         */
        void         setButton(int id);
    Q_SIGNALS:
        /** Signal emitted whenever any button in the group changes state,
         *  for whatever reason.
         *  @param button The button which is now selected
         */
        void         buttonSet(QAbstractButton* button);

    private Q_SLOTS:
        void         slotButtonToggled(bool);

    private:
        QMap<int, QAbstractButton*> mIds;
};


// vim: et sw=4:
