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
		enum Operation { ADD, EDIT, DELETE, MULTI };
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
		virtual UndoItem* restore() = 0;
		virtual bool      deleteID(const QString& id)  { return false; }

		enum Error { ERR_NONE, ERR_PROG, ERR_NOT_FOUND, ERR_CREATE };
		static int        mLastId;
		static Error      mRestoreError;   // error code valid only if restore() returns 0

	protected:
		UndoItem(Undo::Type);
		int               mId;     // unique identifier (only for mType = UNDO, REDO)
		Undo::Type        mType;   // which list (if any) the object is in
};

class UndoAdd : public UndoItem
{
	public:
		UndoAdd(Undo::Type, const QString& eventID);
		virtual Operation operation() const     { return ADD; }
		virtual QString   description() const;
		virtual QString   eventID() const       { return mEventID; }
		virtual QString   newEventID() const    { return mEventID; }
		virtual UndoItem* restore();
	private:
		QString  mEventID;
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
	private:
		KAEvent*  mEvent;
};

class UndoDeletes : public UndoItem
{
	public:
		UndoDeletes(Undo::Type, const QValueList<KAEvent>&);
		UndoDeletes(Undo::Type, Undo::List&);
		~UndoDeletes();
		virtual Operation operation() const     { return MULTI; }
		virtual QString   description() const;
		virtual UndoItem* restore();
		virtual bool      deleteID(const QString& id);
	private:
		Undo::List  mUndos;    // this list must always have > 1 entry
};

static bool getCurrentEvent(const QString& id, KAEvent::Status, KAEvent& result);


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
void Undo::saveAdd(const QString& eventID)
{
	new UndoAdd(UNDO, eventID);
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

/******************************************************************************
*  Remove any redos which are made invalid by a new undo.
*/
void Undo::removeRedos(const QString& eventID)
{
	QString id = eventID;
	for (Iterator it = mRedoList.begin();  it != mRedoList.end();  )
	{
		UndoItem* item = *it;
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
				id = item->newEventID();     // continue looking for its post-edit ID
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


/*=============================================================================
=  Class: UndoAdd
=  Undo item for alarm creation.
=============================================================================*/

UndoAdd::UndoAdd(Undo::Type type, const QString& eventID)
	: UndoItem(type),
	  mEventID(eventID)
{ }

/******************************************************************************
*  Undo the item, i.e. delete the alarm which was added.
*  Create a redo item to add the alarm back again.
*  Reply = redo item.
*/
UndoItem* UndoAdd::restore()
{
	// Retrieve the current state of the alarm
	KAEvent::Status status = KAEvent::uidStatus(mEventID);
	KAEvent event;
	if (!getCurrentEvent(mEventID, status, event))
	{
		mRestoreError = ERR_NOT_FOUND;
		return 0;
	}

	// Create a redo item to recreate the alarm
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	UndoItem* undo = new UndoDelete(t, event);     // 'event' gets modified by KAlarm::deleteEvent()

	switch (status)
	{
		case KAEvent::ACTIVE:
			KAlarm::deleteEvent(event, true);   // archive it if it has already triggered
			break;
		case KAEvent::TEMPLATE:
			KAlarm::deleteTemplate(event);
			break;
		case KAEvent::EXPIRED:
			break;
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
QString UndoAdd::description() const
{
	switch (KAEvent::uidStatus(mEventID))
	{
		case KAEvent::ACTIVE:
			return i18n("Action to create a new alarm", "New alarm");
		case KAEvent::TEMPLATE:
			return i18n("Action to create a new alarm template", "New template");
		default:
			break;
	}
	return QString::null;
}


/*=============================================================================
=  Class: UndoEdit
=  Undo item for alarm edit.
=============================================================================*/

UndoEdit::UndoEdit(Undo::Type type, const KAEvent& oldEvent, const QString& newEventID)
	: UndoItem(type),
	  mOldEvent(new KAEvent(oldEvent)),
	  mNewEventID(newEventID)
{ }

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
	KAEvent::Status status = KAEvent::uidStatus(mNewEventID);
	KAEvent newEvent;
	if (!getCurrentEvent(mNewEventID, status, newEvent))
	{
		mRestoreError = ERR_NOT_FOUND;
		return 0;
	}

	// Create a redo item to restore the edit
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	UndoItem* undo = new UndoEdit(t, newEvent, mOldEvent->id());

	switch (status)
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
	switch (KAEvent::uidStatus(mNewEventID))
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
{ }

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
	QString id = mEvent->id();
	switch (KAEvent::uidStatus(id))
	{
		case KAEvent::ACTIVE:
			if (!KAlarm::addEvent(*mEvent, 0, true))
			{
				mRestoreError = ERR_CREATE;
				return 0;
			}
			if (mEvent->toBeArchived())
			{
				// Now that is has been restored to the active calendar,
				// remove it from the archive calendar
				mEvent->setUid(KAEvent::EXPIRED);
				KAlarm::deleteEvent(*mEvent, false);
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
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoAdd(t, id);
}

/******************************************************************************
*  Return a description of the Undo item for displaying.
*/
QString UndoDelete::description() const
{
	switch (KAEvent::uidStatus(mEvent->id()))
	{
		case KAEvent::ACTIVE:
			return i18n("Action to delete an alarm", "Delete alarm");
		case KAEvent::TEMPLATE:
			return i18n("Action to delete an alarm template", "Delete template");
		case KAEvent::EXPIRED:
			return i18n("Delete expired alarm");
		default:
			break;
	}
	return QString::null;
}


/*=============================================================================
=  Class: UndoDeletes
=  Undo item for multiple alarm deletion.
=============================================================================*/

UndoDeletes::UndoDeletes(Undo::Type type, const QValueList<KAEvent>& events)
	: UndoItem(type)    // UNDO only
{
	for (QValueList<KAEvent>::ConstIterator it = events.begin();  it != events.end();  ++it)
		mUndos.append(new UndoDelete(Undo::NONE, *it));
}

UndoDeletes::UndoDeletes(Undo::Type type, Undo::List& undos)
	: UndoItem(type),
	  mUndos(undos)
{
}

UndoDeletes::~UndoDeletes()
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
UndoItem* UndoDeletes::restore()
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
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDeletes(t, newUndos);
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
		switch (KAEvent::uidStatus((*it)->eventID()))
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

/******************************************************************************
*  If one of the multiple items has the specified ID, delete it.
*  If an item is deleted and there is only one item left, the UndoDeletes
*  instance is removed from its list and replaced by an UndoDelete/UndoAdd instead.
*  Reply = true if this instance was replaced. The caller must delete it.
*        = false otherwise.
*/
bool UndoDeletes::deleteID(const QString& id)
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
				Undo::replace(this, item);
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
=  Local declarations.
=============================================================================*/

/******************************************************************************
*  Fetch the current state of the alarm with a given ID.
*/
static bool getCurrentEvent(const QString& id, KAEvent::Status status, KAEvent& event)
{
	AlarmCalendar* cal = 0;
	switch (status)
	{
		case KAEvent::ACTIVE:
			cal = AlarmCalendar::activeCalendar();
			break;
		case KAEvent::TEMPLATE:
			cal = AlarmCalendar::templateCalendarOpen();
			break;
		default:
			return false;
	}
	if (!cal)
		return false;
	KCal::Event* kcalEvent = cal->event(id);
	if (!kcalEvent)
		return false;

	// The alarm is still in the calendar
	event.set(*kcalEvent); 
	return true;
}

#endif
