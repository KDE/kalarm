/*
 *  undo.cpp  -  undo/redo facility
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "undo.h"

#include "functions.h"
#include "resources/resources.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/alarmtext.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QApplication>

static int maxCount = 12;

#ifdef DELETE
#undef DELETE // conflicting Windows macro
#endif

class UndoItem
{
    public:
        enum Operation { ADD, EDIT, DELETE, REACTIVATE, DEACTIVATE, MULTI };
        UndoItem();           // needed by QList
        virtual ~UndoItem();
        virtual Operation  operation() const = 0;
        virtual QString    actionText() const    { return !mName.isEmpty() ? mName : defaultActionText(); }
        virtual QString    defaultActionText() const = 0;
        virtual QString    description() const   { return {}; }
        virtual QString    eventID() const       { return {}; }
        virtual QString    oldEventID() const    { return {}; }
        virtual QString    newEventID() const    { return {}; }
        virtual Resource   resource() const      { return {}; }
        int                id() const            { return mId; }
        Undo::Type         type() const          { return mType; }
        void               setType(Undo::Type t) { mType = t; }
        CalEvent::Type     calendar() const      { return mCalendar; }
        virtual void       setCalendar(CalEvent::Type s) { mCalendar = s; }
        virtual UndoItem*  restore() = 0;
        virtual bool       deleteID(const QString& /*id*/)  { return false; }
        virtual void       dumpDebug() const;

        enum Error   { ERR_NONE, ERR_PROG, ERR_NOT_FOUND, ERR_CREATE, ERR_TEMPLATE, ERR_ARCHIVED };
        enum Warning { WARN_NONE, WARN_KORG_ADD, WARN_KORG_MODIFY, WARN_KORG_DELETE };
        static int         mLastId;
        static Error       mRestoreError;         // error code valid only if restore() returns 0
        static Warning     mRestoreWarning;       // warning code set by restore()
        static KAlarm::UpdateResult mRestoreWarningKorg; // KOrganizer error status set by restore()
        static int         mRestoreWarningCount;  // item count for mRestoreWarning (to allow i18n messages to work correctly)

    protected:
        UndoItem(Undo::Type, const QString& name = QString());
        static QString     addDeleteActionText(CalEvent::Type, bool add);
        QString            description(const KAEvent&) const;
        void               replaceWith(UndoItem* item)   { Undo::replace(this, item); }
        virtual void       dumpDebugTitle(const char* typeName) const;

        QString            mName;      // specified action name (overrides default)
        int                mId {0};    // unique identifier (only for mType = UNDO, REDO)
        Undo::Type         mType;      // which list (if any) the object is in
        CalEvent::Type     mCalendar;
};

class UndoMultiBase : public UndoItem
{
    public:
        UndoMultiBase(Undo::Type t, const QString& name)
                : UndoItem(t, name), mUndos(new Undo::List) {}
        UndoMultiBase(Undo::Type t, Undo::List* undos, const QString& name)
                : UndoItem(t, name), mUndos(undos) {}
        UndoMultiBase(const UndoMultiBase&) = delete;
        UndoMultiBase& operator=(const UndoMultiBase&) = delete;
        ~UndoMultiBase() override { delete mUndos; }
        const Undo::List* undos() const         { return mUndos; }
    protected:
        void               dumpDebugTitle(const char* typeName) const override;

        Undo::List* mUndos;    // this list must always have >= 2 entries
};

template <class T> class UndoMulti : public UndoMultiBase
{
    public:
        UndoMulti(Undo::Type, const Undo::EventList&, const QString& name);
        UndoMulti(Undo::Type t, Undo::List* undos, const QString& name)  : UndoMultiBase(t, undos, name) {}
        Operation         operation() const override     { return MULTI; }
        UndoItem*         restore() override;
        bool              deleteID(const QString& id) override;
        virtual UndoItem* createRedo(Undo::List*) = 0;
};

class UndoAdd : public UndoItem
{
    public:
        UndoAdd(Undo::Type, const Undo::Event&, const QString& name = QString());
        UndoAdd(Undo::Type, const KAEvent&, const Resource&, const QString& name = QString());
        UndoAdd(Undo::Type, const KAEvent&, const Resource&, const QString& name, CalEvent::Type);
        Operation          operation() const override     { return ADD; }
        QString            defaultActionText() const override;
        QString            description() const override   { return mDescription; }
        Resource           resource() const override      { return mResource; }
        QString            eventID() const override       { return mEventId; }
        QString            newEventID() const override    { return mEventId; }
        UndoItem*          restore() override             { return doRestore(); }
        void               dumpDebug() const override;
    protected:
        UndoItem*          doRestore(bool setArchive = false);
        virtual UndoItem*  createRedo(const KAEvent&, const Resource&);
        void               dumpDebugTitle(const char* typeName) const override;
    private:
        Resource       mResource;  // resource containing the event
        QString        mEventId;
        QString        mDescription;
};

class UndoEdit : public UndoItem
{
    public:
        UndoEdit(Undo::Type, const KAEvent& oldEvent, const QString& newEventID,
                 const Resource&, const QStringList& dontShowErrors, const QString& description);
        UndoEdit(const UndoEdit&) = delete;
        UndoEdit& operator=(const UndoEdit&) = delete;
        ~UndoEdit() override;
        Operation          operation() const override     { return EDIT; }
        QString            defaultActionText() const override;
        QString            description() const override   { return mDescription; }
        Resource           resource() const override      { return mResource; }
        QString            eventID() const override       { return mNewEventId; }
        QString            oldEventID() const override    { return mOldEvent->id(); }
        QString            newEventID() const override    { return mNewEventId; }
        UndoItem*          restore() override;
        void               dumpDebug() const override;
    protected:
        void               dumpDebugTitle(const char* typeName) const override;
    private:
        Resource       mResource;  // resource containing the event
        KAEvent*       mOldEvent;
        QString        mNewEventId;
        QString        mDescription;
        QStringList    mDontShowErrors;
};

class UndoDelete : public UndoItem
{
    public:
        UndoDelete(Undo::Type, const Undo::Event&, const QString& name = QString());
        UndoDelete(Undo::Type, const KAEvent&, const Resource&, const QStringList& dontShowErrors, const QString& name = QString());
        UndoDelete(const UndoDelete&) = delete;
        UndoDelete& operator=(const UndoDelete&) = delete;
        ~UndoDelete() override;
        Operation          operation() const override     { return DELETE; }
        QString            defaultActionText() const override;
        QString            description() const override   { return UndoItem::description(*mEvent); }
        Resource           resource() const override      { return mResource; }
        QString            eventID() const override       { return mEvent->id(); }
        QString            oldEventID() const override    { return mEvent->id(); }
        UndoItem*          restore() override;
        KAEvent*           event() const                  { return mEvent; }
        void               dumpDebug() const override;
    protected:
        virtual UndoItem*  createRedo(const KAEvent&, const Resource&);
        void               dumpDebugTitle(const char* typeName) const override;
    private:
        Resource       mResource;  // resource containing the event
        KAEvent*       mEvent;
        QStringList    mDontShowErrors;
};

class UndoReactivate : public UndoAdd
{
    public:
        UndoReactivate(Undo::Type t, const Undo::Event& e, const QString& name = QString())
                 : UndoAdd(t, e.event, e.resource, name, CalEvent::ACTIVE) {}
        UndoReactivate(Undo::Type t, const KAEvent& e, const Resource& r, const QString& name = QString())
                 : UndoAdd(t, e, r, name, CalEvent::ACTIVE) {}
        Operation          operation() const override     { return REACTIVATE; }
        QString            defaultActionText() const override;
        UndoItem*          restore() override;
        void               dumpDebug() const override;
    protected:
        UndoItem*          createRedo(const KAEvent&, const Resource&) override;
};

class UndoDeactivate : public UndoDelete
{
    public:
        UndoDeactivate(Undo::Type t, const KAEvent& e, const Resource& r, const QString& name = QString())
                 : UndoDelete(t, e, r, QStringList(), name) {}
        Operation          operation() const override     { return DEACTIVATE; }
        QString            defaultActionText() const override;
        UndoItem*          restore() override;
        void               dumpDebug() const override;
    protected:
        UndoItem*          createRedo(const KAEvent&, const Resource&) override;
};

class UndoAdds : public UndoMulti<UndoAdd>
{
    public:
        UndoAdds(Undo::Type t, const Undo::EventList& events, const QString& name = QString())
                 : UndoMulti<UndoAdd>(t, events, name) {}   // UNDO only
        UndoAdds(Undo::Type t, Undo::List* undos, const QString& name)
                 : UndoMulti<UndoAdd>(t, undos, name) {}
        QString            defaultActionText() const override;
        UndoItem*          createRedo(Undo::List*) override;
        void               dumpDebug() const override;
};

class UndoDeletes : public UndoMulti<UndoDelete>
{
    public:
        UndoDeletes(Undo::Type t, const Undo::EventList& events, const QString& name = QString())
                 : UndoMulti<UndoDelete>(t, events, name) {}   // UNDO only
        UndoDeletes(Undo::Type t, Undo::List* undos, const QString& name)
                 : UndoMulti<UndoDelete>(t, undos, name) {}
        QString            defaultActionText() const override;
        UndoItem*          createRedo(Undo::List*) override;
        void               dumpDebug() const override;
};

class UndoReactivates : public UndoMulti<UndoReactivate>
{
    public:
        UndoReactivates(Undo::Type t, const Undo::EventList& events, const QString& name = QString())
                 : UndoMulti<UndoReactivate>(t, events, name) {}   // UNDO only
        UndoReactivates(Undo::Type t, Undo::List* undos, const QString& name)
                 : UndoMulti<UndoReactivate>(t, undos, name) {}
        QString            defaultActionText() const override;
        UndoItem*          createRedo(Undo::List*) override;
        void               dumpDebug() const override;
};

Undo*       Undo::mInstance = nullptr;
Undo::List  Undo::mUndoList;
Undo::List  Undo::mRedoList;


/******************************************************************************
* Create the one and only instance of the Undo class.
*/
Undo* Undo::instance()
{
    if (!mInstance)
        mInstance = new Undo(qApp);
    return mInstance;
}

/******************************************************************************
* Clear the lists of undo and redo items.
*/
void Undo::clear()
{
    if (!mUndoList.isEmpty()  ||  !mRedoList.isEmpty())
    {
        mInstance->blockSignals(true);
        while (!mUndoList.isEmpty())
            delete mUndoList.first();    // N.B. 'delete' removes the object from the list
        while (!mRedoList.isEmpty())
            delete mRedoList.first();    // N.B. 'delete' removes the object from the list
        mInstance->blockSignals(false);
        emitChanged();
    }
}

/******************************************************************************
* Create an undo item and add it to the list of undos.
* N.B. The base class constructor adds the object to the undo list.
*/
void Undo::saveAdd(const KAEvent& event, const Resource& resource, const QString& name)
{
    new UndoAdd(UNDO, event, resource, name);
    emitChanged();
}

void Undo::saveAdds(const Undo::EventList& events, const QString& name)
{
    int count = events.count();
    if (count == 1)
        saveAdd(events.first().event, events.first().resource, name);
    else if (count > 1)
    {
        new UndoAdds(UNDO, events, name);
        emitChanged();
    }
}

void Undo::saveEdit(const Undo::Event& oldEvent, const KAEvent& newEvent)
{
    new UndoEdit(UNDO, oldEvent.event, newEvent.id(), oldEvent.resource, oldEvent.dontShowErrors, AlarmText::summary(newEvent));
    removeRedos(oldEvent.event.id());    // remove any redos which are made invalid by this edit
    emitChanged();
}

void Undo::saveDelete(const Undo::Event& event, const QString& name)
{
    new UndoDelete(UNDO, event.event, event.resource, event.dontShowErrors, name);
    removeRedos(event.event.id());    // remove any redos which are made invalid by this deletion
    emitChanged();
}

void Undo::saveDeletes(const Undo::EventList& events, const QString& name)
{
    int count = events.count();
    if (count == 1)
        saveDelete(events[0], name);
    else if (count > 1)
    {
        new UndoDeletes(UNDO, events, name);
        for (const Undo::Event& event : events)
            removeRedos(event.event.id());    // remove any redos which are made invalid by these deletions
        emitChanged();
    }
}

void Undo::saveReactivate(const KAEvent& event, const Resource& resource, const QString& name)
{
    new UndoReactivate(UNDO, event, resource, name);
    emitChanged();
}

void Undo::saveReactivates(const EventList& events, const QString& name)
{
    int count = events.count();
    if (count == 1)
        saveReactivate(events[0].event, events[0].resource, name);
    else if (count > 1)
    {
        new UndoReactivates(UNDO, events, name);
        emitChanged();
    }
}

/******************************************************************************
* Remove any redos which are made invalid by a new undo.
*/
void Undo::removeRedos(const QString& eventID)
{
    QString id = eventID;
    for (int i = 0;  i < mRedoList.count();  )
    {
        UndoItem* item = mRedoList[i];
        if (item->operation() == UndoItem::MULTI)
        {
            if (item->deleteID(id))
            {
                // The old multi-redo was replaced with a new single redo
                delete item;   // N.B. 'delete' removes the object from the list
            }
        }
        else if (item->eventID() == id)
        {
            if (item->operation() == UndoItem::EDIT)
                id = item->oldEventID();   // continue looking for its post-edit ID
            delete item;   // N.B. 'delete' removes the object from the list
        }
        else
            ++i;
    }
}

/******************************************************************************
* Undo or redo a specified item.
* Reply = true if success, or if the item no longer exists.
*/
bool Undo::undo(int i, Undo::Type type, QWidget* parent, const QString& action)
{
    UndoItem::mRestoreError        = UndoItem::ERR_NONE;
    UndoItem::mRestoreWarning      = UndoItem::WARN_NONE;
    UndoItem::mRestoreWarningKorg  = KAlarm::UPDATE_OK;
    UndoItem::mRestoreWarningCount = 0;
    List& list = (type == UNDO) ? mUndoList : mRedoList;
    if (i < list.count()  &&  list[i]->type() == type)
    {
        list[i]->restore();
        delete list[i];    // N.B. 'delete' removes the object from its list
        emitChanged();
    }

    QString err;
    switch (UndoItem::mRestoreError)
    {
        case UndoItem::ERR_NONE:
        {
            KAlarm::UpdateError errcode;
            switch (UndoItem::mRestoreWarning)
            {
                case UndoItem::WARN_KORG_ADD:     errcode = KAlarm::ERR_ADD;  break;
                case UndoItem::WARN_KORG_MODIFY:  errcode = KAlarm::ERR_MODIFY;  break;
                case UndoItem::WARN_KORG_DELETE:  errcode = KAlarm::ERR_DELETE;  break;
                case UndoItem::WARN_NONE:
                default:
                    return true;
            }
            KAlarm::displayKOrgUpdateError(parent, errcode, UndoItem::mRestoreWarningKorg, UndoItem::mRestoreWarningCount);
            return true;
        }
        case UndoItem::ERR_NOT_FOUND:  err = i18nc("@info", "Alarm not found");  break;
        case UndoItem::ERR_CREATE:     err = i18nc("@info", "Error recreating alarm");  break;
        case UndoItem::ERR_TEMPLATE:   err = i18nc("@info", "Error recreating alarm template");  break;
        case UndoItem::ERR_ARCHIVED:   err = i18nc("@info", "Cannot reactivate archived alarm");  break;
        case UndoItem::ERR_PROG:       err = i18nc("@info", "Program error");  break;
        default:                       err = i18nc("@info", "Unknown error");  break;
    }
    KAMessageBox::error(parent, i18nc("@info Undo-action: message", "%1: %2", action, err));
    return false;
}

/******************************************************************************
* Add an undo item to the start of one of the lists.
*/
void Undo::add(UndoItem* item, bool undo)
{
    if (item)
    {
        // Limit the number of items stored
        const int undoCount = mUndoList.count();
        const int redoCount = mRedoList.count();
        if (undoCount + redoCount >= maxCount - 1)
        {
            if (undoCount)
                mUndoList.pop_back();
            else
                mRedoList.pop_back();
        }

        // Append the new item
        List* const list = undo ? &mUndoList : &mRedoList;
        list->prepend(item);
    }
}

/******************************************************************************
* Remove an undo item from one of the lists.
*/
void Undo::remove(UndoItem* item, bool undo)
{
    List* const list = undo ? &mUndoList : &mRedoList;
    if (!list->isEmpty())
        list->removeAt(list->indexOf(item));
}

/******************************************************************************
* Replace an undo item in one of the lists.
*/
void Undo::replace(UndoItem* old, UndoItem* New)
{
    const Type type = old->type();
    List* const list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : nullptr;
    if (!list)
        return;
    const int i = list->indexOf(old);
    if (i >= 0)
    {
        New->setType(type);    // ensure the item points to the correct list
        (*list)[i] = New;
        old->setType(NONE);    // mark the old item as no longer being in a list
    }
}

/******************************************************************************
* Return the action description of the latest undo/redo item.
*/
QString Undo::actionText(Undo::Type type)
{
    const List* const list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : nullptr;
    return (list && !list->isEmpty()) ? (*list)[0]->actionText() : QString();
}

/******************************************************************************
* Return the action description of the undo/redo item with the specified ID.
*/
QString Undo::actionText(Undo::Type type, int id)
{
    const UndoItem* undo_ = getItem(id, type);
    return undo_ ? undo_->actionText() : QString();
}

/******************************************************************************
* Return the alarm description of the undo/redo item with the specified ID.
*/
QString Undo::description(Undo::Type type, int id)
{
    const UndoItem* undo_ = getItem(id, type);
    return undo_ ? undo_->description() : QString();
}

/******************************************************************************
* Return the descriptions of all undo or redo items, in order latest first.
* For alarms which have undergone more than one change, only the first one is
* listed, to force dependent undos to be executed in their correct order.
* If 'ids' is non-null, also returns a list of their corresponding IDs.
*/
QList<int> Undo::ids(Undo::Type type)
{
    QList<int> ids;
    QStringList ignoreIDs;
//int n=0;
    const List* const list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : nullptr;
    if (!list)
        return ids;
    for (const UndoItem* item : *list)
    {
        // Check whether this item should be ignored because it is a
        // dependent undo. If not, add this item's ID to the ignore list.
        bool omit = false;
        if (item->operation() == UndoItem::MULTI)
        {
            // If any item in a multi-undo is disqualified, omit the whole multi-undo
            QStringList newIDs;
            const Undo::List* undos = ((UndoMultiBase*)item)->undos();
            for (const UndoItem* undo_ : *undos)
            {
                const QString evid = undo_->eventID();
                if (ignoreIDs.contains(evid))
                    omit = true;
                else if (omit)
                    ignoreIDs.append(evid);
                else
                    newIDs.append(evid);
            }
            if (omit)
            {
                for (const QString& newID : std::as_const(newIDs))
                    ignoreIDs.append(newID);
            }
        }
        else
        {
            omit = ignoreIDs.contains(item->eventID());
            if (!omit)
                ignoreIDs.append(item->eventID());
            if (item->operation() == UndoItem::EDIT)
                ignoreIDs.append(item->oldEventID());   // continue looking for its post-edit ID
        }
        if (!omit)
            ids.append(item->id());
//else qCDebug(KALARM_LOG)<<"Undo::ids(): omit"<<item->actionText()<<":"<<item->description();
    }
//qCDebug(KALARM_LOG)<<"Undo::ids():"<<n<<" ->"<<ids.count();
    return ids;
}

/******************************************************************************
* Emit the appropriate 'changed' signal.
*/
void Undo::emitChanged()
{
    if (mInstance)
        mInstance->emitChanged(actionText(UNDO), actionText(REDO));
}

/******************************************************************************
* Return the item with the specified ID.
*/
UndoItem* Undo::getItem(int id, Undo::Type type)
{
    List* const list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : nullptr;
    if (list)
    {
        for (int i = 0, end = list->count();  i < end;  ++i)
        {
            UndoItem* item = (*list)[i];
            if (item->id() == id)
                return item;
        }
    }
    return nullptr;
}

/******************************************************************************
* Find an item with the specified ID.
*/
int Undo::findItem(int id, Undo::Type type)
{
    const List& list = (type == UNDO) ? mUndoList : mRedoList;
    int i = 0;
    for (int end = list.count();  i < end;  ++i)
    {
        if (list[i]->id() == id)
            break;
    }
    return i;
}

/******************************************************************************
* Dump the last 'count' undos or redos to debug, starting with the most recent.
*/
void Undo::dumpDebug(Undo::Type type, int count)
{
#ifndef KDE_NO_DEBUG_OUTPUT
    const List& list = (type == UNDO) ? mUndoList : mRedoList;
    if (count > list.count())
        count = list.count();
    qCDebug(KALARM_LOG) << "Undo::dumpDebug():" << count;
    for (int i = 0;  i < count;  ++i)
        list[i]->dumpDebug();
#endif
}

/*=============================================================================
=  Class: UndoItem
=  A single undo action.
=============================================================================*/
int                  UndoItem::mLastId = 0;
UndoItem::Error      UndoItem::mRestoreError;
UndoItem::Warning    UndoItem::mRestoreWarning;
KAlarm::UpdateResult UndoItem::mRestoreWarningKorg;
int                  UndoItem::mRestoreWarningCount;

/******************************************************************************
* Constructor.
* Optionally appends the undo to the list of undos.
*/
UndoItem::UndoItem(Undo::Type type, const QString& name)
    : mName(name)
    , mType(type)
    , mCalendar(CalEvent::EMPTY)
{
    if (type != Undo::NONE)
    {
        mId = ++mLastId;
        if (mId < 0)
            mId = mLastId = 1;    // wrap round if we reach a negative number
        Undo::add(this, (mType == Undo::UNDO));
    }
}

/******************************************************************************
* Destructor.
* Removes the undo from the list (if it's in the list).
*/
UndoItem::~UndoItem()
{
    if (mType != Undo::NONE)
        Undo::remove(this, (mType == Undo::UNDO));
}

/******************************************************************************
* Return the description of an event.
*/
QString UndoItem::description(const KAEvent& event) const
{
    return (mCalendar == CalEvent::TEMPLATE) ? event.name() : AlarmText::summary(event);
}

/******************************************************************************
* Return the action description of an add or delete Undo/Redo item for displaying.
*/
QString UndoItem::addDeleteActionText(CalEvent::Type calendar, bool add)
{
    switch (calendar)
    {
        case CalEvent::ACTIVE:
            if (add)
                return i18nc("@info Action to create a new alarm", "New alarm");
            else
                return i18nc("@info Action to delete an alarm", "Delete alarm");
        case CalEvent::TEMPLATE:
            if (add)
                return i18nc("@info Action to create a new alarm template", "New template");
            else
                return i18nc("@info Action to delete an alarm template", "Delete template");
        case CalEvent::ARCHIVED:
            return i18nc("@info", "Delete archived alarm");
        default:
            break;
    }
    return {};
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoItem::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoItem");
#endif
}
void UndoItem::dumpDebugTitle(const char* typeName) const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    qCDebug(KALARM_LOG) << typeName << "begin:";
    qCDebug(KALARM_LOG) << "-- mName:       " << mName;
    qCDebug(KALARM_LOG) << "-- mId:         " << mId;
    qCDebug(KALARM_LOG) << "-- mType:       " << (mType == Undo::UNDO ? "Undo" : mType == Undo::REDO ? "Redo" : "None");
    qCDebug(KALARM_LOG) << "-- mCalendar:   " << mCalendar;
#endif
}

/*=============================================================================
=  Class: UndoMultiBase
=  Undo item for multiple alarms.
=============================================================================*/

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoMultiBase::dumpDebugTitle(const char* typeName) const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    UndoItem::dumpDebugTitle(typeName);
    qCDebug(KALARM_LOG) << "-- mUndos count:" << mUndos->count();
#endif
}

/*=============================================================================
=  Class: UndoMulti
=  Undo item for multiple alarms.
=============================================================================*/

template <class T>
UndoMulti<T>::UndoMulti(Undo::Type type, const Undo::EventList& events, const QString& name)
    : UndoMultiBase(type, name)    // UNDO only
{
    for (const Undo::Event& event : events)
        mUndos->append(new T(Undo::NONE, event));
}

/******************************************************************************
* Undo the item, i.e. restore multiple alarms which were deleted (or delete
* alarms which were restored).
* Create a redo item to delete (or restore) the alarms again.
* Reply = redo item.
*/
template <class T>
UndoItem* UndoMulti<T>::restore()
{
    auto newUndos = new Undo::List;
    for (int i = 0, end = mUndos->count();  i < end;  ++i)
    {
        UndoItem* undo = (*mUndos)[i]->restore();
        if (undo)
            newUndos->append(undo);
    }
    if (newUndos->isEmpty())
    {
        delete newUndos;
        return nullptr;
    }

    // Create a redo item to delete the alarm again
    return createRedo(newUndos);
}

/******************************************************************************
* If one of the multiple items has the specified ID, delete it.
* If an item is deleted and there is only one item left, the UndoMulti
* instance is removed from its list and replaced by the remaining UndoItem instead.
* Reply = true if this instance was replaced. The caller must delete it.
*       = false otherwise.
*/
template <class T>
bool UndoMulti<T>::deleteID(const QString& id)
{
    for (int i = 0, end = mUndos->count();  i < end;  ++i)
    {
        UndoItem* item = (*mUndos)[i];
        if (item->eventID() == id)
        {
            // Found a matching entry - remove it
            mUndos->removeAt(i);
            if (mUndos->count() == 1)
            {
                // There is only one entry left after removal.
                // Replace 'this' multi instance with the remaining single entry.
                replaceWith(item);
                return true;
            }
            else
            {
                delete item;
                return false;
            }
        }
    }
    return false;
}


/*=============================================================================
=  Class: UndoAdd
=  Undo item for alarm creation.
=============================================================================*/

UndoAdd::UndoAdd(Undo::Type type, const Undo::Event& undo, const QString& name)
    : UndoItem(type, name)
    , mResource(undo.resource)
    , mEventId(undo.event.id())
{
    setCalendar(undo.event.category());
    mDescription = UndoItem::description(undo.event);    // calendar must be set before calling this
}

UndoAdd::UndoAdd(Undo::Type type, const KAEvent& event, const Resource& resource, const QString& name)
    : UndoItem(type, name)
    , mResource(resource)
    , mEventId(event.id())
{
    setCalendar(event.category());
    mDescription = UndoItem::description(event);    // calendar must be set before calling this
}

UndoAdd::UndoAdd(Undo::Type type, const KAEvent& event, const Resource& resource, const QString& name, CalEvent::Type cal)
    : UndoItem(type, name)
    , mResource(resource)
    , mEventId(CalEvent::uid(event.id(), cal))    // convert if old-style event ID
{
    setCalendar(cal);
    mDescription = UndoItem::description(event);    // calendar must be set before calling this
}

/******************************************************************************
* Undo the item, i.e. delete the alarm which was added.
* Create a redo item to add the alarm back again.
* Reply = redo item.
*/
UndoItem* UndoAdd::doRestore(bool setArchive)
{
    // Retrieve the current state of the alarm
    qCDebug(KALARM_LOG) << "UndoAdd::doRestore:" << mEventId;
    KAEvent event = mResource.event(mEventId);
    if (!event.isValid())
    {
        // Alarm is no longer in calendar, or its type is now disabled
        mRestoreError = ERR_NOT_FOUND;
        return nullptr;
    }

    // Create a redo item to recreate the alarm.
    // Do it now, since 'event' gets modified by KAlarm::deleteEvent()
    UndoItem* undo = createRedo(event, mResource);

    switch (calendar())
    {
        case CalEvent::ACTIVE:
        {
            if (setArchive)
                event.setArchive();
            // Archive it if it has already triggered
            KAlarm::UpdateResult status = KAlarm::deleteEvent(event, mResource, true);
            switch (status.status)
            {
                case KAlarm::UPDATE_ERROR:
                case KAlarm::UPDATE_FAILED:
                case KAlarm::SAVE_FAILED:
                    mRestoreError = ERR_CREATE;
                    break;
                case KAlarm::UPDATE_KORG_FUNCERR:
                case KAlarm::UPDATE_KORG_ERRINIT:
                case KAlarm::UPDATE_KORG_ERRSTART:
                case KAlarm::UPDATE_KORG_ERR:
                    mRestoreWarning = WARN_KORG_DELETE;
                    ++mRestoreWarningCount;
                    if (status.status > mRestoreWarningKorg.status)
                        mRestoreWarningKorg = status;
                    break;
                default:
                    break;
            }
            break;
        }
        case CalEvent::TEMPLATE:
            if (KAlarm::deleteTemplate(event) != KAlarm::UPDATE_OK)
                mRestoreError = ERR_TEMPLATE;
            break;
        case CalEvent::ARCHIVED:    // redoing the deletion of an archived alarm
        {
            Resource res;
            KAlarm::deleteEvent(event, res);
            break;
        }
        default:
            delete undo;
            mRestoreError = ERR_PROG;
            return nullptr;
    }
    return undo;
}

/******************************************************************************
* Create a redo item to add the alarm back again.
*/
UndoItem* UndoAdd::createRedo(const KAEvent& event, const Resource& resource)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoDelete(t, event, resource, QStringList(), mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoAdd::defaultActionText() const
{
    return addDeleteActionText(calendar(), (type() == Undo::UNDO));
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoAdd::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoAdd");
#endif
}
void UndoAdd::dumpDebugTitle(const char* typeName) const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    UndoItem::dumpDebugTitle(typeName);
    qCDebug(KALARM_LOG) << "-- mResource:   " << mResource.id();
    qCDebug(KALARM_LOG) << "-- mEventId:    " << mEventId;
    qCDebug(KALARM_LOG) << "-- mDescript:   " << mDescription;
#endif
}


/*=============================================================================
=  Class: UndoAdds
=  Undo item for multiple alarm creation.
=============================================================================*/

/******************************************************************************
* Create a redo item to add the alarms again.
*/
UndoItem* UndoAdds::createRedo(Undo::List* undos)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoAdds(t, undos, mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoAdds::defaultActionText() const
{
    return i18nc("@info", "Create multiple alarms");
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoAdds::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoAdds");
#endif
}


/*=============================================================================
=  Class: UndoEdit
=  Undo item for alarm edit.
=============================================================================*/

UndoEdit::UndoEdit(Undo::Type type, const KAEvent& oldEvent, const QString& newEventID,
                   const Resource& resource, const QStringList& dontShowErrors, const QString& description)
    : UndoItem(type)
    , mResource(resource)
    , mOldEvent(new KAEvent(oldEvent))
    , mNewEventId(newEventID)
    , mDescription(description)
    , mDontShowErrors(dontShowErrors)
{
    setCalendar(oldEvent.category());
}

UndoEdit::~UndoEdit()
{
    delete mOldEvent;
}

/******************************************************************************
* Undo the item, i.e. undo an edit to a previously existing alarm.
* Create a redo item to reapply the edit.
* Reply = redo item.
*/
UndoItem* UndoEdit::restore()
{
    qCDebug(KALARM_LOG) << "UndoEdit::restore:" << mNewEventId;
    // Retrieve the current state of the alarm
    KAEvent newEvent = mResource.event(mNewEventId);
    if (!newEvent.isValid())
    {
        mRestoreError = ERR_NOT_FOUND;    // alarm is no longer in calendar
        return nullptr;
    }

    // Create a redo item to restore the edit
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    UndoItem* undo = new UndoEdit(t, newEvent, mOldEvent->id(), mResource, KAlarm::dontShowErrors(EventId(newEvent)), mDescription);

    switch (calendar())
    {
        case CalEvent::ACTIVE:
        {
            KAlarm::UpdateResult status = KAlarm::modifyEvent(newEvent, *mOldEvent);
            switch (status.status)
            {
                case KAlarm::UPDATE_ERROR:
                case KAlarm::UPDATE_FAILED:
                case KAlarm::SAVE_FAILED:
                    mRestoreError = ERR_CREATE;
                    break;
                case KAlarm::UPDATE_KORG_FUNCERR:
                case KAlarm::UPDATE_KORG_ERRINIT:
                case KAlarm::UPDATE_KORG_ERRSTART:
                case KAlarm::UPDATE_KORG_ERR:
                    mRestoreWarning = WARN_KORG_MODIFY;
                    ++mRestoreWarningCount;
                    if (status.status > mRestoreWarningKorg.status)
                        mRestoreWarningKorg = status;
                    // fall through to default
                    [[fallthrough]];
                default:
                    KAlarm::setDontShowErrors(EventId(*mOldEvent), mDontShowErrors);
                    break;
            }
            break;
        }
        case CalEvent::TEMPLATE:
            if (KAlarm::updateTemplate(*mOldEvent) != KAlarm::UPDATE_OK)
                mRestoreError = ERR_TEMPLATE;
            break;
        case CalEvent::ARCHIVED:    // editing of archived events is not allowed
        default:
            delete undo;
            mRestoreError = ERR_PROG;
            return nullptr;
    }
    return undo;
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoEdit::defaultActionText() const
{
    switch (calendar())
    {
        case CalEvent::ACTIVE:
            return i18nc("@info Action to edit an alarm", "Edit alarm");
        case CalEvent::TEMPLATE:
            return i18nc("@info Action to edit an alarm template", "Edit template");
        default:
            break;
    }
    return {};
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoEdit::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoEdit");
#endif
}
void UndoEdit::dumpDebugTitle(const char* typeName) const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    UndoItem::dumpDebugTitle(typeName);
    qCDebug(KALARM_LOG) << "-- mResource:   " << mResource.id();
    qCDebug(KALARM_LOG) << "-- mOldEvent:   " << mOldEvent->id();
    qCDebug(KALARM_LOG) << "-- mNewEventId: " << mNewEventId;
    qCDebug(KALARM_LOG) << "-- mDescription:" << mDescription;
    qCDebug(KALARM_LOG) << "-- mDontShowErr:" << mDontShowErrors;
#endif
}


/*=============================================================================
=  Class: UndoDelete
=  Undo item for alarm deletion.
=============================================================================*/

UndoDelete::UndoDelete(Undo::Type type, const Undo::Event& undo, const QString& name)
    : UndoItem(type, name)
    , mResource(undo.resource)
    , mEvent(new KAEvent(undo.event))
    , mDontShowErrors(undo.dontShowErrors)
{
    setCalendar(mEvent->category());
}

UndoDelete::UndoDelete(Undo::Type type, const KAEvent& event, const Resource& resource, const QStringList& dontShowErrors, const QString& name)
    : UndoItem(type, name)
    , mResource(resource)
    , mEvent(new KAEvent(event))
    , mDontShowErrors(dontShowErrors)
{
    setCalendar(mEvent->category());
}

UndoDelete::~UndoDelete()
{
    delete mEvent;
}

/******************************************************************************
* Undo the item, i.e. restore an alarm which was deleted.
* Create a redo item to delete the alarm again.
* Reply = redo item.
*/
UndoItem* UndoDelete::restore()
{
    qCDebug(KALARM_LOG) << "UndoDelete::restore:" << mEvent->id();
    // Restore the original event
    CalEvent::Type saveType = calendar();
    switch (calendar())
    {
        case CalEvent::ACTIVE:
            if (mEvent->toBeArchived())
            {
                // It was archived when it was deleted
                mEvent->setCategory(CalEvent::ARCHIVED);
                mEvent->setResourceId(Resources::resourceForEvent(mEvent->id()).id());
                const KAlarm::UpdateResult status = KAlarm::reactivateEvent(*mEvent, mResource);
                switch (status.status)
                {
                    case KAlarm::UPDATE_KORG_FUNCERR:
                    case KAlarm::UPDATE_KORG_ERRINIT:
                    case KAlarm::UPDATE_KORG_ERRSTART:
                    case KAlarm::UPDATE_KORG_ERR:
                        mRestoreWarning = WARN_KORG_ADD;
                        ++mRestoreWarningCount;
                        if (status.status > mRestoreWarningKorg.status)
                            mRestoreWarningKorg = status;
                        break;
                    case KAlarm::UPDATE_ERROR:
                    case KAlarm::UPDATE_FAILED:
                    case KAlarm::SAVE_FAILED:
                        mRestoreError = ERR_ARCHIVED;
                        return nullptr;
                    case KAlarm::UPDATE_OK:
                        break;
                }
            }
            else
            {
                const KAlarm::UpdateResult status = KAlarm::addEvent(*mEvent, mResource, nullptr, true);
                switch (status.status)
                {
                    case KAlarm::UPDATE_KORG_FUNCERR:
                    case KAlarm::UPDATE_KORG_ERRINIT:
                    case KAlarm::UPDATE_KORG_ERRSTART:
                    case KAlarm::UPDATE_KORG_ERR:
                        mRestoreWarning = WARN_KORG_ADD;
                        ++mRestoreWarningCount;
                        if (status.status > mRestoreWarningKorg.status)
                            mRestoreWarningKorg = status;
                        break;
                    case KAlarm::UPDATE_ERROR:
                    case KAlarm::UPDATE_FAILED:
                    case KAlarm::SAVE_FAILED:
                        mRestoreError = ERR_CREATE;
                        return nullptr;
                    case KAlarm::UPDATE_OK:
                        break;
                }
            }
            KAlarm::setDontShowErrors(EventId(*mEvent), mDontShowErrors);
            break;
        case CalEvent::TEMPLATE:
            if (KAlarm::addTemplate(*mEvent, mResource) != KAlarm::UPDATE_OK)
            {
                mRestoreError = ERR_CREATE;
                return nullptr;
            }
            break;
        case CalEvent::ARCHIVED:
            if (!KAlarm::addArchivedEvent(*mEvent, mResource))
            {
                mRestoreError = ERR_CREATE;
                return nullptr;
            }
            break;
        default:
            mRestoreError = ERR_PROG;
            return nullptr;
    }

    // Create a redo item to delete the alarm again
    mEvent->setCategory(saveType);
    return createRedo(*mEvent, mResource);
}

/******************************************************************************
* Create a redo item to archive the alarm again.
*/
UndoItem* UndoDelete::createRedo(const KAEvent& event, const Resource& resource)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoAdd(t, event, resource, mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoDelete::defaultActionText() const
{
    return addDeleteActionText(calendar(), (type() == Undo::REDO));
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoDelete::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoDelete");
#endif
}
void UndoDelete::dumpDebugTitle(const char* typeName) const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    UndoItem::dumpDebugTitle(typeName);
    qCDebug(KALARM_LOG) << "-- mResource:   " << mResource.id();
    qCDebug(KALARM_LOG) << "-- mEvent:      " << mEvent->id();
    qCDebug(KALARM_LOG) << "-- mDontShowErr:" << mDontShowErrors;
#endif
}


/*=============================================================================
=  Class: UndoDeletes
=  Undo item for multiple alarm deletion.
=============================================================================*/

/******************************************************************************
* Create a redo item to delete the alarms again.
*/
UndoItem* UndoDeletes::createRedo(Undo::List* undos)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoDeletes(t, undos, mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoDeletes::defaultActionText() const
{
    if (mUndos->isEmpty())
        return {};
    for (const UndoItem* item : std::as_const(*mUndos))
    {
        switch (item->calendar())
        {
            case CalEvent::ACTIVE:
                return i18nc("@info", "Delete multiple alarms");
            case CalEvent::TEMPLATE:
                return i18nc("@info", "Delete multiple templates");
            case CalEvent::ARCHIVED:
                break;    // check if they are ALL archived
            default:
                return {};
        }
    }
    return i18nc("@info", "Delete multiple archived alarms");
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoDeletes::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoDeletes");
#endif
}


/*=============================================================================
=  Class: UndoReactivate
=  Undo item for alarm reactivation.
=============================================================================*/

/******************************************************************************
* Undo the item, i.e. re-archive the alarm which was reactivated.
* Create a redo item to reactivate the alarm back again.
* Reply = redo item.
*/
UndoItem* UndoReactivate::restore()
{
    qCDebug(KALARM_LOG) << "UndoReactivate::restore";
    // Validate the alarm's calendar
    switch (calendar())
    {
        case CalEvent::ACTIVE:
            break;
        default:
            mRestoreError = ERR_PROG;
            return nullptr;
    }
    return UndoAdd::doRestore(true);     // restore alarm, ensuring that it is re-archived
}

/******************************************************************************
* Create a redo item to add the alarm back again.
*/
UndoItem* UndoReactivate::createRedo(const KAEvent& event, const Resource& resource)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoDeactivate(t, event, resource, mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoReactivate::defaultActionText() const
{
    return i18nc("@info", "Reactivate alarm");
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoReactivate::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoReactivate");
#endif
}


/*=============================================================================
=  Class: UndoDeactivate
=  Redo item for alarm reactivation.
=============================================================================*/

/******************************************************************************
* Undo the item, i.e. reactivate an alarm which was archived.
* Create a redo item to archive the alarm again.
* Reply = redo item.
*/
UndoItem* UndoDeactivate::restore()
{
    qCDebug(KALARM_LOG) << "UndoDeactivate::restore";
    // Validate the alarm's calendar
    switch (calendar())
    {
        case CalEvent::ACTIVE:
            break;
        default:
            mRestoreError = ERR_PROG;
            return nullptr;
    }

    return UndoDelete::restore();
}

/******************************************************************************
* Create a redo item to archive the alarm again.
*/
UndoItem* UndoDeactivate::createRedo(const KAEvent& event, const Resource& resource)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoReactivate(t, event, resource, mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoDeactivate::defaultActionText() const
{
    return i18nc("@info", "Reactivate alarm");
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoDeactivate::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoDeactivate");
#endif
}


/*=============================================================================
=  Class: UndoReactivates
=  Undo item for multiple alarm reactivation.
=============================================================================*/

/******************************************************************************
* Create a redo item to reactivate the alarms again.
*/
UndoItem* UndoReactivates::createRedo(Undo::List* undos)
{
    const Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
    return new UndoReactivates(t, undos, mName);
}

/******************************************************************************
* Return the action description of the Undo item for displaying.
*/
QString UndoReactivates::defaultActionText() const
{
    return i18nc("@info", "Reactivate multiple alarms");
}

/******************************************************************************
* Dump the instance's contents to debug.
*/
void UndoReactivates::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    dumpDebugTitle("UndoReactivates");
#endif
}


/*=============================================================================
=  Class: Event
=  Event details for external calls.
=============================================================================*/
Undo::Event::Event(const KAEvent& e, const Resource& r)
    : event(e)
    , resource(r)
{
    if (e.category() == CalEvent::ACTIVE)
        dontShowErrors = KAlarm::dontShowErrors(EventId(e));
}

#include "moc_undo.cpp"

// vim: et sw=4:
