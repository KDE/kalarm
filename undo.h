/*
 *  undo.h  -  undo/redo facility
 *  Program:  kalarm
 *  Copyright © 2005-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef UNDO_H
#define UNDO_H

/**  @file undo.h - undo/redo facility */

#include "autodeletelist.h"

#include <kalarmcal/kaevent.h>

#include <AkonadiCore/collection.h>
#include <QList>
#include <QStringList>

#ifndef USE_AKONADI
class AlarmResource;
#endif
class UndoItem;
using namespace KAlarmCal;


class Undo : public QObject
{
        Q_OBJECT
    public:
        enum Type { NONE, UNDO, REDO };
        // N.B. The Event structure must be constructed before the action for
        // which the undo is being created is carried out, since the
        // don't-show-errors status is not contained within the KAEvent itself.
        struct Event
        {
            Event() {}
            Event(const KAEvent&, const Akonadi::Collection&);
            KAEvent        event;
            mutable Akonadi::Collection collection;
            QStringList    dontShowErrors;
        };
        class EventList : public QList<Event>
        {
        public:
            void append(const KAEvent& e, const Akonadi::Collection& c)  { QList<Event>::append(Event(e, c)); }
        };

        static Undo*       instance();
        static void        saveAdd(const KAEvent&, const Akonadi::Collection&, const QString& name = QString());
        static void        saveAdds(const EventList&, const QString& name = QString());
        static void        saveEdit(const Event& oldEvent, const KAEvent& newEvent);
        static void        saveDelete(const Event&, const QString& name = QString());
        static void        saveDeletes(const EventList&, const QString& name = QString());
        static void        saveReactivate(const KAEvent&, const Akonadi::Collection&, const QString& name = QString());
        static void        saveReactivates(const EventList&, const QString& name = QString());
        static bool        undo(QWidget* parent, const QString& action)
                                              { return undo(0, UNDO, parent, action); }
        static bool        undo(int id, QWidget* parent, const QString& action)
                                              { return undo(findItem(id, UNDO), UNDO, parent, action); }
        static bool        redo(QWidget* parent, const QString& action)
                                              { return undo(0, REDO, parent, action); }
        static bool        redo(int id, QWidget* parent, const QString& action)
                                              { return undo(findItem(id, REDO), REDO, parent, action); }
        static void        clear();
        static bool        haveUndo()         { return !mUndoList.isEmpty(); }
        static bool        haveRedo()         { return !mRedoList.isEmpty(); }
        static QString     actionText(Type);
        static QString     actionText(Type, int id);
        static QString     description(Type, int id);
        static QList<int>  ids(Type);
        static void        emitChanged();

        // Types for use by UndoItem class and its descendants
        typedef AutoDeleteList<UndoItem> List;

    signals:
        void               changed(const QString& undo, const QString& redo);

    protected:
        // Methods for use by UndoItem class
        static void        add(UndoItem*, bool undo);
        static void        remove(UndoItem*, bool undo);
        static void        replace(UndoItem* old, UndoItem* New);

    private:
        Undo(QObject* parent)  : QObject(parent) { }
        static void        removeRedos(const QString& eventID);
        static bool        undo(int index, Type, QWidget* parent, const QString& action);
        static UndoItem*   getItem(int id, Type);
        static int         findItem(int id, Type);
        void               emitChanged(const QString& undo, const QString& redo)
                                           { emit changed(undo, redo); }

        static Undo*       mInstance;     // the one and only Undo instance
        static List        mUndoList;     // edit history for undo, latest undo first
        static List        mRedoList;     // edit history for redo, latest redo first

    friend class UndoItem;
};

#endif // UNDO_H

// vim: et sw=4:
