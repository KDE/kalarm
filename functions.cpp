/*
 *  functions.cpp  -  miscellaneous functions
 *  Program:  kalarm
 *  (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kalarm.h"

#include <qdeepcopy.h>
#include <kconfig.h>
#include <kaction.h>
#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include <libkcal/event.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "alarmlistview.h"
#include "daemon.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "preferences.h"
#include "shellprocess.h"
#include "templatelistview.h"
#include "templatemenuaction.h"
#include "functions.h"


namespace
{
bool resetDaemonQueued = false;
}

/* Define the icons to be used for "New alarm" and "New alarm from template".
 * The former is plain, while the latter has a yellow star in it.
 * TODO: These definitions may need to be changed for new versions of KDE.
 * DON'T CHANGE THESE VARIABLE NAMES. (The KAlarm standalone package depends on them.)
 */
const char NEW_ICON[] =               "filenew";
const char NEW_FROM_TEMPLATE_ICON[] = "file_new_template";


namespace KAlarm
{

/******************************************************************************
*  Display a main window with the specified event selected.
*/
MainWindow* displayMainWindowSelected(const QString& eventID)
{
	MainWindow* win = MainWindow::firstWindow();
	if (!win)
	{
		if (theApp()->checkCalendarDaemon())    // ensure calendar is open and daemon started
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
			win->showNormal();
		win->raise();
		win->setActiveWindow();
	}
	if (win  &&  !eventID.isEmpty())
		win->selectEvent(eventID);
	return win;
}

/******************************************************************************
* Create a New Alarm KAction.
*/
KAction* createNewAlarmAction(const QString& label, QObject* receiver, const char* slot, KActionCollection* actions, const char* name)
{
	return new KAction(label, NEW_ICON, Qt::Key_Insert, receiver, slot, actions, name);
}

/******************************************************************************
* Create a New From Template KAction.
*/
TemplateMenuAction* createNewFromTemplateAction(const QString& label, QObject* receiver, const char* slot, KActionCollection* actions, const char* name)
{
	return new TemplateMenuAction(label, NEW_FROM_TEMPLATE_ICON, receiver, slot, actions, name);
}

/******************************************************************************
* Add a new active (non-expired) alarm.
* Save it in the calendar file and add it to every main window instance.
* If 'selectionView' is non-null, the selection highlight is moved to the new
* event in that listView instance.
* 'event' is updated with the actual event ID.
*/
bool addEvent(KAEvent& event, AlarmListView* selectionView, bool useEventID)
{
	kdDebug(5950) << "KAlarm::addEvent(): " << event.id() << endl;
	if (!theApp()->checkCalendarDaemon())    // ensure calendar is open and daemon started
		return false;

	// Save the event details in the calendar file, and get the new event ID
	AlarmCalendar* cal = AlarmCalendar::activeCalendar();
	cal->addEvent(event, useEventID);
	cal->save();

	// Update the window lists
	AlarmListView::addEvent(event, selectionView);
	return true;
}

/******************************************************************************
* Save the event in the expired calendar file and adjust every main window instance.
* The event's ID is changed to an expired ID if necessary.
*/
bool addExpiredEvent(KAEvent& event)
{
	kdDebug(5950) << "KAlarm::addExpiredEvent(" << event.id() << ")\n";
	AlarmCalendar* cal = AlarmCalendar::expiredCalendarOpen();
	if (!cal)
		return false;
	bool archiving = (KAEvent::uidStatus(event.id()) == KAEvent::ACTIVE);
	if (archiving)
		event.setSaveDateTime(QDateTime::currentDateTime());   // time stamp to control purging
	KCal::Event* kcalEvent = cal->addEvent(event);
	cal->save();

	// Update window lists
	if (!archiving)
		AlarmListView::addEvent(event, 0);
	else if (kcalEvent)
		AlarmListView::modifyEvent(KAEvent(*kcalEvent), 0);
	return true;
}

/******************************************************************************
* Add a new template.
* Save it in the calendar file and add it to every template list view.
* If 'selectionView' is non-null, the selection highlight is moved to the new
* event in that listView instance.
* 'event' is updated with the actual event ID.
*/
bool addTemplate(KAEvent& event, TemplateListView* selectionView)
{
	kdDebug(5950) << "KAlarm::addTemplate(): " << event.id() << endl;

	// Add the template to the calendar file
	AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
	if (!cal)
		return false;
	cal->addEvent(event);
	cal->save();
	cal->emitEmptyStatus();

	// Update the window lists
	TemplateListView::addEvent(event, selectionView);
	return true;
}

/******************************************************************************
* Modify an active (non-expired) alarm in the calendar file and in every main
* window instance.
* The new event will have a different event ID from the old one.
* If 'selectionView' is non-null, the selection highlight is moved to the
* modified event in that listView instance.
*/
void modifyEvent(KAEvent& oldEvent, const KAEvent& newEvent, AlarmListView* selectionView)
{
	kdDebug(5950) << "KAlarm::modifyEvent(): '" << oldEvent.id() << endl;

	if (!newEvent.valid())
		deleteEvent(oldEvent, true);
	else
	{
		// Update the event in the calendar file, and get the new event ID
		AlarmCalendar* cal = AlarmCalendar::activeCalendar();
		cal->deleteEvent(oldEvent.id());
		cal->addEvent(const_cast<KAEvent&>(newEvent), true);
		cal->save();

		// Update the window lists
		AlarmListView::modifyEvent(oldEvent.id(), newEvent, selectionView);
	}
}

/******************************************************************************
* Update an active (non-expired) alarm from the calendar file and from every
* main window instance.
* The new event will have the same event ID as the old one.
* If 'selectionView' is non-null, the selection highlight is moved to the
* updated event in that listView instance.
*/
void updateEvent(KAEvent& event, AlarmListView* selectionView, bool archiveOnDelete, bool incRevision)
{
	kdDebug(5950) << "KAlarm::updateEvent(): " << event.id() << endl;

	if (!event.valid())
		deleteEvent(event, archiveOnDelete);
	else
	{
		// Update the event in the calendar file
		if (incRevision)
			event.incrementRevision();    // ensure alarm daemon sees the event has changed
		AlarmCalendar* cal = AlarmCalendar::activeCalendar();
		cal->updateEvent(event);
		cal->save();

		// Update the window lists
		AlarmListView::modifyEvent(event, selectionView);
	}
}

/******************************************************************************
* Update a template in the calendar file and in every template list view.
* If 'selectionView' is non-null, the selection highlight is moved to the
* updated event in that listView instance.
*/
void updateTemplate(const KAEvent& event, TemplateListView* selectionView)
{
	AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
	if (cal)
	{
		cal->updateEvent(event);
		cal->save();

		TemplateListView::modifyEvent(event.id(), event, selectionView);
	}
}

/******************************************************************************
* Delete an alarm from the calendar file and from every main window instance.
* If the event is archived, the event's ID is changed to an expired ID if necessary.
*/
void deleteEvent(KAEvent& event, bool archive)
{
	QString id = event.id();
	kdDebug(5950) << "KAlarm::deleteEvent(): " << id << endl;

	// Update the window lists
	AlarmListView::deleteEvent(id);

	// Delete the event from the calendar file
	if (KAEvent::uidStatus(id) == KAEvent::EXPIRED)
	{
		AlarmCalendar* cal = AlarmCalendar::expiredCalendarOpen();
		if (cal)
			cal->deleteEvent(id, true);   // save calendar after deleting
	}
	else
	{
		if (archive  &&  event.toBeArchived())
			addExpiredEvent(event);     // this changes the event ID to an expired ID
		AlarmCalendar* cal = AlarmCalendar::activeCalendar();
		cal->deleteEvent(id);
		cal->save();
	}
}

/******************************************************************************
* Delete a template from the calendar file and from every template list view.
*/
void deleteTemplate(const KAEvent& event)
{
	QString id = event.id();

	// Delete the template from the calendar file
	AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
	if (cal)
	{
		cal->deleteEvent(id);
		cal->save();
		cal->emitEmptyStatus();
	}

	// Update the window lists
	TemplateListView::deleteEvent(id);
}

/******************************************************************************
* Delete an alarm from the display calendar.
*/
void deleteDisplayEvent(const QString& eventID)
{
	kdDebug(5950) << "KAlarm::deleteDisplayEvent(" << eventID << ")\n";

	if (KAEvent::uidStatus(eventID) == KAEvent::DISPLAYING)
	{
		AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
		if (cal)
			cal->deleteEvent(eventID, true);   // save calendar after deleting
	}
}

/******************************************************************************
* Undelete an expired alarm, and update every main window instance.
* The archive bit is set to ensure that it gets re-archived if it is deleted again.
* If 'selectionView' is non-null, the selection highlight is moved to the
* restored event in that listView instance.
*/
bool reactivateEvent(KAEvent& event, AlarmListView* selectionView, bool useEventID)
{
	QString id = event.id();
	kdDebug(5950) << "KAlarm::reactivateEvent(): " << id << endl;

	// Delete the event from the expired calendar file
	if (KAEvent::uidStatus(id) == KAEvent::EXPIRED)
	{
		QDateTime now = QDateTime::currentDateTime();
		if (event.occursAfter(now, true))
		{
			if (event.recurs())
				event.setNextOccurrence(now, true);   // skip any recurrences in the past
			event.setArchive();    // ensure that it gets re-archived if it is deleted

			// Save the event details in the calendar file, and get the new event ID
			AlarmCalendar* cal = AlarmCalendar::activeCalendar();
			cal->addEvent(event, useEventID);
			cal->save();

			// Update the window lists
			AlarmListView::undeleteEvent(id, event, selectionView);

			cal = AlarmCalendar::expiredCalendarOpen();
			if (cal)
				cal->deleteEvent(id, true);   // save calendar after deleting
			return true;
		}
	}
	return false;
}

/******************************************************************************
* Enable or disable an alarm in the calendar file and in every main window instance.
* The new event will have the same event ID as the old one.
* If 'selectionView' is non-null, the selection highlight is moved to the
* updated event in that listView instance.
*/
void enableEvent(KAEvent& event, AlarmListView* selectionView, bool enable)
{
	kdDebug(5950) << "KAlarm::enableEvent(" << enable << "): " << event.id() << endl;

	if (enable != event.enabled())
	{
		event.setEnabled(enable);

		// Update the event in the calendar file
		AlarmCalendar* cal = AlarmCalendar::activeCalendar();
		cal->updateEvent(event);
		cal->save();

		// If we're disabling a display alarm, close any message window
		if (!enable  &&  event.displayAction())
		{
			MessageWin* win = MessageWin::findEvent(event.id());
			delete win;
		}

		// Update the window lists
		AlarmListView::modifyEvent(event, selectionView);
	}
}

/******************************************************************************
*  Returns a list of all alarm templates.
*  If shell commands are disabled, command alarm templates are omitted.
*/
QPtrList<KAEvent> templateList()
{
	QPtrList<KAEvent> templates;
	templates.setAutoDelete(true);
	AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
	if (cal)
	{
		bool includeCmdAlarms = ShellProcess::authorised();
		KCal::Event::List events = cal->events();
		for (KCal::Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
			KCal::Event* kcalEvent = *it;
			KAEvent* event = new KAEvent(*kcalEvent);
			if (includeCmdAlarms  ||  event->action() != KAEvent::COMMAND)
				templates.append(event);
		}
	}
	return templates;
}

/******************************************************************************
* Reset the alarm daemon and reload the calendar.
* If the daemon is not already running, start it.
*/
void resetDaemon()
{
	kdDebug(5950) << "KAlarm::resetDaemon()" << endl;
	if (!resetDaemonQueued)
	{
		resetDaemonQueued = true;
		theApp()->processQueue();
	}
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* If resetDaemon() has been called, reset the alarm daemon and reload the calendars.
* If the daemon is not already running, start it.
*/
void resetDaemonIfQueued()
{
	if (resetDaemonQueued)
	{
		kdDebug(5950) << "KAlarm::resetDaemonIfNeeded()" << endl;
		AlarmCalendar::activeCalendar()->reload();
		AlarmCalendar::expiredCalendar()->reload();

		// Close any message windows for alarms which are now disabled
		KAEvent event;
		KCal::Event::List events = AlarmCalendar::activeCalendar()->events();
		for (KCal::Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
			KCal::Event* kcalEvent = *it;
			event.set(*kcalEvent);
			if (!event.enabled()  &&  event.displayAction())
			{
				MessageWin* win = MessageWin::findEvent(event.id());
				delete win;
			}
		}

		MainWindow::refresh();
		if (!Daemon::reset())
			Daemon::start();
		resetDaemonQueued = false;
	}
}

/******************************************************************************
*  Read the size for the specified window from the config file, for the
*  current screen resolution.
*  Reply = true if size set in the config file, in which case 'result' is set
*        = false if no size is set, in which case 'result' is unchanged.
*/
bool readConfigWindowSize(const char* window, QSize& result)
{
	KConfig* config = KGlobal::config();
	config->setGroup(QString::fromLatin1(window));
	QWidget* desktop = KApplication::desktop();
	QSize s = QSize(config->readNumEntry(QString::fromLatin1("Width %1").arg(desktop->width()), 0),
	                config->readNumEntry(QString::fromLatin1("Height %1").arg(desktop->height()), 0));
	if (s.isEmpty())
		return false;
	result = s;
	return true;
}

/******************************************************************************
*  Write the size for the specified window to the config file, for the
*  current screen resolution.
*/
void writeConfigWindowSize(const char* window, const QSize& size)
{
	KConfig* config = KGlobal::config();
	config->setGroup(QString::fromLatin1(window));
	QWidget* desktop = KApplication::desktop();
	config->writeEntry(QString::fromLatin1("Width %1").arg(desktop->width()), size.width());
	config->writeEntry(QString::fromLatin1("Height %1").arg(desktop->height()), size.height());
	config->sync();
}

/******************************************************************************
* Check from its mime type whether a file appears to be a text or image file.
* If a text file, its type is distinguished.
* Reply = file type.
*/
FileType fileType(const QString& mimetype)
{
	static const char* applicationTypes[] = {
		"x-shellscript", "x-nawk", "x-awk", "x-perl", "x-python",
		"x-desktop", "x-troff", 0 };
	static const char* formattedTextTypes[] = {
		"html", "xml", 0 };

	if (mimetype.startsWith(QString::fromLatin1("image/")))
		return Image;
	int slash = mimetype.find('/');
	if (slash < 0)
		return Unknown;
	QString type = mimetype.mid(slash + 1);
	const char* typel = type.latin1();
	if (mimetype.startsWith(QString::fromLatin1("application")))
	{
		for (int i = 0;  applicationTypes[i];  ++i)
			if (!strcmp(typel, applicationTypes[i]))
				return TextApplication;
	}
	else if (mimetype.startsWith(QString::fromLatin1("text")))
	{
		for (int i = 0;  formattedTextTypes[i];  ++i)
			if (!strcmp(typel, formattedTextTypes[i]))
				return TextFormatted;
		return TextPlain;
	}
	return Unknown;
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
*  Return the supplied string with any accelerator code stripped out.
*/
QString stripAccel(const QString& text)
{
	unsigned len = text.length();
	QString out = QDeepCopy<QString>(text);
	QChar *corig = (QChar*)out.unicode();
	QChar *cout  = corig;
	QChar *cin   = cout;
	while (len)
	{
		if ( *cin == '&' )
		{
			++cin;
			--len;
			if ( !len )
				break;
		}
		*cout = *cin;
		++cout;
		++cin;
		--len;
	}
	unsigned newlen = cout - corig;
	if (newlen != out.length())
		out.truncate(newlen);
	return out;
}

} // namespace KAlarm
