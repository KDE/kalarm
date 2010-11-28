/*
 *  kcalcore_constptr.h  -  ConstPtr types for kcalcore objects
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

#ifndef KCALCORE_CONSTPTR_H
#define KCALCORE_CONSTPTR_H

#ifdef USE_AKONADI

#include <QSharedPointer>
#include <kcalcore/event.h>
#include <kcalcore/alarm.h>
#include <kcalcore/person.h>
#if 0
namespace KCalCore
{
    class Event;
    class Alarm;
    class Person;
}
#endif

namespace KCalCore
{

#if 0
template<class T> class ConstPtrT
{
    public:
        ConstPtrT() {}
        ConstPtrT(const T* e) : cd(e), isConst(true) {}    // implicit conversion
        ConstPtrT(T* e) : d(e), isConst(false) {}    // implicit conversion
        ConstPtrT& operator=(const T* e)  { cd = e; isConst = true; return *this; }
        ConstPtrT& operator=(T* e)        { d = e; isConst = false; return *this; }
        operator bool() const        { return d; }
        bool operator!() const       { return !d; }
        const T* operator->() const  { return d; }
        T*       operator->()        { return isConst ? d : d; }
        const T& operator*() const   { return *d; }
        T&       operator*()         { return isConst ? *cd : *d; }
        operator const T*() const    { return d; }
        operator T*()                { return isConst ? cd : d; }
    private:
        union {
            T*       d;
            const T* cd;
        };
        bool isConst;
};
#endif

// KCalCore::Event::Ptr is equivalent to KCal::Event*
typedef QSharedPointer<const Event>  ConstEventPtr;
template <class T>
inline T constCast(const ConstEventPtr& event)
{
    return T(const_cast<Event*>(event.data()));
}

// KCalCore::Alarm::Ptr is equivalent to KCal::Alarm*
typedef QSharedPointer<const KCalCore::Alarm>  ConstAlarmPtr;
template <class T>
inline T constCast(const ConstAlarmPtr& alarm)
{
    return T(const_cast<Alarm*>(alarm.data()));
}

// KCalCore::Person::Ptr is equivalent to KCal::Person*
typedef QSharedPointer<const KCalCore::Person>  ConstPersonPtr;

} // namespace KCalCore

#endif // USE_AKONADI
#endif // KCALCORE_CONSTPTR_H

// vim: et sw=4:
