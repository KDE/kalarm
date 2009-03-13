/*
 *  functions.cpp  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "functions.h"

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "eventlistmodel.h"
#include "alarmlistview.h"
#include "alarmresources.h"
#include "editdlg.h"
#include "identities.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "preferences.h"
#include "shellprocess.h"
#include "templatelistview.h"
#include "templatemenuaction.h"

#include <QDir>
#include <QRegExp>
#include <QDesktopWidget>
#include <QtDBus/QtDBus>

#include <kconfiggroup.h>
#include <kaction.h>
#include <ktoggleaction.h>
#include <kactioncollection.h>
#include <kdbusservicestarter.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <ksystemtimezone.h>
#include <KStandardGuiItem>
#include <kstandardshortcut.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kicon.h>
#include <kdebug.h>

#include <kcal/event.h>
#include <kcal/icalformat.h>
#include <kpimidentities/identitymanager.h>
#include <kpimidentities/identity.h>
#include <kcal/person.h>
#include <kholidays/holidays.h>
#include <ktoolinvocation.h>


namespace
{
bool          refreshAlarmsQueued = false;
QString       korganizerName    = "korganizer";
QString       korgStartError;
QDBusInterface* korgInterface = 0;

const char*   KMAIL_DBUS_SERVICE      = "org.kde.kmail";
//const char*   KMAIL_DBUS_IFACE        = "org.kde.kmail.kmail";
const char*   KMAIL_DBUS_WINDOW_PATH  = "/kmail/kmail_mainwindow_1";
const char*   KORG_DBUS_SERVICE       = "org.kde.korganizer";
const char*   KORG_DBUS_IFACE         = "org.kde.korganizer.Korganizer";
// D-Bus object path of KOrganizer's notification interface
#define       KORG_DBUS_PATH            "/Korganizer"
#define       KORG_DBUS_LOAD_PATH       "/korganizer_PimApplication"
const char*   KORG_DBUS_WINDOW_PATH   = "/korganizer/MainWindow_1";
const QString KORGANIZER_UID         = QString::fromLatin1("-korg");

const char*   ALARM_OPTS_FILE        = "alarmopts";
const char*   DONT_SHOW_ERRORS_GROUP = "DontShowErrors";

void editNewTemplate(EditAlarmDlg::Type, const KAEvent* preset, QWidget* parent);
KAlarm::UpdateStatus sendToKOrganizer(const KAEvent*);
KAlarm::UpdateStatus deleteFromKOrganizer(const QString& eventID);
KAlarm::UpdateStatus runKOrganizer();
QString uidKOrganizer(const QString& eventID);
}


namespace KAlarm
{

void doEditNewAlarm(EditAlarmDlg*);


/******************************************************************************
*  Display a main window with the specified event selected.
*/
MainWindow* displayMainWindowSelected(const QString& eventID)
{
	MainWindow* win = MainWindow::firstWindow();
	if (!win)
	{
		if (theApp()->checkCalendar())    // ensure calendar is open
		{
			win = MainWindow::create();
			win->show();
		}
	}
	else
	{
		// There is already a main window, so make it the active window
		bool visible = win->isVisible();
		if (visible)
			win->hide();        // in case it's on a different desktop
		if (!visible  ||  win->isMinimized())
		{
			win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
			win->show();
		}
		win->raise();
		win->activateWindow();
	}
	if (win  &&  !eventID.isEmpty())
		win->selectEvent(eventID);
	return win;
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
KToggleAction* createAlarmEnableAction(QObject* parent)
{
	KToggleAction* action = new KToggleAction(i18nc("@action", "Enable &Alarms"), parent);
	action->setChecked(theApp()->alarmsEnabled());
	QObject::connect(action, SIGNAL(toggled(bool)), theApp(), SLOT(setAlarmsEnabled(bool)));
	// The following line ensures that all instances are kept in the same state
	QObject::connect(theApp(), SIGNAL(alarmEnabledToggled(bool)), action, SLOT(setChecked(bool)));
	return action;
}

/******************************************************************************
* Create a "Spread Windows" action.
*/
KToggleAction* createSpreadWindowsAction(QObject* parent)
{
	KToggleAction* action = new KToggleAction(i18nc("@action", "Spread Windows"), parent);
	QObject::connect(action, SIGNAL(triggered(bool)), theApp(), SLOT(spreadWindows(bool)));
	// The following line ensures that all instances are kept in the same state
	QObject::connect(theApp(), SIGNAL(spreadWindowsToggled(bool)), action, SLOT(setChecked(bool)));
	return action;
}

/******************************************************************************
* Create a New From Template QAction.
*/
TemplateMenuAction* createNewFromTemplateAction(const QString& label, KActionCollection* actions, const QString& name)
{
	return new TemplateMenuAction(KIcon(QLatin1String("document-new-from-template")), label, actions, name);
}

/******************************************************************************
* Add a new active (non-archived) alarm.
* Save it in the calendar file and add it to every main window instance.
* Parameters: msgParent = parent widget for any resource selection prompt or
* error message.
* 'event' is updated with the actual event ID.
*/
UpdateStatus addEvent(KAEvent& event, AlarmResource* resource, QWidget* msgParent, int options, bool showKOrgErr)
{
	kDebug() << event.id();
	bool cancelled = false;
	UpdateStatus status = UPDATE_OK;
	if (!theApp()->checkCalendar())    // ensure calendar is open
		status = UPDATE_FAILED;
	else
	{
		// Save the event details in the calendar file, and get the new event ID
		AlarmCalendar* cal = AlarmCalendar::resources();
		KAEvent* newev = new KAEvent(event);
		if (!cal->addEvent(newev, msgParent, (options & USE_EVENT_ID), resource, (options & NO_RESOURCE_PROMPT), &cancelled))
		{
			delete newev;
			status = UPDATE_FAILED;
		}
		else
		{
			event = *newev;   // update event ID etc.
			if (!cal->save())
				status = SAVE_FAILED;
		}
		if (status == UPDATE_OK)
		{
			if ((options & ALLOW_KORG_UPDATE)  &&  event.copyToKOrganizer())
			{
				UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the event
				if (st > status)
					status = st;
			}

			// Update the window lists
			EventListModel::alarms()->addEvent(newev);
		}
	}

	if (status != UPDATE_OK  &&  !cancelled  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_ADD, 1, 1, showKOrgErr);
	return status;
}

/******************************************************************************
* Add a list of new active (non-archived) alarms.
* Save them in the calendar file and add them to every main window instance.
* The events are updated with their actual event IDs.
*/
UpdateStatus addEvents(QList<KAEvent>& events, QWidget* msgParent, bool allowKOrgUpdate, bool showKOrgErr)
{
	kDebug() << events.count();
	if (events.isEmpty())
		return UPDATE_OK;
	int warnErr = 0;
	int warnKOrg = 0;
	UpdateStatus status = UPDATE_OK;
	AlarmResource* resource;
	if (!theApp()->checkCalendar())    // ensure calendar is open
		status = UPDATE_FAILED;
	else
	{
		resource = AlarmResources::instance()->destination(KCalEvent::ACTIVE, msgParent);
		if (!resource)
		{
			kDebug() << "No resource";
			status = UPDATE_FAILED;
		}
	}
	if (status == UPDATE_OK)
	{
		QString selectID;
		AlarmCalendar* cal = AlarmCalendar::resources();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			// Save the event details in the calendar file, and get the new event ID
			KAEvent* newev = new KAEvent(events[i]);
			if (!cal->addEvent(newev, msgParent, false, resource))
			{
				delete newev;
				status = UPDATE_ERROR;
				++warnErr;
				continue;
			}
			events[i] = *newev;   // update event ID etc.
			if (allowKOrgUpdate  &&  newev->copyToKOrganizer())
			{
				UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the event
				if (st != UPDATE_OK)
				{
					++warnKOrg;
					if (st > status)
						status = st;
				}
			}

			// Update the window lists, but not yet which item is selected
			EventListModel::alarms()->addEvent(newev);
//			selectID = newev->id();
		}
		if (warnErr == events.count())
			status = UPDATE_FAILED;
		else if (!cal->save())
		{
			status = SAVE_FAILED;
			warnErr = 0;    // everything failed
		}
	}

	if (status != UPDATE_OK  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_ADD, (warnErr ? warnErr : events.count()), warnKOrg, showKOrgErr);
	return status;
}

/******************************************************************************
* Save the event in the archived resource and adjust every main window instance.
* The event's ID is changed to an archived ID if necessary.
*/
bool addArchivedEvent(KAEvent& event, AlarmResource* resource)
{
	kDebug() << event.id();
	QString oldid = event.id();
	bool archiving = (event.category() == KCalEvent::ACTIVE);
	if (archiving  &&  !Preferences::archivedKeepDays())
		return false;   // expired alarms aren't being kept
	KAEvent* newev = new KAEvent(event);
	if (archiving)
	{
		newev->setCategory(KCalEvent::ARCHIVED);    // this changes the event ID
		newev->setSaveDateTime(KDateTime::currentUtcDateTime());   // time stamp to control purging
	}
	AlarmCalendar* cal = AlarmCalendar::resources();
	// Note that archived resources are automatically saved after changes are made
	if (!cal->addEvent(newev, 0, false, resource))
	{
		delete newev;     // failed to add to calendar - leave event in its original state
		return false;
	}
	event = *newev;   // update event ID etc.

	// Update window lists.
	// Note: updateEvent() is not used here since that doesn't trigger refiltering
	// of the alarm list, resulting in the archived event still remaining visible
	// even if archived events are supposed to be hidden.
	if (archiving)
		EventListModel::alarms()->removeEvent(oldid);
	EventListModel::alarms()->addEvent(newev);
	return true;
}

/******************************************************************************
* Add a new template.
* Save it in the calendar file and add it to every template list view.
* 'event' is updated with the actual event ID.
* Parameters: promptParent = parent widget for any resource selection prompt.
*/
UpdateStatus addTemplate(KAEvent& event, AlarmResource* resource, QWidget* msgParent)
{
	kDebug() << event.id();
	UpdateStatus status = UPDATE_OK;

	// Add the template to the calendar file
	KAEvent* newev = new KAEvent(event);
	AlarmCalendar* cal = AlarmCalendar::resources();
	if (!cal->addEvent(newev, msgParent, false, resource))
	{
		delete newev;
		status = UPDATE_FAILED;
	}
	else
	{
		event = *newev;   // update event ID etc.
		if (!cal->save())
			status = SAVE_FAILED;
		else
		{
			cal->emitEmptyStatus();

			// Update the window lists
			EventListModel::templates()->addEvent(newev);
			return UPDATE_OK;
		}
	}

	if (msgParent)
		displayUpdateError(msgParent, status, ERR_TEMPLATE, 1);
	return status;
}

/******************************************************************************
* Modify an active (non-archived) alarm in the calendar file and in every main
* window instance.
* The new event must have a different event ID from the old one.
*/
UpdateStatus modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent, bool showKOrgErr)
{
	kDebug() << oldEvent.id();

	UpdateStatus status = UPDATE_OK;
	if (!newEvent.valid())
	{
		deleteEvent(oldEvent, true);
		status = UPDATE_FAILED;
	}
	else
	{
		QString oldId = oldEvent.id();
		if (oldEvent.copyToKOrganizer())
		{
			// Tell KOrganizer to delete its old event.
			// But ignore errors, because the user could have manually
			// deleted it since KAlarm asked KOrganizer to set it up.
			deleteFromKOrganizer(oldId);
		}
		// Delete from the window lists to prevent the event's invalid
		// pointer being accessed.
		EventListModel::alarms()->removeEvent(oldId);

		// Update the event in the calendar file, and get the new event ID
		KAEvent* newev = new KAEvent(newEvent);
		AlarmCalendar* cal = AlarmCalendar::resources();
		if (!cal->modifyEvent(oldId, newev))
		{
			delete newev;
			status = UPDATE_FAILED;
		}
		else
		{
			newEvent = *newev;
			if (!cal->save())
				status = SAVE_FAILED;
			if (status == UPDATE_OK)
			{
				if (newev->copyToKOrganizer())
				{
					UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the new event
					if (st > status)
						status = st;
				}

				// Remove "Don't show error messages again" for the old alarm
				setDontShowErrors(oldId);

				// Update the window lists
				EventListModel::alarms()->addEvent(newev);
			}
		}
	}

	if (status != UPDATE_OK  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_MODIFY, 1, 1, showKOrgErr);
	return status;
}

/******************************************************************************
* Update an active (non-archived) alarm from the calendar file and from every
* main window instance.
* The new event will have the same event ID as the old one.
* The event is not updated in KOrganizer, since this function is called when an
* existing alarm is rescheduled (due to recurrence or deferral).
*/
UpdateStatus updateEvent(KAEvent& event, QWidget* msgParent, bool archiveOnDelete)
{
	kDebug() << event.id();

	if (!event.valid())
		deleteEvent(event, archiveOnDelete);
	else
	{
		// Update the event in the calendar file.
		AlarmCalendar* cal = AlarmCalendar::resources();
		KAEvent* newEvent = cal->updateEvent(event);
		if (!cal->save())
		{
			if (msgParent)
				displayUpdateError(msgParent, SAVE_FAILED, ERR_ADD, 1);
			return SAVE_FAILED;
		}

		// Update the window lists
		EventListModel::alarms()->updateEvent(newEvent);
	}
	return UPDATE_OK;
}

/******************************************************************************
* Update a template in the calendar file and in every template list view.
* If 'selectionView' is non-null, the selection highlight is moved to the
* updated event in that listView instance.
*/
UpdateStatus updateTemplate(KAEvent& event, QWidget* msgParent)
{
	AlarmCalendar* cal = AlarmCalendar::resources();
	KAEvent* newEvent = cal->updateEvent(event);
	UpdateStatus status = UPDATE_OK;
	if (!newEvent)
		status = UPDATE_FAILED;
	else if (!cal->save())
		status = SAVE_FAILED;
	if (status != UPDATE_OK)
	{
		if (msgParent)
			displayUpdateError(msgParent, SAVE_FAILED, ERR_TEMPLATE, 1);
		return status;
	}

	EventListModel::templates()->updateEvent(newEvent);
	return UPDATE_OK;
}

/******************************************************************************
* Delete alarms from the calendar file and from every main window instance.
* If the events are archived, the events' IDs are changed to archived IDs if necessary.
*/
UpdateStatus deleteEvent(KAEvent& event, bool archive, QWidget* msgParent, bool showKOrgErr)
{
	KAEvent::List events;
	events += &event;
	return deleteEvents(events, archive, msgParent, showKOrgErr);
}

UpdateStatus deleteEvents(KAEvent::List& events, bool archive, QWidget* msgParent, bool showKOrgErr)
{
	kDebug() << events.count();
	if (events.isEmpty())
		return UPDATE_OK;
	int warnErr = 0;
	int warnKOrg = 0;
	UpdateStatus status = UPDATE_OK;
	AlarmCalendar* cal = AlarmCalendar::resources();
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		// Save the event details in the calendar file, and get the new event ID
		KAEvent* event = events[i];
		QString id = event->id();

		// Update the window lists and clear stored command errors
		EventListModel::alarms()->removeEvent(id);
		event->setCommandError(KAEvent::CMD_NO_ERROR);

		// Delete the event from the calendar file
		if (event->category() != KCalEvent::ARCHIVED)
		{
			if (event->copyToKOrganizer())
			{
				// The event was shown in KOrganizer, so tell KOrganizer to
				// delete it. But ignore errors, because the user could have
				// manually deleted it from KOrganizer since it was set up.
				UpdateStatus st = deleteFromKOrganizer(id);
				if (st != UPDATE_OK)
				{
					++warnKOrg;
					if (st > status)
						status = st;
				}
			}
			if (archive  &&  event->toBeArchived())
				addArchivedEvent(*event);     // this changes the event ID to an archived ID
		}
		if (!cal->deleteEvent(id, false))   // don't save calendar after deleting
		{
			status = UPDATE_ERROR;
			++warnErr;
		}

		// Remove "Don't show error messages again" for this alarm
		setDontShowErrors(id);
	}

	if (warnErr == events.count())
		status = UPDATE_FAILED;
	else if (!cal->save())      // save the calendars now
	{
		status = SAVE_FAILED;
		warnErr = events.count();
	}
	if (status != UPDATE_OK  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_DELETE, warnErr, warnKOrg, showKOrgErr);
	return status;
}

/******************************************************************************
* Delete templates from the calendar file and from every template list view.
*/
UpdateStatus deleteTemplates(const QStringList& eventIDs, QWidget* msgParent)
{
	kDebug() << eventIDs.count();
	if (eventIDs.isEmpty())
		return UPDATE_OK;
	int warnErr = 0;
	UpdateStatus status = UPDATE_OK;
	AlarmCalendar* cal = AlarmCalendar::resources();
	for (int i = 0, end = eventIDs.count();  i < end;  ++i)
	{
		// Update the window lists
		QString id = eventIDs[i];
		EventListModel::templates()->removeEvent(id);

		// Delete the template from the calendar file
		AlarmCalendar* cal = AlarmCalendar::resources();
		if (!cal->deleteEvent(id, false))    // don't save calendar after deleting
		{
			status = UPDATE_ERROR;
			++warnErr;
		}
	}

	if (warnErr == eventIDs.count())
		status = UPDATE_FAILED;
	else if (!cal->save())      // save the calendars now
	{
		status = SAVE_FAILED;
		warnErr = eventIDs.count();
	}
	cal->emitEmptyStatus();
	if (status != UPDATE_OK  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_TEMPLATE, warnErr);
	return status;
}

/******************************************************************************
* Delete an alarm from the display calendar.
*/
void deleteDisplayEvent(const QString& eventID)
{
	kDebug() << eventID;
	AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
	if (cal)
		cal->deleteEvent(eventID, true);   // save calendar after deleting
}

/******************************************************************************
* Undelete archived alarms, and update every main window instance.
* The archive bit is set to ensure that they get re-archived if deleted again.
* 'ineligibleIDs' is filled in with the IDs of any ineligible events.
*/
UpdateStatus reactivateEvent(KAEvent& event, AlarmResource* resource, QWidget* msgParent, bool showKOrgErr)
{
	QStringList ids;
	KAEvent::List events;
	events += &event;
	return reactivateEvents(events, ids, resource, msgParent, showKOrgErr);
}

UpdateStatus reactivateEvents(KAEvent::List& events, QStringList& ineligibleIDs, AlarmResource* resource, QWidget* msgParent, bool showKOrgErr)
{
	kDebug() << events.count();
	ineligibleIDs.clear();
	if (events.isEmpty())
		return UPDATE_OK;
	int warnErr = 0;
	int warnKOrg = 0;
	UpdateStatus status = UPDATE_OK;
	if (!resource)
		resource = AlarmResources::instance()->destination(KCalEvent::ACTIVE, msgParent);
	if (!resource)
	{
		kDebug() << "No resource";
		status = UPDATE_FAILED;
		warnErr = events.count();
	}
	else
	{
		QString selectID;
		int count = 0;
		AlarmCalendar* cal = AlarmCalendar::resources();
		KDateTime now = KDateTime::currentUtcDateTime();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			// Delete the event from the archived resource
			KAEvent* event = events[i];
			if (event->category() != KCalEvent::ARCHIVED
			||  !event->occursAfter(now, true))
			{
				ineligibleIDs += event->id();
				continue;
			}
			++count;

			KAEvent* newev = new KAEvent(*event);
			QString oldid = event->id();
			newev->setCategory(KCalEvent::ACTIVE);    // this changes the event ID
			if (newev->recurs()  ||  newev->repeatCount())
				newev->setNextOccurrence(now);   // skip any recurrences in the past
			newev->setArchive();    // ensure that it gets re-archived if it is deleted

			// Save the event details in the calendar file.
			// This converts the event ID.
			if (!cal->addEvent(newev, msgParent, true, resource))
			{
				delete newev;
				status = UPDATE_ERROR;
				++warnErr;
				continue;
			}
			if (newev->copyToKOrganizer())
			{
				UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the event
				if (st != UPDATE_OK)
				{
					++warnKOrg;
					if (st > status)
						status = st;
				}
			}

			// Update the window lists
			EventListModel::alarms()->updateEvent(oldid, newev);
//			selectID = newev->id();

			if (cal->event(oldid)    // no error if event doesn't exist in archived resource
			&&  !cal->deleteEvent(oldid, false))   // don't save calendar after deleting
			{
				status = UPDATE_ERROR;
				++warnErr;
			}
			events[i] = newev;
		}

		if (warnErr == count)
			status = UPDATE_FAILED;
		// Save the calendars, even if all events failed, since more than one calendar was updated
		if (!cal->save()  &&  status != UPDATE_FAILED)
		{
			status = SAVE_FAILED;
			warnErr = count;
		}
	}
	if (status != UPDATE_OK  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_REACTIVATE, warnErr, warnKOrg, showKOrgErr);
	return status;
}

/******************************************************************************
* Enable or disable alarms in the calendar file and in every main window instance.
* The new events will have the same event IDs as the old ones.
*/
UpdateStatus enableEvents(KAEvent::List& events, bool enable, QWidget* msgParent)
{
	kDebug() << events.count();
	if (events.isEmpty())
		return UPDATE_OK;
	UpdateStatus status = UPDATE_OK;
	AlarmCalendar* cal = AlarmCalendar::resources();
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		KAEvent* event = events[i];
		if (enable != event->enabled())
		{
			event->setEnabled(enable);

			// Update the event in the calendar file
			KAEvent* newev = cal->updateEvent(event);

			// If we're disabling a display alarm, close any message window
			if (!enable  &&  event->displayAction())
			{
				MessageWin* win = MessageWin::findEvent(event->id());
				delete win;
			}

			// Update the window lists
			EventListModel::alarms()->updateEvent(newev);
		}
	}

	if (!cal->save())
		status = SAVE_FAILED;
	if (status != UPDATE_OK  &&  msgParent)
		displayUpdateError(msgParent, status, ERR_ADD, events.count(), 0);
	return status;
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge all archived events from the default archived alarm resource whose end
* time is longer ago than 'purgeDays'. All events are deleted if 'purgeDays' is
* zero.
*/
void purgeArchive(int purgeDays)
{
	if (purgeDays < 0)
		return;
	kDebug() << purgeDays;
	QDate cutoff = QDate::currentDate().addDays(-purgeDays);
	AlarmResource* resource = AlarmResources::instance()->getStandardResource(AlarmResource::ARCHIVED);
	if (!resource)
		return;
	KAEvent::List events = AlarmCalendar::resources()->events(resource);
	for (int i = 0;  i < events.count();  )
	{
		KAEvent* event = events[i];
		KCal::Incidence* kcalIncidence = resource->incidence(event->id());
		if (purgeDays  &&  kcalIncidence  &&  kcalIncidence->created().date() >= cutoff)
			events.removeAt(i);
		else
			EventListModel::alarms()->removeEvent(events[i++]);   // update the window lists
	}
	if (!events.isEmpty())
		AlarmCalendar::resources()->purgeEvents(events);   // delete the events and save the calendar
}

/******************************************************************************
* Display an error message about an error when saving an event.
*/
void displayUpdateError(QWidget* parent, UpdateStatus status, UpdateError code, int nAlarms, int nKOrgAlarms, bool showKOrgError)
{
	QString errmsg;
	if (status > UPDATE_KORG_ERR)
	{
		switch (code)
		{
			case ERR_ADD:
			case ERR_MODIFY:
				errmsg = (nAlarms > 1) ? i18nc("@info", "Error saving alarms")
				                       : i18nc("@info", "Error saving alarm");
				break;
			case ERR_DELETE:
				errmsg = (nAlarms > 1) ? i18nc("@info", "Error deleting alarms")
				                       : i18nc("@info", "Error deleting alarm");
				break;
			case ERR_REACTIVATE:
				errmsg = (nAlarms > 1) ? i18nc("@info", "Error saving reactivated alarms")
				                       : i18nc("@info", "Error saving reactivated alarm");
				break;
			case ERR_TEMPLATE:
				errmsg = (nAlarms > 1) ? i18nc("@info", "Error saving alarm templates")
				                       : i18nc("@info", "Error saving alarm template");
				break;
		}
		KMessageBox::error(parent, errmsg);
	}
	else if (showKOrgError)
		displayKOrgUpdateError(parent, code, status, nKOrgAlarms);
}

/******************************************************************************
* Display an error message corresponding to a specified alarm update error code.
*/
void displayKOrgUpdateError(QWidget* parent, UpdateError code, UpdateStatus korgError, int nAlarms)
{
	QString errmsg;
	switch (code)
	{
		case ERR_ADD:
		case ERR_REACTIVATE:
			errmsg = (nAlarms > 1) ? i18nc("@info", "Unable to show alarms in KOrganizer")
			                       : i18nc("@info", "Unable to show alarm in KOrganizer");
			break;
		case ERR_MODIFY:
			errmsg = i18nc("@info", "Unable to update alarm in KOrganizer");
			break;
		case ERR_DELETE:
			errmsg = (nAlarms > 1) ? i18nc("@info", "Unable to delete alarms from KOrganizer")
			                       : i18nc("@info", "Unable to delete alarm from KOrganizer");
			break;
		case ERR_TEMPLATE:
			return;
	}
	QString msg;
	if (korgError == UPDATE_KORG_ERRSTART)
		msg = i18nc("@info", "<para>%1</para><para>(KOrganizer not fully started)</para>", errmsg);
	else if (korgError == UPDATE_KORG_ERR)
		msg = i18nc("@info", "<para>%1</para><para>(Error communicating with KOrganizer)</para>", errmsg);
	else
		msg = errmsg;
	KMessageBox::error(parent, msg);
}

/******************************************************************************
* Execute a New Alarm dialog for the specified alarm type.
*/
void editNewAlarm(EditAlarmDlg::Type type, QWidget* parent)
{
	EditAlarmDlg* editDlg = EditAlarmDlg::create(false, type, true, parent);
	doEditNewAlarm(editDlg);
	delete editDlg;
}

/******************************************************************************
* Execute a New Alarm dialog for the specified alarm type.
*/
void editNewAlarm(KAEvent::Action action, QWidget* parent, const AlarmText* text)
{
	bool setAction = false;
	EditAlarmDlg::Type type;
	switch (action)
	{
		case KAEvent::MESSAGE:
		case KAEvent::FILE:
			type = EditAlarmDlg::DISPLAY;
			setAction = true;
			break;
		case KAEvent::COMMAND:
			type = EditAlarmDlg::COMMAND;
			break;
		case KAEvent::EMAIL:
			type = EditAlarmDlg::EMAIL;
			break;
	}
	EditAlarmDlg* editDlg = EditAlarmDlg::create(false, type, true, parent);
	if (setAction  ||  text)
		editDlg->setAction(action, *text);
	doEditNewAlarm(editDlg);
	delete editDlg;
}

/******************************************************************************
* Execute a New Alarm dialog, optionally either presetting it to the supplied
* event, or setting the action and text.
*/
void editNewAlarm(const KAEvent* preset, QWidget* parent)
{
	EditAlarmDlg* editDlg = EditAlarmDlg::create(false, preset, true, parent);
	doEditNewAlarm(editDlg);
	delete editDlg;
}

/******************************************************************************
* Common code for editNewAlarm() variants.
*/
void doEditNewAlarm(EditAlarmDlg* editDlg)
{
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAEvent event;
		AlarmResource* resource;
		editDlg->getEvent(event, resource);

		// Add the alarm to the displayed lists and to the calendar file
		UpdateStatus status = addEvent(event, resource, editDlg);
		switch (status)
		{
			case UPDATE_FAILED:
				return;
			case UPDATE_KORG_ERR:
			case UPDATE_KORG_ERRSTART:
			case UPDATE_KORG_FUNCERR:
				displayKOrgUpdateError(editDlg, ERR_ADD, status, 1);
				break;
			default:
				break;
		}
		Undo::saveAdd(event, resource);

		outputAlarmWarnings(editDlg, &event);
	}
}

/******************************************************************************
* Display the alarm edit dialog to edit a new alarm, preset with a template.
*/
bool editNewAlarm(const QString& templateName, QWidget* parent)
{
	if (!templateName.isEmpty())
	{
		KAEvent* templateEvent = AlarmCalendar::resources()->templateEvent(templateName);
		if (templateEvent->valid())
		{
			editNewAlarm(templateEvent, parent);
			return true;
		}
		kWarning() << templateName << ": template not found";
	}
	return false;
}

/******************************************************************************
* Create a new template.
*/
void editNewTemplate(EditAlarmDlg::Type type, QWidget* parent)
{
	::editNewTemplate(type, 0, parent);
}

/******************************************************************************
* Create a new template, based on an existing event or template.
*/
void editNewTemplate(const KAEvent* preset, QWidget* parent)
{
	::editNewTemplate(EditAlarmDlg::Type(0), preset, parent);
}

} // namespace KAlarm
namespace
{

/******************************************************************************
* Create a new template.
* 'preset' is non-null to base it on an existing event or template; otherwise,
* the alarm type is set to 'type'.
*/
void editNewTemplate(EditAlarmDlg::Type type, const KAEvent* preset, QWidget* parent)
{
	if (!AlarmResources::instance()->activeCount(AlarmResource::TEMPLATE, true))
	{
		KMessageBox::sorry(parent, i18nc("@info", "You must enable a template resource to save the template in"));
		return;
	}
	EditAlarmDlg* editDlg;
	if (preset)
		editDlg = EditAlarmDlg::create(true, preset, true, parent);
	else
		editDlg = EditAlarmDlg::create(true, type, true, parent);
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAEvent event;
		AlarmResource* resource;
		editDlg->getEvent(event, resource);

		// Add the template to the displayed lists and to the calendar file
		KAlarm::addTemplate(event, resource, editDlg);
		Undo::saveAdd(event, resource);
	}
	delete editDlg;
}

} // namespace
namespace KAlarm
{

/******************************************************************************
* Open the Edit Alarm dialog to edit the specified alarm.
* If the alarm is read-only or archived, the dialog is opened read-only.
*/
void editAlarm(KAEvent* event, QWidget* parent)
{
	if (event->expired()  ||  AlarmCalendar::resources()->eventReadOnly(event->id()))
	{
		viewAlarm(event, parent);
		return;
	}
	EditAlarmDlg* editDlg = EditAlarmDlg::create(false, event, false, parent, EditAlarmDlg::RES_USE_EVENT_ID);
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAEvent newEvent;
		AlarmResource* resource;
		bool changeDeferral = !editDlg->getEvent(newEvent, resource);

		// Update the event in the displays and in the calendar file
		Undo::Event undo(*event, resource);
		if (changeDeferral)
		{
			// The only change has been to an existing deferral
			if (updateEvent(newEvent, editDlg, true) != UPDATE_OK)   // keep the same event ID
				return;   // failed to save event
		}
		else
		{
			UpdateStatus status = modifyEvent(*event, newEvent, editDlg);
			if (status != UPDATE_OK  &&  status <= UPDATE_KORG_ERR)
				displayKOrgUpdateError(editDlg, ERR_MODIFY, status, 1);
		}
		Undo::saveEdit(undo, newEvent);

		outputAlarmWarnings(editDlg, &newEvent);
	}
	delete editDlg;
}

/******************************************************************************
* Display the alarm edit dialog to edit a specified alarm.
* An error occurs if the alarm is read-only or expired.
*/
bool editAlarm(const QString& eventID, QWidget* parent)
{
	KAEvent* event = AlarmCalendar::resources()->event(eventID);
	if (!event)
	{
		kError() << eventID << ": event ID not found";
		return false;
	}
	if (AlarmCalendar::resources()->eventReadOnly(eventID))
	{
		kError() << eventID << ": read-only";
		return false;
	}
	switch (event->category())
	{
		case KCalEvent::ACTIVE:
		case KCalEvent::TEMPLATE:
			break;
		default:
			kError() << eventID << ": event not active or template";
			return false;
	}
	editAlarm(event, parent);
	return true;
}

/******************************************************************************
* Open the Edit Alarm dialog to edit the specified template.
* If the template is read-only, the dialog is opened read-only.
*/
void editTemplate(KAEvent* event, QWidget* parent)
{
	if (AlarmCalendar::resources()->eventReadOnly(event->id()))
	{
		// The template is read-only, so make the dialogue read-only
		EditAlarmDlg* editDlg = EditAlarmDlg::create(true, event, false, parent, EditAlarmDlg::RES_PROMPT, true);
		editDlg->exec();
		delete editDlg;
		return;
	}
	EditAlarmDlg* editDlg = EditAlarmDlg::create(true, event, false, parent, EditAlarmDlg::RES_USE_EVENT_ID);
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAEvent newEvent;
		AlarmResource* resource;
		editDlg->getEvent(newEvent, resource);
		QString id = event->id();
		newEvent.setEventID(id);

		// Update the event in the displays and in the calendar file
		Undo::Event undo(*event, resource);
		updateTemplate(newEvent, editDlg);
		Undo::saveEdit(undo, newEvent);
	}
	delete editDlg;
}

/******************************************************************************
* Open the Edit Alarm dialog to view the specified alarm (read-only).
*/
void viewAlarm(const KAEvent* event, QWidget* parent)
{
	EditAlarmDlg* editDlg = EditAlarmDlg::create(false, event, false, parent, EditAlarmDlg::RES_PROMPT, true);
	editDlg->exec();
	delete editDlg;
}

/******************************************************************************
* Returns a list of all alarm templates.
* If shell commands are disabled, command alarm templates are omitted.
*/
KAEvent::List templateList()
{
	KAEvent::List templates;
	bool includeCmdAlarms = ShellProcess::authorised();
	KAEvent::List events = AlarmCalendar::resources()->events(KCalEvent::TEMPLATE);
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		KAEvent* event = events[i];
		if (includeCmdAlarms  ||  event->action() != KAEvent::COMMAND)
			templates.append(event);
	}
	return templates;
}

/******************************************************************************
* To be called after an alarm has been edited.
* Prompt the user to re-enable alarms if they are currently disabled, and if
* it's an email alarm, warn if no 'From' email address is configured.
*/
void outputAlarmWarnings(QWidget* parent, const KAEvent* event)
{
	if (event  &&  event->action() == KAEvent::EMAIL
	&&  Preferences::emailAddress().isEmpty())
		KMessageBox::information(parent, i18nc("@info Please set the 'From' email address...",
		                                       "<para>%1</para><para>Please set it in the Configuration dialog.</para>", KAMail::i18n_NeedFromEmailAddress()));

	if (!theApp()->alarmsEnabled())
	{
		if (KMessageBox::warningYesNo(parent, i18nc("@info", "<para>Alarms are currently disabled.</para><para>Do you want to enable alarms now?</para>"),
		                              QString(), KGuiItem(i18nc("@action:button", "Enable")), KGuiItem(i18nc("@action:button", "Keep Disabled")),
		                              QLatin1String("EditEnableAlarms"))
		                == KMessageBox::Yes)
			theApp()->setAlarmsEnabled(true);
	}
}

/******************************************************************************
* Reload the calendar.
*/
void refreshAlarms()
{
	kDebug();
	if (!refreshAlarmsQueued)
	{
		refreshAlarmsQueued = true;
		theApp()->processQueue();
	}
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* If refreshAlarms() has been called, reload the calendars.
*/
void refreshAlarmsIfQueued()
{
	if (refreshAlarmsQueued)
	{
		kDebug();
		AlarmCalendar::resources()->reload();

		// Close any message windows for alarms which are now disabled
		KAEvent::List events = AlarmCalendar::resources()->events(KCalEvent::ACTIVE);
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			KAEvent* event = events[i];
			if (!event->enabled()  &&  event->displayAction())
			{
				MessageWin* win = MessageWin::findEvent(event->id());
				delete win;
			}
		}

		MainWindow::refresh();
		refreshAlarmsQueued = false;
	}
}

/******************************************************************************
*  Start KMail if it isn't already running, and optionally iconise it.
*  Reply = reason for failure to run KMail (which may be the empty string)
*        = null string if success.
*/
QString runKMail(bool minimise)
{
	QString dbusService = KMAIL_DBUS_SERVICE;
	QString errmsg;
	if (!runProgram(QLatin1String("kmail"), dbusService, (minimise ? QLatin1String(KMAIL_DBUS_WINDOW_PATH) : QString()), errmsg))
		return i18nc("@info", "Unable to start <application>KMail</application><nl/>(<message>%1</message>)", errmsg);
	return QString();
}

/******************************************************************************
*  Start another program for D-Bus access if it isn't already running.
*  If 'dbusWindowPath' is not empty, the program's window with that D-Bus path
*  name is iconised.
*  On exit, 'errorMessage' contains an error message if failure.
*  Reply = true if the program is now running.
*/
bool runProgram(const QString& program, QString& dbusService, const QString& dbusWindowPath, QString& errorMessage)
{
	QDBusReply<bool> reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(dbusService);
	if (!reply.isValid()  ||  !reply.value())
	{
		// Program is not already running, so start it
		if (KToolInvocation::startServiceByDesktopName(program, QString(), &errorMessage, &dbusService))
		{
			kError() << "Couldn't start" << program << " (" << errorMessage << ")";
			return false;
		}
		if (!dbusWindowPath.isEmpty())
		{
			// Minimise its window - don't use hide() since this would remove all
			// trace of it from the panel if it is not configured to be docked in
			// the system tray.
#ifdef __GNUC__
#warning Make minimise window work
#endif
//Call D-Bus app/MainWin showMinimized()???
#if 0
//			QDBusInterface iface(dbusService, DBUS_PATH, DBUS_IFACE);
//			QDBusInterface iface(dbusService, dbusWindowPath);
			QDBusInterface iface(dbusService, "/kmail_mainwindow_1", "com.trolltech.Qt.QWidget");
			QDBusError err = iface.callWithArgumentList(QDBus::NoBlock, QLatin1String("showMinimized"), QList<QVariant>());
			if (err.isValid())
				kError() << program << ": showMinimized D-Bus call failed:" << err.message();
#endif
		}
	}
	errorMessage.clear();
	return true;
}

/******************************************************************************
* The "Don't show again" option for error messages is personal to the user on a
* particular computer. For example, he may want to inhibit error messages only
* on his laptop. So the status is not stored in the alarm calendar, but in the
* user's local KAlarm data directory.
******************************************************************************/

/******************************************************************************
* Return the Don't-show-again error message tags set for a specified alarm ID.
*/
QStringList dontShowErrors(const QString& eventId)
{
	if (eventId.isEmpty())
		return QStringList();
	KConfig config(KStandardDirs::locateLocal("appdata", ALARM_OPTS_FILE));
	KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
	return group.readEntry(eventId, QStringList());
}

/******************************************************************************
* Check whether the specified Don't-show-again error message tag is set for an
* alarm ID.
*/
bool dontShowErrors(const QString& eventId, const QString& tag)
{
	if (tag.isEmpty()  ||  eventId.isEmpty())
		return false;
	QStringList tags = dontShowErrors(eventId);
	return tags.indexOf(tag) >= 0;
}

/******************************************************************************
* Reset the Don't-show-again error message tags for an alarm ID.
* If 'tags' is empty, the config entry is deleted.
*/
void setDontShowErrors(const QString& eventId, const QStringList& tags)
{
	if (eventId.isEmpty())
		return;
	KConfig config(KStandardDirs::locateLocal("appdata", ALARM_OPTS_FILE));
	KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
	if (tags.isEmpty())
		group.deleteEntry(eventId);
	else
		group.writeEntry(eventId, tags);
	group.sync();
}

/******************************************************************************
* Set the specified Don't-show-again error message tag for an alarm ID.
* Existing tags are unaffected.
*/
void setDontShowErrors(const QString& eventId, const QString& tag)
{
	if (tag.isEmpty()  ||  eventId.isEmpty())
		return;
	KConfig config(KStandardDirs::locateLocal("appdata", ALARM_OPTS_FILE));
	KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
	QStringList tags = group.readEntry(eventId, QStringList());
	if (tags.indexOf(tag) < 0)
	{
		tags += tag;
		group.writeEntry(eventId, tags);
		group.sync();
	}
}

/******************************************************************************
*  Read the size for the specified window from the config file, for the
*  current screen resolution.
*  Reply = true if size set in the config file, in which case 'result' is set
*        = false if no size is set, in which case 'result' is unchanged.
*/
bool readConfigWindowSize(const char* window, QSize& result, int* splitterWidth)
{
	KConfigGroup config(KGlobal::config(), window);
	QWidget* desktop = KApplication::desktop();
	QSize s = QSize(config.readEntry(QString::fromLatin1("Width %1").arg(desktop->width()), (int)0),
	                config.readEntry(QString::fromLatin1("Height %1").arg(desktop->height()), (int)0));
	if (s.isEmpty())
		return false;
	result = s;
	if (splitterWidth)
		*splitterWidth = config.readEntry(QString::fromLatin1("Splitter %1").arg(desktop->width()), -1);
	return true;
}

/******************************************************************************
*  Write the size for the specified window to the config file, for the
*  current screen resolution.
*/
void writeConfigWindowSize(const char* window, const QSize& size, int splitterWidth)
{
	KConfigGroup config(KGlobal::config(), window);
	QWidget* desktop = KApplication::desktop();
	config.writeEntry(QString::fromLatin1("Width %1").arg(desktop->width()), size.width());
	config.writeEntry(QString::fromLatin1("Height %1").arg(desktop->height()), size.height());
	if (splitterWidth >= 0)
		config.writeEntry(QString::fromLatin1("Splitter %1").arg(desktop->width()), splitterWidth);
	config.sync();
}

/******************************************************************************
* Return the current KAlarm version number.
*/
int Version()
{
	static int version = 0;
	if (!version)
		version = getVersionNumber(KALARM_VERSION);
	return version;
}

/******************************************************************************
* Convert the supplied KAlarm version string to a version number.
* Reply = version number (double digit for each of major, minor & issue number,
*         e.g. 010203 for 1.2.3
*       = 0 if invalid version string.
*/
int getVersionNumber(const QString& version, QString* subVersion)
{
	// N.B. Remember to change  Version(int major, int minor, int rev)
	//      if the representation returned by this method changes.
	if (subVersion)
		subVersion->clear();
	int count = version.count(QChar('.')) + 1;
	if (count < 2)
		return 0;
	bool ok;
	unsigned vernum = version.section('.', 0, 0).toUInt(&ok) * 10000;  // major version
	if (!ok)
		return 0;
	unsigned v = version.section('.', 1, 1).toUInt(&ok);               // minor version
	if (!ok)
		return 0;
	vernum += (v < 99 ? v : 99) * 100;
	if (count >= 3)
	{
		// Issue number: allow other characters to follow the last digit
		QString issue = version.section('.', 2);
		int n = issue.length();
		if (!n  ||  !issue[0].isDigit())
			return 0;
		int i;
		for (i = 0;  i < n && issue[i].isDigit();  ++i) ;
		if (subVersion)
			*subVersion = issue.mid(i);
		v = issue.left(i).toUInt();   // issue number
		vernum += (v < 99 ? v : 99);
	}
	return vernum;
}

/******************************************************************************
* Check from its mime type whether a file appears to be a text or image file.
* If a text file, its type is distinguished.
* Reply = file type.
*/
FileType fileType(const KMimeType::Ptr& mimetype)
{
	if (mimetype->is("text/html"))
		return TextFormatted;
	if (mimetype->is("application/x-executable"))
		return TextApplication;
	if (mimetype->is("text/plain"))
		return TextPlain;
	if (mimetype->name().startsWith(QLatin1String("image/")))
		return Image;
	return Unknown;
}

/******************************************************************************
* Display a modal dialog to choose an existing file, initially highlighting
* any specified file.
* @param initialFile The file to initially highlight - must be a full path name or URL.
* @param defaultDir The directory to start in if @p initialFile is empty. If empty,
*                   the user's home directory will be used. Updated to the
*                   directory containing the selected file, if a file is chosen.
* @param mode OR of KFile::Mode values, e.g. ExistingOnly, LocalOnly.
* Reply = URL selected. If none is selected, URL.isEmpty() is true.
*/
QString browseFile(const QString& caption, QString& defaultDir, const QString& initialFile,
                   const QString& filter, KFile::Modes mode, QWidget* parent)
{
	QString initialDir = !initialFile.isEmpty() ? QString(initialFile).remove(QRegExp("/[^/]*$"))
	                   : !defaultDir.isEmpty()  ? defaultDir
	                   :                          QDir::homePath();
	KFileDialog fileDlg(initialDir, filter, parent);
	fileDlg.setOperationMode(mode & KFile::ExistingOnly ? KFileDialog::Opening : KFileDialog::Saving);
	fileDlg.setMode(KFile::File | mode);
	fileDlg.setCaption(caption);
	if (!initialFile.isEmpty())
		fileDlg.setSelection(initialFile);
	if (fileDlg.exec() != QDialog::Accepted)
		return QString();
	KUrl url = fileDlg.selectedUrl();
	defaultDir = url.path();
	return (mode & KFile::LocalOnly) ? url.pathOrUrl() : url.prettyUrl();
}

/******************************************************************************
* Check whether a date/time is during working hours and/or holidays, depending
* on the flags set for the specified event.
*/
bool isWorkingTime(const KDateTime& dt, const KAEvent* event)
{
	bool workOnly = event && event->workTimeOnly();
	bool holidays = event && event->holidaysExcluded();
	if ((workOnly  &&  !Preferences::workDays().testBit(dt.date().dayOfWeek() - 1))
	||  (holidays  &&  Preferences::holidays().isHoliday(dt.date())))
		return false;
	if (!workOnly)
		return true;
	return dt.isDateOnly()
	   ||  (dt.time() >= Preferences::workDayStart()  &&  dt.time() < Preferences::workDayEnd());
}

/******************************************************************************
*  Return the first day of the week for the user's locale.
*  Reply = 1 (Mon) .. 7 (Sun).
*/
int localeFirstDayOfWeek()
{
	static int firstDay = 0;
	if (!firstDay)
		firstDay = KGlobal::locale()->weekStartDay();
	return firstDay;
}

/******************************************************************************
* Return the week day name (Monday = 1).
*/
QString weekDayName(int day, const KLocale* locale)
{
	switch (day)
	{
		case 1: return ki18nc("@option Name of the weekday", "Monday").toString(locale);
		case 2: return ki18nc("@option Name of the weekday", "Tuesday").toString(locale);
		case 3: return ki18nc("@option Name of the weekday", "Wednesday").toString(locale);
		case 4: return ki18nc("@option Name of the weekday", "Thursday").toString(locale);
		case 5: return ki18nc("@option Name of the weekday", "Friday").toString(locale);
		case 6: return ki18nc("@option Name of the weekday", "Saturday").toString(locale);
		case 7: return ki18nc("@option Name of the weekday", "Sunday").toString(locale);
	}
	return QString();
}

/******************************************************************************
* Convert a date/time specification string into a local date/time or date value.
* Parameters:
*   tzString  = in the form [[[yyyy-]mm-]dd-]hh:mm [TZ] or yyyy-mm-dd [TZ].
*   dateTime  = receives converted date/time value.
*   defaultDt = default date/time used for missing parts of tzString, or null
*               to use current date/time.
*   allowTZ   = whether to allow a time zone specifier in tzString.
* Reply = true if successful.
*/
bool convTimeString(const QByteArray& tzString, KDateTime& dateTime, const KDateTime& defaultDt, bool allowTZ)
{
#define MAX_DT_LEN 19
	int i = tzString.indexOf(' ');
	if (i > MAX_DT_LEN  ||  (i >= 0 && !allowTZ))
		return false;
	QString zone = (i >= 0) ? QString::fromLatin1(tzString.mid(i)) : QString();
	char timeStr[MAX_DT_LEN+1];
	strcpy(timeStr, tzString.left(i >= 0 ? i : MAX_DT_LEN));
	int dt[5] = { -1, -1, -1, -1, -1 };
	char* s;
	char* end;
	bool noTime;
	// Get the minute value
	if ((s = strchr(timeStr, ':')) == 0)
		noTime = true;
	else
	{
		noTime = false;
		*s++ = 0;
		dt[4] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[4] >= 60)
			return false;
		// Get the hour value
		if ((s = strrchr(timeStr, '-')) == 0)
			s = timeStr;
		else
			*s++ = 0;
		dt[3] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[3] >= 24)
			return false;
	}
	bool noDate = true;
	if (s != timeStr)
	{
		noDate = false;
		// Get the day value
		if ((s = strrchr(timeStr, '-')) == 0)
			s = timeStr;
		else
			*s++ = 0;
		dt[2] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
			return false;
		if (s != timeStr)
		{
			// Get the month value
			if ((s = strrchr(timeStr, '-')) == 0)
				s = timeStr;
			else
				*s++ = 0;
			dt[1] = strtoul(s, &end, 10);
			if (end == s  ||  *end  ||  dt[1] == 0  ||  dt[1] > 12)
				return false;
			if (s != timeStr)
			{
				// Get the year value
				dt[0] = strtoul(timeStr, &end, 10);
				if (end == timeStr  ||  *end)
					return false;
			}
		}
	}

	QDate date;
	if (dt[0] >= 0)
		date = QDate(dt[0], dt[1], dt[2]);
	QTime time(0, 0, 0);
	if (noTime)
	{
		// No time was specified, so the full date must have been specified
		if (dt[0] < 0  ||  !date.isValid())
			return false;
		dateTime = KAlarm::applyTimeZone(zone, date, time, false, defaultDt);
	}
	else
	{
		// Compile the values into a date/time structure
		time.setHMS(dt[3], dt[4], 0);
		if (dt[0] < 0)
		{
			// Some or all of the date was omitted.
			// Use the default date/time if provided.
			if (defaultDt.isValid())
			{
				dt[0] = defaultDt.date().year();
				date.setYMD(dt[0],
				            (dt[1] < 0 ? defaultDt.date().month() : dt[1]),
				            (dt[2] < 0 ? defaultDt.date().day() : dt[2]));
			}
			else
				date.setYMD(2000, 1, 1);  // temporary substitute for date
		}
		dateTime = KAlarm::applyTimeZone(zone, date, time, true, defaultDt);
		if (!dateTime.isValid())
			return false;
		if (dt[0] < 0)
		{
			// Some or all of the date was omitted.
			// Use the current date in the specified time zone as default.
			KDateTime now = KDateTime::currentDateTime(dateTime.timeSpec());
			date = dateTime.date();
			date.setYMD(now.date().year(),
			            (dt[1] < 0 ? now.date().month() : dt[1]),
			            (dt[2] < 0 ? now.date().day() : dt[2]));
			if (!date.isValid())
				return false;
			if (noDate  &&  time < now.time())
				date = date.addDays(1);
			dateTime.setDate(date);
		}
	}
	return dateTime.isValid();
}

/******************************************************************************
* Convert a time zone specifier string and apply it to a given date and/or time.
* The time zone specifier is a system time zone name, e.g. "Europe/London",
* "UTC" or "Clock". If no time zone is specified, it defaults to the local time
* zone.
* If 'defaultDt' is valid, it supplies the time spec and default date.
*/
KDateTime applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                        bool haveTime, const KDateTime& defaultDt)
{
	bool error = false;
	KDateTime::Spec spec = KDateTime::LocalZone;
	QString zone = tzstring.trimmed();
	if (defaultDt.isValid())
	{
		// Time spec is supplied - time zone specifier is not allowed
		if (!zone.isEmpty())
			error = true;
		else
			spec = defaultDt.timeSpec();
	}
	else if (!zone.isEmpty())
	{
		if (zone == QLatin1String("Clock"))
			spec = KDateTime::ClockTime;
		else if (zone == QLatin1String("UTC"))
			spec = KDateTime::UTC;
		else
		{
			KTimeZone tz = KSystemTimeZones::zone(zone);
			error = !tz.isValid();
			if (!error)
				spec = tz;
		}
	}

	KDateTime result;
	if (!error)
	{
		if (!date.isValid())
		{
			// It's a time without a date
			if (defaultDt.isValid())
			       result = KDateTime(defaultDt.date(), time, spec);
			else if (spec == KDateTime::LocalZone  ||  spec == KDateTime::ClockTime)
				result = KDateTime(QDate::currentDate(), time, spec);
		}
		else if (haveTime)
		{
			// It's a date and time
			result = KDateTime(date, time, spec);
		}
		else
		{
			// It's a date without a time
			result = KDateTime(date, spec);
		}
	}
	return result;
}

} // namespace KAlarm


namespace {

/******************************************************************************
* Tell KOrganizer to put an alarm in its calendar.
* It will be held by KOrganizer as a simple event, without alarms - KAlarm
* is still responsible for alarming.
*/
KAlarm::UpdateStatus sendToKOrganizer(const KAEvent* event)
{
	KCal::Event* kcalEvent = AlarmCalendar::resources()->createKCalEvent(event);
	// Change the event ID to avoid duplicating the same unique ID as the original event
	QString uid = uidKOrganizer(event->id());
	kcalEvent->setUid(uid);
	kcalEvent->clearAlarms();
	QString userEmail;
	switch (event->action())
	{
		case KAEvent::MESSAGE:
		case KAEvent::FILE:
		case KAEvent::COMMAND:
			kcalEvent->setSummary(event->cleanText());
			userEmail = Preferences::emailAddress();
			break;
		case KAEvent::EMAIL:
		{
			QString from = event->emailFromId()
			             ? Identities::identityManager()->identityForUoid(event->emailFromId()).fullEmailAddr()
			             : Preferences::emailAddress();
			AlarmText atext;
			atext.setEmail(event->emailAddresses(", "), from, QString(), QString(), event->emailSubject(), QString());
			kcalEvent->setSummary(atext.displayText());
			userEmail = from;
			break;
		}
	}
	kcalEvent->setOrganizer(KCal::Person(QString(), userEmail));

	// Translate the event into string format
	KCal::ICalFormat format;
	format.setTimeSpec(Preferences::timeZone(true));
	QString iCal = format.toICalString(kcalEvent);
	delete kcalEvent;

	// Send the event to KOrganizer
	KAlarm::UpdateStatus st = runKOrganizer();   // start KOrganizer if it isn't already running, and create its D-Bus interface
	if (st != KAlarm::UPDATE_OK)
		return st;
	QList<QVariant> args;
	args << iCal;
	QDBusReply<bool> reply = korgInterface->callWithArgumentList(QDBus::Block, QLatin1String("addIncidence"), args);
	if (!reply.isValid())
	{
		if (reply.error().type() == QDBusError::UnknownObject)
		{
			kError()<<"addIncidence() D-Bus error: still starting";
			return KAlarm::UPDATE_KORG_ERRSTART;
		}
		kError() << "addIncidence(" << uid << ") D-Bus call failed:" << reply.error().message();
		return KAlarm::UPDATE_KORG_ERR;
	}
	if (!reply.value())
	{
		kDebug() << "addIncidence(" << uid << ") D-Bus call returned false";
		return KAlarm::UPDATE_KORG_FUNCERR;
	}
	kDebug() << uid << ": success";
	return KAlarm::UPDATE_OK;
}

/******************************************************************************
* Tell KOrganizer to delete an event from its calendar.
*/
KAlarm::UpdateStatus deleteFromKOrganizer(const QString& eventID)
{
	KAlarm::UpdateStatus st = runKOrganizer();   // start KOrganizer if it isn't already running, and create its D-Bus interface
	if (st != KAlarm::UPDATE_OK)
		return st;
	QString newID = uidKOrganizer(eventID);
	QList<QVariant> args;
	args << newID << true;
	QDBusReply<bool> reply = korgInterface->callWithArgumentList(QDBus::Block, QLatin1String("deleteIncidence"), args);
	if (!reply.isValid())
	{
		if (reply.error().type() == QDBusError::UnknownObject)
		{
			kError()<<"deleteIncidence() D-Bus error: still starting";
			return KAlarm::UPDATE_KORG_ERRSTART;
		}
		kError() << "deleteIncidence(" << newID << ") D-Bus call failed:" << reply.error().message();
		return KAlarm::UPDATE_KORG_ERR;
	}
	if (!reply.value())
	{
		kDebug() << "deleteIncidence(" << newID << ") D-Bus call returned false";
		return KAlarm::UPDATE_KORG_FUNCERR;
	}
	kDebug() << newID << ": success";
	return KAlarm::UPDATE_OK;
}

/******************************************************************************
* Start KOrganizer if not already running, and create its D-Bus interface.
*/
KAlarm::UpdateStatus runKOrganizer()
{
	QString error, dbusService;
	int result = KDBusServiceStarter::self()->findServiceFor("DBUS/Organizer", QString(), &error, &dbusService);
	if (result)
	{
		kWarning() << "Unable to start DBUS/Organizer:" << dbusService << error;
		return KAlarm::UPDATE_KORG_ERR;
	}
	// If Kontact is running, there is be a load() method which needs to be called
	// to load KOrganizer into Kontact. But if KOrganizer is running independently,
	// the load() method doesn't exist.
	QDBusInterface iface(KORG_DBUS_SERVICE, KORG_DBUS_LOAD_PATH, "org.kde.KUniqueApplication");
	if (!iface.isValid())
	{
		kWarning() << "Unable to access "KORG_DBUS_LOAD_PATH" D-Bus interface:" << iface.lastError().message();
		return KAlarm::UPDATE_KORG_ERR;
	}
	QDBusReply<bool> reply = iface.call("load");
	if ((!reply.isValid() || !reply.value())
	&&  iface.lastError().type() != QDBusError::UnknownMethod)
	{
		kWarning() << "Loading KOrganizer failed:" << iface.lastError().message();
		return KAlarm::UPDATE_KORG_ERR;
	}

	// KOrganizer has been started, but it may not have the necessary
	// D-Bus interface available yet.
	if (!korgInterface  ||  !korgInterface->isValid())
	{
		delete korgInterface;
		korgInterface = new QDBusInterface(KORG_DBUS_SERVICE, KORG_DBUS_PATH, KORG_DBUS_IFACE);
		if (!korgInterface->isValid())
		{
			kWarning() << "Unable to access "KORG_DBUS_PATH" D-Bus interface:" << korgInterface->lastError().message();
			delete korgInterface;
			korgInterface = 0;
			return KAlarm::UPDATE_KORG_ERRSTART;
		}
	}
	return KAlarm::UPDATE_OK;
}

/******************************************************************************
* Insert a KOrganizer string after the hyphen in the supplied event ID.
*/
QString uidKOrganizer(const QString& id)
{
	QString result = id;
	int i = result.lastIndexOf('-');
	if (i < 0)
		i = result.length();
	return result.insert(i, KORGANIZER_UID);
}

} // namespace

/******************************************************************************
* Case insensitive comparison for use by qSort().
*/
bool caseInsensitiveLessThan(const QString& s1, const QString& s2)
{
	return s1.toLower() < s2.toLower();
}
