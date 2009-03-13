/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright © 2002-2009 by David Jarvie <djarvie@kde.org>
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
#include "traywindow.moc"

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "alarmresources.h"
#include "alarmtext.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "newalarmaction.h"
#include "prefdlg.h"
#include "preferences.h"
#include "templatemenuaction.h"

#include <stdlib.h>

#include <QToolTip>
#include <QMouseEvent>
#include <QList>
#include <QTimer>

#include <kactioncollection.h>
#include <ktoggleaction.h>
#include <kapplication.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstandardaction.h>
#include <kstandardguiitem.h>
#include <kiconeffect.h>
#include <kconfig.h>
#include <kdebug.h>


struct TipItem
{
	QDateTime  dateTime;
	QString    text;
};


/*=============================================================================
= Class: TrayWindow
= The KDE system tray window.
=============================================================================*/

TrayWindow::TrayWindow(MainWindow* parent)
	: KSystemTrayIcon(parent),
	  mAssocMainWindow(parent)
{
	kDebug();
	// Set up GUI icons
	mIconEnabled  = loadIcon("kalarm");
	if (mIconEnabled.isNull())
		KMessageBox::sorry(parent, i18nc("@info", "Cannot load system tray icon."));
	else
	{
		KIconLoader loader;
		QImage iconDisabled = mIconEnabled.pixmap(loader.currentSize(KIconLoader::Panel)).toImage();
		KIconEffect::toGray(iconDisabled, 1.0);
		mIconDisabled = QIcon(QPixmap::fromImage(iconDisabled));
	}
#ifdef __GNUC__
#warning How to implement drag-and-drop?
#endif
	//setAcceptDrops(true);         // allow drag-and-drop onto this window

	// Set up the context menu
	KActionCollection* actions = actionCollection();
	KAction* a = KAlarm::createAlarmEnableAction(this);
	actions->addAction(QLatin1String("tAlarmsEnable"), a);
	contextMenu()->addAction(a);
	connect(theApp(), SIGNAL(alarmEnabledToggled(bool)), SLOT(setEnabledStatus(bool)));

	mActionNew = new NewAlarmAction(false, i18nc("@action", "&New Alarm"), this);
	actions->addAction(QLatin1String("tNew"), mActionNew);
	contextMenu()->addAction(mActionNew);
	connect(mActionNew, SIGNAL(selected(EditAlarmDlg::Type)), SLOT(slotNewAlarm(EditAlarmDlg::Type)));

	mActionNewFromTemplate = KAlarm::createNewFromTemplateAction(i18nc("@action", "New Alarm From &Template"), actions, QLatin1String("tNewFromTempl"));
	contextMenu()->addAction(mActionNewFromTemplate);
	connect(mActionNewFromTemplate, SIGNAL(selected(const KAEvent*)), SLOT(slotNewFromTemplate(const KAEvent*)));
	a = KAlarm::createSpreadWindowsAction(this);
	actions->addAction(QLatin1String("tSpread"), a);
	contextMenu()->addAction(a);
	contextMenu()->addAction(KStandardAction::preferences(this, SLOT(slotPreferences()), actions));

	// Replace the default handler for the Quit context menu item
	const char* quitName = KStandardAction::name(KStandardAction::Quit);
	delete actions->action(quitName);
	// Quit only once the event loop is called; otherwise, the parent tray icon
	// will be deleted while still processing the action, resulting in a crash.
	a = KStandardAction::quit(0, 0, actions);
	connect(a, SIGNAL(triggered(bool)), SLOT(slotQuit()), Qt::QueuedConnection);

	// Set icon to correspond with the alarms enabled menu status
	setEnabledStatus(theApp()->alarmsEnabled());

	connect(AlarmResources::instance(), SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotResourceStatusChanged()));
	connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(slotActivated(QSystemTrayIcon::ActivationReason)));
	slotResourceStatusChanged();   // initialise action states
}

TrayWindow::~TrayWindow()
{
	kDebug();
	theApp()->removeWindow(this);
	emit deleted();
}

/******************************************************************************
* Called when the status of a resource has changed.
* Enable or disable actions appropriately.
*/
void TrayWindow::slotResourceStatusChanged()
{
	// Find whether there are any writable active alarm resources
	bool active = AlarmResources::instance()->activeCount(AlarmResource::ACTIVE, true);
	mActionNew->setEnabled(active);
	mActionNewFromTemplate->setEnabled(active);
}

/******************************************************************************
*  Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void TrayWindow::slotNewAlarm(EditAlarmDlg::Type type)
{
	KAlarm::editNewAlarm(type);
}

/******************************************************************************
*  Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void TrayWindow::slotNewFromTemplate(const KAEvent* event)
{
	KAlarm::editNewAlarm(event);
}

/******************************************************************************
*  Called when the "Configure KAlarm" menu item is selected.
*/
void TrayWindow::slotPreferences()
{
	KAlarmPrefDlg::display();
}

/******************************************************************************
* Called when the Quit context menu item is selected.
* Note that this must be called by the event loop, not directly from the menu
* item, since otherwise the tray icon will be deleted while still processing
* the menu, resulting in a crash.
*/
void TrayWindow::slotQuit()
{
	theApp()->doQuit(parentWidget());
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state, and the icon pixmap.
*/
void TrayWindow::setEnabledStatus(bool status)
{
	kDebug() << (int)status;
	setIcon(status ? mIconEnabled : mIconDisabled);
}

/******************************************************************************
*  Called when the mouse is clicked over the panel icon.
*  A left click displays the KAlarm main window.
*  A middle button click displays the New Alarm window.
*/
void TrayWindow::slotActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger)
	{
		// Left click: display/hide the first main window
		if (mAssocMainWindow  &&  mAssocMainWindow->isVisible())
		{
			mAssocMainWindow->raise();
			mAssocMainWindow->activateWindow();
		}
	}
	else if (reason == QSystemTrayIcon::MiddleClick)
	{
		if (mActionNew->isEnabled())
			mActionNew->trigger();    // display a New Alarm dialog
	}
}

/******************************************************************************
*  Called when the drag cursor enters the panel icon.
*/
void TrayWindow::dragEnterEvent(QDragEnterEvent* e)
{
	MainWindow::executeDragEnterEvent(e);
}

/******************************************************************************
*  Called when an object is dropped on the panel icon.
*  If the object is recognised, the edit alarm dialog is opened appropriately.
*/
void TrayWindow::dropEvent(QDropEvent* e)
{
	MainWindow::executeDropEvent(0, e);
}

/******************************************************************************
*  Called when any event occurs.
*  If it's a tooltip event, display the tooltip text showing alarms due in the
*  next 24 hours. The limit of 24 hours is because only times, not dates, are
*  displayed.
*/
bool TrayWindow::event(QEvent* e)
{
	if (e->type() != QEvent::ToolTip)
		return KSystemTrayIcon::event(e);
	QHelpEvent* he = (QHelpEvent*)e;
	bool enabled = theApp()->alarmsEnabled();
	QString altext;
	if (enabled  &&  Preferences::tooltipAlarmCount())
		altext = tooltipAlarmText();
	QString text;
	if (enabled)
		text = i18nc("@info:tooltip", "%1%2", KGlobal::mainComponent().aboutData()->programName(), altext);
	else
		text = i18nc("@info:tooltip 'KAlarm - disabled'", "%1 - disabled", KGlobal::mainComponent().aboutData()->programName());
	kDebug() << text;
	QToolTip::showText(he->globalPos(), text);
	return true;
}

/******************************************************************************
*  Return the tooltip text showing alarms due in the next 24 hours.
*  The limit of 24 hours is because only times, not dates, are displayed.
*/
QString TrayWindow::tooltipAlarmText() const
{
	KAEvent event;
	const QString& prefix = Preferences::tooltipTimeToPrefix();
	int maxCount = Preferences::tooltipAlarmCount();
	KDateTime now = KDateTime::currentLocalDateTime();
	KDateTime tomorrow = now.addDays(1);

	// Get today's and tomorrow's alarms, sorted in time order
	int i, iend;
	QList<TipItem> items;
	KAEvent::List events = AlarmCalendar::resources()->events(KDateTime(now.date(), QTime(0,0,0), KDateTime::LocalZone), tomorrow, KCalEvent::ACTIVE);
	for (i = 0, iend = events.count();  i < iend;  ++i)
	{
		KAEvent* event = events[i];
		if (event->enabled()  &&  !event->expired()  &&  event->action() == KAEvent::MESSAGE)
		{
			TipItem item;
			QDateTime dateTime = event->nextTrigger(KAEvent::DISPLAY_TRIGGER).effectiveKDateTime().toLocalZone().dateTime();
			if (dateTime > tomorrow.dateTime())
				continue;   // ignore alarms after tomorrow at the current clock time
			item.dateTime = dateTime;

			// The alarm is due today, or early tomorrow
			if (Preferences::showTooltipAlarmTime())
			{
				item.text += KGlobal::locale()->formatTime(item.dateTime.time());
				item.text += QLatin1Char(' ');
			}
			if (Preferences::showTooltipTimeToAlarm())
			{
				int mins = (now.dateTime().secsTo(item.dateTime) + 59) / 60;
				if (mins < 0)
					mins = 0;
				char minutes[3] = "00";
				minutes[0] = static_cast<char>((mins%60) / 10 + '0');
				minutes[1] = static_cast<char>((mins%60) % 10 + '0');
				if (Preferences::showTooltipAlarmTime())
					item.text += i18nc("@info/plain prefix + hours:minutes", "(%1%2:%3)", prefix, mins/60, minutes);
				else
					item.text += i18nc("@info/plain prefix + hours:minutes", "%1%2:%3", prefix, mins/60, minutes);
				item.text += QLatin1Char(' ');
			}
			item.text += AlarmText::summary(event);

			// Insert the item into the list in time-sorted order
			int it = 0;
			for (int itend = items.count();  it < itend;  ++it)
			{
				if (item.dateTime <= items[it].dateTime)
					break;
			}
			items.insert(it, item);
		}
	}
	kDebug();
	QString text;
	int count = 0;
	for (i = 0, iend = items.count();  i < iend;  ++i)
	{
		kDebug() << "--" << (count+1) << ")" << items[i].text;
		text += "<br />" + items[i].text;
		if (++count == maxCount)
			break;
	}
	return text;
}

/******************************************************************************
* Called when the associated main window is closed.
*/
void TrayWindow::removeWindow(MainWindow* win)
{
	if (win == mAssocMainWindow)
		mAssocMainWindow = 0;
}
