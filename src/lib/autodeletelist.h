/*
 *  autodeletelist.h  -  pointer list with auto-delete on destruction
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>


/**
 *  A list of pointers which are auto-deleted when the list is deleted.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
template <class T>
class AutoDeleteList : public QList<T*>
{
public:
    AutoDeleteList() : QList<T*>() {}
    ~AutoDeleteList()
    {
        // Remove from list first before deleting the pointer, in
        // case the pointer's destructor removes it from the list.
        while (!this->isEmpty())
            delete this->takeFirst();
    }

    // Prevent copying since that would create two owners of the pointers
    AutoDeleteList(const AutoDeleteList&) = delete;
    AutoDeleteList& operator=(const AutoDeleteList&) = delete;
};


// vim: et sw=4:
