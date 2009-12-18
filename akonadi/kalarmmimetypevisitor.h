/*
 *  kalarmeventvisitor.h  -  Checks whether Incidences are Events
 *  Program:  kalarm
 *  Copyright © 2009 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
*/

#ifndef KALARMMIMETYPEVISITOR_H
#define KALARMMIMETYPEVISITOR_H

#include <kcal/incidence.h>

class KAlarmMimeTypeVisitor : public KCal::IncidenceBase::Visitor
{
    public:
        KAlarmMimeTypeVisitor()  {}

        /** Returns true if it is an Event. */
        virtual bool visit(KCal::Event* event)  { return event; }

        /** Returns false because it is not an Event. */
        virtual bool visit(KCal::Todo*)  { return false; }

        /** Returns false because it is not an Event. */
        virtual bool visit(KCal::Journal*)  { return false; }

        /** Returns false because it is not an Event. */
        virtual bool visit(KCal::FreeBusy*)  { return false; }

        /** Determines whether an Incidence is an Event. */
        bool isEvent(KCal::Incidence* i)  { return i->accept(*this); }
};

#endif

// vim: et sw=4:
