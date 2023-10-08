/*
 *  autoqpointer.h  -  QPointer which on destruction deletes object
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009, 2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QPointer>


/**
 *  Stores a pointer to a QObject, and ensures that when either it or the QObject is deleted,
 *  the 'right thing' happens. It is like a QScopedPointer holding a QPointer. It ensures:
 *  - When the QObject pointed to is deleted, the stored pointer is set to null;
 *  - When the AutoQPointer is deleted, its QObject is also deleted.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
template <class T>
class AutoQPointer : public QPointer<T>
{
public:
    AutoQPointer() = default;
    AutoQPointer(T* p) : QPointer<T>(p) {}                    //cppcheck-suppress noExplicitConstructor; Allow implicit conversion
    AutoQPointer(const QPointer<T>& p) : QPointer<T>(p) {}    //cppcheck-suppress noExplicitConstructor; Allow implicit conversion
    ~AutoQPointer()  { delete this->data(); }
    AutoQPointer<T>& operator=(T* p) { QPointer<T>::operator=(p); return *this; }

    AutoQPointer(const AutoQPointer<T>&) = delete;
    AutoQPointer<T>& operator=(const AutoQPointer<T>&) = delete;
};

// vim: et sw=4:
