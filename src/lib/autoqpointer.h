/*
 *  autoqpointer.h  -  QPointer which on destruction deletes object
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AUTOQPOINTER_H
#define AUTOQPOINTER_H

#include <QPointer>


/**
 *  A QPointer which when destructed, deletes the object it points to.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
template <class T>
class AutoQPointer : public QPointer<T>
{
    public:
        AutoQPointer() : QPointer<T>() {}
        AutoQPointer(T* p) : QPointer<T>(p) {}
        AutoQPointer(const QPointer<T>& p) : QPointer<T>(p) {}
        ~AutoQPointer()  { delete this->data(); }
        AutoQPointer<T>& operator=(const AutoQPointer<T>& p) { QPointer<T>::operator=(p); return *this; }
        AutoQPointer<T>& operator=(T* p) { QPointer<T>::operator=(p); return *this; }
};

#endif // AUTOQPOINTER_H

// vim: et sw=4:
