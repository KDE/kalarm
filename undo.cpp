/*
 *  undo.cpp  -  edit undo facility
 *  Program:  kalarm
 *  (C) 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"

#ifdef UNDO_FEATURE
#include <qobject.h>
#include <qstringlist.h>

#include <kapplication.h>
#include <klocale.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "functions.h"
#include "undo.moc"

static int maxCount = 12;


class UndoItem
{
	public:
		enum Operation { ADD, EDIT, DELETE, REACTIVATE, DEACTIVATE, MULTI };
		UndoItem();           // needed by QValueList
		virtual ~UndoItem();
		virtual Operation operation() const = 0;
		virtual QString   description() const = 0;
		virtual QString   tooltip() const       { return QString::null; }
		virtual QString   eventID() const       { return QString::null; }
		virtual QString   oldEventID() const    { return QString::null; }
		virtual QString   newEventID() const    { return QString::null; }
		int               id() const            { return mId; }
		Undo::Type        type() const          { return mType; }
		void              setType(Undo::Type t) { mType = t; }
		KAEvent::Status   calendar() const      { return mCalendar; }
		virtual void      setCalendar(KAEvent::Status s) { mCalendar = s; }
		virtual UndoItem* restore() = 0;
		virtual bool      deleteID(const QString& /*id*/)  { return false; }

		enum Error { ERR_NONE, ERR_PROG, ERR_NOT_FOUND, ERR_CREATE, ERR_EXPIRED };
		static int        mLastId;
		static Error      mRestoreError;   // error code valid only if restore() returns 0

	protected:
		UndoItem(Undo::Type);
		static QString    addDeleteDescription(KAEvent::Status, bool add);
		void              replaceWith(UndoItem* item)   { Undo::replace(this, item); }

		int               mId;     // unique identifier (only for mType = UNDO, REDO)
		Undo::Type        mType;   // which list (if any) the object is in
		KAEvent::Status   mCalendar;
};

class UndoAdd : public UndoItem
{
	public:
		UndoAdd(Undo::Type, const KAEvent&);
		UndoAdd(Undo::Type, const QString& eventID, KAEvent::Status);
		virtual Operation operation() const     { return ADD; }
		virtual QString   description() const;
		virtual QString   tooltip() const       { return mTooltip; }
		virtual QString   eventID() const       { return mEventID; }
		virtual QString   newEventID() const    { return mEventID; }
		virtual UndoItem* restore()             { return doRestore(); }
	protected:
		UndoItem*         doRestore(bool setArchive = false);
		virtual UndoItem* createRedo(const KAEvent&);
	private:
		QString  mEventID;
		QString  mTooltip;
};

class UndoEdit : public UndoItem
{
	public:
		UndoEdit(Undo::Type, const KAEvent& oldEvent, const QString& newEventID);
		~UndoEdit();
		virtual Operation operation() const     { return EDIT; }
		virtual QString   description() const;
		virtual QString   eventID() const       { return mNewEventID; }
		virtual QString   oldEventID() const    { return mOldEvent->id(); }
		virtual QString   newEventID() const    { return mNewEventID; }
		virtual UndoItem* restore();
	private:
		KAEvent*  mOldEvent;
		QString   mNewEventID;
};

class UndoDelete : public UndoItem
{
	public:
		UndoDelete(Undo::Type, const KAEvent&);
		~UndoDelete();
		virtual Operation operation() const     { return DELETE; }
		virtual QString   description() const;
		virtual QString   eventID() const       { return mEvent->id(); }
		virtual QString   oldEventID() const    { return mEvent->id(); }
		virtual UndoItem* restore();
		KAEvent*  event() const                 { return mEvent; }
	protected:
		virtual UndoItem* createRedo(const KAEvent&);
	private:
		KAEvent*  mEvent;
};

class UndoReactivate : public UndoAdd
{
	public:
		UndoReactivate(Undo::Type t, const KAEvent& e)  : UndoAdd(t, e.id(), KAEvent::ACTIVE) { }
		virtual Operation operation() const     { return REACTIVATE; }
		virtual QString   description() const;
		virtual UndoItem* restore();
	protected:
		virtual UndoItem* createRedo(const KAEvent&);
};

class UndoDeactivate : public UndoDelete
{
	public:
		UndoDeactivate(Undo::Type t, const KAEvent& e)  : UndoDelete(t, e) { }
		virtual Operation operation() const     { return DEACTIVATE; }
		virtual QString   description() const;
		virtual UndoItem* restore();
	protected:
		virtual UndoItem* createRedo(const KAEvent&);
};

template <class T> class UndoMulti : public UndoItem
{
	public:
		UndoMulti(Undo::Type, const QValueList<KAEvent>&);
		UndoMulti(Undo::Type, Undo::List&);
		~UndoMulti();
		virtual Operation operation() const     { return MULTI; }
		virtual UndoItem* restore();
		virtual bool      deleteID(const QString& id);
		virtual UndoItem* createRedo(Undo::List&) = 0;
	protected:
		Undo::List  mUndos;    // this list must always have > 1 entry
};

class UndoDeletes : public UndoMulti<UndoDelete>
{
	public:
		UndoDeletes(Undo::Type t, const QValueList<KAEvent>& events)
		                  : UndoMulti<UndoDelete>(t, events) { }   // UNDO only
		UndoDeletes(Undo::Type t, Undo::List& undos)
		                  : UndoMulti<UndoDelete>(t, undos) { }
		virtual QString   description() const;
		virtual UndoItem* createRedo(Undo::List&);
};

class UndoReactivates : public UndoMulti<UndoReactivate>
{
	public:
		UndoReactivates(Undo::Type t, const QValueList<KAEvent>& events)
		                  : UndoMulti<UndoReactivate>(t, events) { }   // UNDO only
		UndoReactivates(Undo::Type t, Undo::List& undos)
		                  : UndoMulti<UndoReactivate>(t, undos) { }
		virtual QString   description() const;
		virtual UndoItem* createRedo(Undo::List&);
};

Undo*       Undo::mInstance = 0;
Undo::List  Undo::mUndoList;
Undo::List  Undo::mRedoList;


/******************************************************************************
*  Create the one and only instance of the Undo class.
*/
Undo* Undo::instance()
{
	if (!mInstance)
		mInstance = new Undo(kapp);
	return mInstance;
}

/******************************************************************************
*  Clear the lists of undo and redo items.
*/
void Undo::clear()
{
	if (!mUndoList.isEmpty()  ||  !mRedoList.isEmpty())
	{
		mInstance->blockSignals(true);
		while (mUndoList.count())
			delete mUndoList.first();    // N.B. 'delete' removes the object from the list
		while (mRedoList.count())
			delete mRedoList.first();    // N.B. 'delete' removes the object from the list
		mInstance->blockSignals(false);
		emitChanged();
	}
}

/******************************************************************************
*  Create an undo item and add it to the list of undos.
*  N.B. The base class constructor adds the object to the undo list.
*/
void Undo::saveAdd(const KAEvent& event)
{
	new UndoAdd(UNDO, event);
	emitChanged();
}

void Undo::saveEdit(const KAEvent& oldEvent, const QString& newEventID)
{
	new UndoEdit(UNDO, oldEvent, newEventID);
	removeRedos(oldEvent.id());    // remove any redos which are made invalid by this edit
	emitChanged();
}

void Undo::saveDelete(const KAEvent& event)
{
	new UndoDelete(UNDO, event);
	removeRedos(event.id());    // remove any redos which are made invalid by this deletion
	emitChanged();
}

void Undo::saveDeletes(const QValueList<KAEvent>& events)
{
	int count = events.count();
	if (count == 1)
		saveDelete(events.first());
	else if (count > 1)
	{
		new UndoDeletes(UNDO, events);
		for (QValueList<KAEvent>::ConstIterator it = events.begin();  it != events.end();  ++it)
			removeRedos((*it).id());    // remove any redos which are made invalid by these deletions
		emitChanged();
	}
}

void Undo::saveReactivate(const KAEvent& event)
{
	new UndoReactivate(UNDO, event);
	emitChanged();
}

void Undo::saveReactivates(const QValueList<KAEvent>& events)
{
	int count = events.count();
	if (count == 1)
		saveReactivate(events.first());
	else if (count > 1)
	{
		new UndoReactivates(UNDO, events);
		emitChanged();
	}
}

/******************************************************************************
*  Remove any redos which are made invalid by a new undo.
*/
void Undo::removeRedos(const QString& eventID)
{
	QString id = eventID;
	for (Iterator it = mRedoList.begin();  it != mRedoList.end();  )
	{
		UndoItem* item = *it;
kdDebug(5950)<<"removeRedos(): "<<item->eventID()<<" (looking for "<<id<<")"<<endl;
		if (item->operation() == UndoItem::MULTI)
		{
			if (item->deleteID(id))
			{
				// The old multi-redo was replaced with a new single redo
				delete item;
			}
			++it;
		}
		else if (item->eventID() == id)
		{
			if (item->operation() == UndoItem::EDIT)
				id = item->oldEventID();   // continue looking for its post-edit ID
			item->setType(NONE);    // prevent the destructor removing it from the list
			delete item;
			it = mRedoList.remove(it);
		}
		else
			++it;
	}
}

/******************************************************************************
*  Undo or redo a specified item.
*  Reply = error message
*        = QString::null if success, or it the item no longer exists.
*/
QString Undo::undo(Undo::Iterator it, Undo::Type type)
{
	QString err;
	if (it != mUndoList.end()  &&  it != mRedoList.end()  &&  (*it)->type() == type)
	{
		if (!(*it)->restore())
		{
			switch (UndoItem::mRestoreError)
			{
				case UndoItem::ERR_NONE:       break;
				case UndoItem::ERR_NOT_FOUND:  err = i18n("Alarm not found");  break;
				case UndoItem::ERR_CREATE:     err = i18n("Error recreating alarm");  break;
				case UndoItem::ERR_EXPIRED:    err = i18n("Cannot reactivate expired alarm");  break;
				case UndoItem::ERR_PROG:       err = i18n("Program error");  break;
				default:                       err = i18n("Unknown error");  break;
			}
		}
		delete *it;    // N.B. 'delete' removes the object from its list
		emitChanged();
	}
	return err;
}

/******************************************************************************
*  Add an undo item to the start of one of the lists.
*/
void Undo::add(UndoItem* item, bool undo)
{
	if (item)
	{
		// Limit the number of items stored
		int undoCount = mUndoList.count();
		int redoCount = mRedoList.count();
		if (undoCount + redoCount >= maxCount - 1)
		{
			if (undoCount)
				mUndoList.pop_back();
			else
				mRedoList.pop_back();
		}

		// Append the new item
		List* list = undo ? &mUndoList : &mRedoList;
		list->prepend(item);
	}
}

/******************************************************************************
*  Remove an undo item from one of the lists.
*/
void Undo::remove(UndoItem* item, bool undo)
{
	List* list = undo ? &mUndoList : &mRedoList;
	if (!list->isEmpty())
		list->remove(item);
}

/******************************************************************************
*  Replace an undo item in one of the lists.
*/
void Undo::replace(UndoItem* old, UndoItem* New)
{
	Type type = old->type();
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	if (!list)
		return;
	Iterator it = list->find(old);
	if (it != list->end())
	{
		New->setType(type);    // ensure the item points to the correct list
		*it = New;
		old->setType(NONE);    // mark the old item as no longer being in a list
	}
}

/******************************************************************************
*  Return the description of the latest undo/redo item.
*/
QString Undo::description(Undo::Type type)
{
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	return (list && !list->isEmpty()) ? list->first()->description() : QString::null;
}

/******************************************************************************
*  Return the description of the undo/redo item with the specified ID.
*/
QString Undo::description(Undo::Type type, int id)
{
	UndoItem* undo = getItem(id, type);
	return undo ? undo->description() : QString::null;
}

/******************************************************************************
*  Return the description of the undo/redo item with the specified ID.
*/
QString Undo::tooltip(Undo::Type type, int id)
{
	UndoItem* undo = getItem(id, type);
	return undo ? undo->tooltip() : QString::null;
}

/******************************************************************************
*  Return the descriptions of all undo or redo items, in order latest first.
*  If 'ids' is non-null, also returns a list of their corresponding IDs.
*/
QValueList<int> Undo::ids(Undo::Type type)
{
	QValueList<int> ids;
int n=0;
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	if (list)
	{
		for (Iterator it = list->begin();  it != list->end();  ++it)
{++n;
			ids.append((*it)->id());
}
	}
kdDebug(5950)<<"Undo::ids(): "<<n<<" -> "<<ids.count()<<endl;
	return ids;
}

/******************************************************************************
*  Emit the appropriate 'changed' signal.
*/
void Undo::emitChanged()
{
	if (mInstance)
		mInstance->emitChanged(description(UNDO), description(REDO));
}

/******************************************************************************
*  Return the item with the specified ID.
*/
UndoItem* Undo::getItem(int id, Undo::Type type)
{
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	if (list)
	{
		for (Iterator it = list->begin();  it != list->end();  ++it)
		{
			if ((*it)->id() == id)
				return *it;
		}
	}
	return 0;
}

/******************************************************************************
*  Find an item with the specified ID.
*/
Undo::Iterator Undo::findItem(int id, Undo::Type type)
{
	List* list = (type == UNDO) ? &mUndoList : &mRedoList;
	Iterator it;
	for (it = list->begin();  it != list->end();  ++it)
	{
		if ((*it)->id() == id)
			break;
	}
	return it;
}


/*=============================================================================
=  Class: UndoItem
=  A single undo action.
=============================================================================*/
int             UndoItem::mLastId = 0;
UndoItem::Error UndoItem::mRestoreError;

/******************************************************************************
*  Constructor.
*  Optionally appends the undo to the list of undos.
*/
UndoItem::UndoItem(Undo::Type type)
	: mId(0),
	  mType(type)
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
*  Destructor.
*  Removes the undo from the list (if it's in the list).
*/
UndoItem::~UndoItem()
{
	if (mType != Undo::NONE)
		Undo::remove(this, (mType == Undo::UNDO));
}

/******************************************************************************
*  Return a description of an add or delete Undo/Redo item for displaying.
*/
QString UndoItem::addDeleteDescription(KAEvent::Status calendar, bool add)
{
	switch (calendar)
	{
		case KAEvent::ACTIVE:
			if (add)
				return i18n("Action to create a new alarm", "New alarm");
			else
				return i18n("Action to delete an alarm", "Delete alarm");
		case KAEvent::TEMPLATE:
			if (add)
				return i18n("Action to create a new alarm template", "New template");
			else
				return i18n("Action to delete an alarm template", "Delete template");
		case KAEvent::EXPIRED:
			return i18n("Delete expired alarm");
		default:
			break;
	}
	return QString::null;
}


/*=============================================================================
=  Class: UndoMulti
=  Undo item for multiple alarms.
=============================================================================*/

template <class T>
UndoMulti<T>::UndoMulti(Undo::Type type, const QValueList<KAEvent>& events)
	: UndoItem(type)    // UNDO only
{
	for (QValueList<KAEvent>::ConstIterator it = events.begin();  it != events.end();  ++it)
		mUndos.append(new T(Undo::NONE, *it));
}

template <class T>
UndoMulti<T>::UndoMulti(Undo::Type type, Undo::List& undos)
	: UndoItem(type),
	  mUndos(undos)
{ }

template <class T>
UndoMulti<T>::~UndoMulti()
{
	for (Undo::List::Iterator it = mUndos.begin();  it != mUndos.end();  ++it)
		delete *it;
}

/******************************************************************************
*  Undo the item, i.e. restore multiple alarms which were deleted (or delete
*  alarms which were restored).
*  Create a redo item to delete (or restore) the alarms again.
*  Reply = redo item.
*/
template <class T>
UndoItem* UndoMulti<T>::restore()
{
	Undo::List newUndos;
	for (Undo::List::Iterator it = mUndos.begin();  it != mUndos.end();  ++it)
	{
		UndoItem* undo = (*it)->restore();
		if (undo)
			newUndos.append(undo);
	}
	if (newUndos.isEmpty())
		return 0;

	// Create a redo item to delete the alarm again
	return createRedo(newUndos);
}

/******************************************************************************
*  If one of the multiple items has the specified ID, delete it.
*  If an item is deleted and there is only one item left, the UndoMulti
*  instance is removed from its list and replaced by the remaining UndoItem instead.
*  Reply = true if this instance was replaced. The caller must delete it.
*        = false otherwise.
*/
template <class T>
bool UndoMulti<T>::deleteID(const QString& id)
{
	for (Undo::List::Iterator it = mUndos.begin();  it != mUndos.end();  ++it)
	{
		if ((*it)->id() == id)
		{
			// Found a matching entry - remove it
			UndoItem* item = *it;
			mUndos.remove(it);
			if (mUndos.count() == 1)
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

UndoAdd::UndoAdd(Undo::Type type, const KAEvent& event)
	: UndoItem(type),
	  mEventID(event.id()),
#warning Tooltip here
	  mTooltip()
{
	setCalendar(KAEvent::uidStatus(mEventID));
}

UndoAdd::UndoAdd(Undo::Type type, const QString& eventID, KAEvent::Status calendar)
	: UndoItem(type),
	  mEventID(KAEvent::uid(eventID, calendar)),
#warning Tooltip here
	  mTooltip()
{
	setCalendar(calendar);
}

/******************************************************************************
*  Undo the item, i.e. delete the alarm which was added.
*  Create a redo item to add the alarm back again.
*  Reply = redo item.
*/
UndoItem* UndoAdd::doRestore(bool setArchive)
{
	// Retrieve the current state of the alarm
	const KCal::Event* kcalEvent = AlarmCalendar::getEvent(mEventID);
	if (!kcalEvent)
	{
		mRestoreError = ERR_NOT_FOUND;    // alarm is no longer in calendar
		return 0;
	}
	KAEvent event(*kcalEvent); 

	// Create a redo item to recreate the alarm.
	// Do it now, since 'event' gets modified by KAlarm::deleteEvent()
	UndoItem* undo = createRedo(event);

	switch (calendar())
	{
		case KAEvent::ACTIVE:
			if (setArchive)
				event.setArchive();
			KAlarm::deleteEvent(event, true);   // archive it if it has already triggered
			break;
		case KAEvent::TEMPLATE:
			KAlarm::deleteTemplate(event);
			break;
		case KAEvent::EXPIRED:    // redoing the deletion of an expired alarm
			KAlarm::deleteEvent(event);
			break;
		default:
			delete undo;
			mRestoreError = ERR_PROG;
			return 0;
	}
	return undo;
}

/******************************************************************************
*  Create a redo item to add the alarm back again.
*/
UndoItem* UndoAdd::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDelete(t, event);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoAdd::description() const
{
	return addDeleteDescription(calendar(), (type() == Undo::UNDO));
}


/*=============================================================================
=  Class: UndoEdit
=  Undo item for alarm edit.
=============================================================================*/

UndoEdit::UndoEdit(Undo::Type type, const KAEvent& oldEvent, const QString& newEventID)
	: UndoItem(type),
	  mOldEvent(new KAEvent(oldEvent)),
	  mNewEventID(newEventID)
{
	setCalendar(KAEvent::uidStatus(mNewEventID));
}

UndoEdit::~UndoEdit()
{
	delete mOldEvent;
}

/******************************************************************************
*  Undo the item, i.e. undo an edit to a previously existing alarm.
*  Create a redo item to reapply the edit.
*  Reply = redo item.
*/
UndoItem* UndoEdit::restore()
{
	// Retrieve the current state of the alarm
	const KCal::Event* kcalEvent = AlarmCalendar::getEvent(mNewEventID);
	if (!kcalEvent)
	{
		mRestoreError = ERR_NOT_FOUND;    // alarm is no longer in calendar
		return 0;
	}
	KAEvent newEvent(*kcalEvent); 

	// Create a redo item to restore the edit
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	UndoItem* undo = new UndoEdit(t, newEvent, mOldEvent->id());

	switch (calendar())
	{
		case KAEvent::ACTIVE:
			KAlarm::modifyEvent(newEvent, *mOldEvent, 0);
			break;
		case KAEvent::TEMPLATE:
			KAlarm::updateTemplate(*mOldEvent, 0);
			break;
		case KAEvent::EXPIRED:    // editing of expired events is not allowed
		default:
			delete undo;
			mRestoreError = ERR_PROG;
			return 0;
	}
	return undo;
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoEdit::description() const
{
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			return i18n("Action to edit an alarm", "Edit alarm");
		case KAEvent::TEMPLATE:
			return i18n("Action to edit an alarm template", "Edit template");
		default:
			break;
	}
	return QString::null;
}


/*=============================================================================
=  Class: UndoDelete
=  Undo item for alarm deletion.
=============================================================================*/

UndoDelete::UndoDelete(Undo::Type type, const KAEvent& event)
	: UndoItem(type),
	  mEvent(new KAEvent(event))
{
	setCalendar(KAEvent::uidStatus(mEvent->id()));
}

UndoDelete::~UndoDelete()
{
	delete mEvent;
}

/******************************************************************************
*  Undo the item, i.e. restore an alarm which was deleted.
*  Create a redo item to delete the alarm again.
*  Reply = redo item.
*/
UndoItem* UndoDelete::restore()
{
	// Restore the original event
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			if (mEvent->toBeArchived())
			{
				// It was archived when it was deleted
				mEvent->setUid(KAEvent::EXPIRED);
				if (!KAlarm::undeleteEvent(*mEvent, 0, true))
				{
					mRestoreError = ERR_EXPIRED;
					return 0;
				}
			}
			else
			{
				if (!KAlarm::addEvent(*mEvent, 0, true))
				{
					mRestoreError = ERR_CREATE;
					return 0;
				}
			}
			break;
		case KAEvent::TEMPLATE:
			if (!KAlarm::addTemplate(*mEvent, 0))
			{
				mRestoreError = ERR_CREATE;
				return 0;
			}
			break;
		case KAEvent::EXPIRED:
			if (!KAlarm::addExpiredEvent(*mEvent))
			{
				mRestoreError = ERR_CREATE;
				return 0;
			}
			break;
		default:
			mRestoreError = ERR_PROG;
			return 0;
	}

	// Create a redo item to delete the alarm again
	return createRedo(*mEvent);
}

/******************************************************************************
*  Create a redo item to archive the alarm again.
*/
UndoItem* UndoDelete::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoAdd(t, event);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoDelete::description() const
{
	return addDeleteDescription(calendar(), (type() == Undo::REDO));
}


/*=============================================================================
=  Class: UndoDeletes
=  Undo item for multiple alarm deletion.
=============================================================================*/

/******************************************************************************
*  Create a redo item to delete the alarms again.
*/
UndoItem* UndoDeletes::createRedo(Undo::List& undos)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDeletes(t, undos);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoDeletes::description() const
{
	if (mUndos.isEmpty())
		return QString::null;
	for (Undo::List::ConstIterator it = mUndos.begin();  it != mUndos.end();  ++it)
	{
		switch ((*it)->calendar())
		{
			case KAEvent::ACTIVE:
				return i18n("Delete multiple alarms");
			case KAEvent::TEMPLATE:
				return i18n("Delete multiple templates");
			case KAEvent::EXPIRED:
				break;    // check if they are ALL expired
			default:
				return QString::null;
		}
	}
	return i18n("Delete multiple expired alarms");
}


/*=============================================================================
=  Class: UndoReactivate
=  Undo item for alarm reactivation.
=============================================================================*/

/******************************************************************************
*  Undo the item, i.e. re-archive the alarm which was reactivated.
*  Create a redo item to reactivate the alarm back again.
*  Reply = redo item.
*/
UndoItem* UndoReactivate::restore()
{
	// Validate the alarm's calendar
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			break;
		default:
			mRestoreError = ERR_PROG;
			return 0;
	}
	return UndoAdd::doRestore(true);     // restore alarm, ensuring that it is re-archived
}

/******************************************************************************
*  Create a redo item to add the alarm back again.
*/
UndoItem* UndoReactivate::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDeactivate(t, event);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoReactivate::description() const
{
	return i18n("Reactivate alarm");
}


/*=============================================================================
=  Class: UndoDeactivate
=  Redo item for alarm reactivation.
=============================================================================*/

/******************************************************************************
*  Undo the item, i.e. reactivate an alarm which was archived.
*  Create a redo item to archive the alarm again.
*  Reply = redo item.
*/
UndoItem* UndoDeactivate::restore()
{
	// Validate the alarm's calendar
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			break;
		default:
			mRestoreError = ERR_PROG;
			return 0;
	}

	return UndoDelete::restore();
}

/******************************************************************************
*  Create a redo item to archive the alarm again.
*/
UndoItem* UndoDeactivate::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoReactivate(t, event);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoDeactivate::description() const
{
	return i18n("Reactivate alarm");
}


/*=============================================================================
=  Class: UndoReactivates
=  Undo item for multiple alarm reactivation.
=============================================================================*/

/******************************************************************************
*  Create a redo item to reactivate the alarms again.
*/
UndoItem* UndoReactivates::createRedo(Undo::List& undos)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoReactivates(t, undos);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoReactivates::description() const
{
	return i18n("Reactivate multiple alarms");
}

#endif
