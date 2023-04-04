/*
 *  undo.h  -  undo/redo facility
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/**  @file undo.h - undo/redo facility */

#include "resources/resource.h"
#include "lib/autodeletelist.h"
#include "kalarmcalendar/kaevent.h"

#include <QStringList>

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
        Event() = default;
        Event(const KAEvent&, const Resource&);
        KAEvent          event;
        mutable Resource resource;
        QStringList      dontShowErrors;
    };
    class EventList : public QList<Event> {
      public:
        void append(const KAEvent &e, const Resource &res) {
          QList<Event>::append(Event(e, res));
        }
    };

    static Undo*       instance();
    static void        saveAdd(const KAEvent&, const Resource&, const QString& name = QString());
    static void        saveAdds(const EventList&, const QString& name = QString());
    static void        saveEdit(const Event& oldEvent, const KAEvent& newEvent);
    static void        saveDelete(const Event&, const QString& name = QString());
    static void        saveDeletes(const EventList&, const QString& name = QString());
    static void        saveReactivate(const KAEvent&, const Resource&, const QString& name = QString());
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
    static QList<int> ids(Type);
    static void        emitChanged();
    static void        dumpDebug(Type, int count);

    // Types for use by UndoItem class and its descendants
    using List = AutoDeleteList<UndoItem>;

Q_SIGNALS:
    void               changed(const QString& undo, const QString& redo);

protected:
    // Methods for use by UndoItem class
    static void        add(UndoItem*, bool undo);
    static void        remove(UndoItem*, bool undo);
    static void        replace(UndoItem* old, UndoItem* New);

private:
    explicit Undo(QObject* parent)  : QObject(parent) {}
    static void        removeRedos(const QString& eventID);
    static bool        undo(int index, Type, QWidget* parent, const QString& action);
    static UndoItem*   getItem(int id, Type);
    static int         findItem(int id, Type);
    void               emitChanged(const QString& undo, const QString& redo)
                                       { Q_EMIT changed(undo, redo); }

    static Undo*       mInstance;     // the one and only Undo instance
    static List        mUndoList;     // edit history for undo, latest undo first
    static List        mRedoList;     // edit history for redo, latest redo first

friend class UndoItem;
};

// vim: et sw=4:
