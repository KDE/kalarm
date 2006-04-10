/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright (c) 2002-2006 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <stdlib.h>

#include <QToolTip>
#include <QMouseEvent>
#include <QList>

#include <kapplication.h>
#include <klocale.h>
#include <kstdaction.h>
#include <kaboutdata.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstdaction.h>
#include <kstdguiitem.h>
#include <kaccel.h>
#include <kconfig.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "alarmtext.h"
#include "daemon.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "prefdlg.h"
#include "preferences.h"
#include "templatemenuaction.h"
#include "traywindow.moc"


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
	: KSystemTray((theApp()->wantRunInSystemTray() ? parent : 0)),
	  mAssocMainWindow(parent)
{
	kDebug(5950) << "TrayWindow::TrayWindow()\n";
	// Set up GUI icons
	mPixmapEnabled  = loadIcon("kalarm");
	mPixmapDisabled = loadIcon("kalarm_disabled");
	if (mPixmapEnabled.isNull() || mPixmapDisabled.isNull())
		KMessageBox::sorry(this, i18n("Cannot load system tray icon."));
	setAcceptDrops(true);         // allow drag-and-drop onto this window

	// Set up the context menu
	KActionCollection* actcol = actionCollection();
	AlarmEnableAction* a = Daemon::createAlarmEnableAction(actcol);
	a->plug(contextMenu());
	connect(a, SIGNAL(switched(bool)), SLOT(setEnabledStatus(bool)));
	KAlarm::createNewAlarmAction(i18n("&New Alarm..."), this, SLOT(slotNewAlarm()), actcol, "tNew")->plug(contextMenu());
	KAlarm::createNewFromTemplateAction(i18n("New Alarm From &Template"), this, SLOT(slotNewFromTemplate(const KAEvent&)), actcol, "tNewFromTempl")->plug(contextMenu());
	KStdAction::preferences(this, SLOT(slotPreferences()), actcol)->plug(contextMenu());

	// Replace the default handler for the Quit context menu item
	const char* quitName = KStdAction::name(KStdAction::Quit);
	delete actcol->action(quitName);
	KStdAction::quit(this, SLOT(slotQuit()), actcol);

	// Set icon to correspond with the alarms enabled menu status
	Daemon::checkStatus();
	setEnabledStatus(Daemon::monitoringAlarms());
}

TrayWindow::~TrayWindow()
{
	kDebug(5950) << "TrayWindow::~TrayWindow()\n";
	theApp()->removeWindow(this);
	emit deleted();
}

/******************************************************************************
* Called just before the context menu is displayed.
* Update the Alarms Enabled item status.
*/
void TrayWindow::contextMenuAboutToShow(KMenu*)
{
	Daemon::checkStatus();
}

/******************************************************************************
*  Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void TrayWindow::slotNewAlarm()
{
	MainWindow::executeNew();
}

/******************************************************************************
*  Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void TrayWindow::slotNewFromTemplate(const KAEvent& event)
{
	MainWindow::executeNew(event);
}

/******************************************************************************
*  Called when the "Configure KAlarm" menu item is selected.
*/
void TrayWindow::slotPreferences()
{
	KAlarmPrefDlg prefDlg;
	prefDlg.exec();
}

/******************************************************************************
* Called when the Quit context menu item is selected.
*/
void TrayWindow::slotQuit()
{
	theApp()->doQuit(this);
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state, and the icon pixmap.
*/
void TrayWindow::setEnabledStatus(bool status)
{
	kDebug(5950) << "TrayWindow::setEnabledStatus(" << (int)status << ")\n";
	setPixmap(status ? mPixmapEnabled : mPixmapDisabled);
}

/******************************************************************************
*  Called when the mouse is clicked over the panel icon.
*  A left click displays the KAlarm main window.
*  A middle button click displays the New Alarm window.
*/
void TrayWindow::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton  &&  !theApp()->wantRunInSystemTray())
	{
		// Left click: display/hide the first main window
		mAssocMainWindow = MainWindow::toggleWindow(mAssocMainWindow);
	}
	else if (e->button() == Qt::MidButton)
		MainWindow::executeNew();    // display a New Alarm dialog
	else
		KSystemTray::mousePressEvent(e);
}

/******************************************************************************
*  Called when the mouse is released over the panel icon.
*  The main window (if not hidden) is raised and made the active window.
*  If this is done in mousePressEvent(), it doesn't work.
*/
void TrayWindow::mouseReleaseEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton  &&  mAssocMainWindow  &&  mAssocMainWindow->isVisible())
	{
		mAssocMainWindow->raise();
		mAssocMainWindow->setActiveWindow();
	}
	else
		KSystemTray::mouseReleaseEvent(e);
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
		return KSystemTray::event(e);
	QHelpEvent* he = (QHelpEvent*)e;
	QString text;
	if (Daemon::monitoringAlarms())
		text = kapp->aboutData()->programName();
	else
		text = i18n("%1 - disabled", kapp->aboutData()->programName());
	kDebug(5950) << "TrayWindow::event(): " << text << endl;
	if (Preferences::tooltipAlarmCount())
		tooltipAlarmText(text);
	QToolTip::showText(he->pos(), text);
	return true;
}

/******************************************************************************
*  Return the tooltip text showing alarms due in the next 24 hours.
*  The limit of 24 hours is because only times, not dates, are displayed.
*/
void TrayWindow::tooltipAlarmText(QString& text) const
{
	KAEvent event;
	const QString& prefix = Preferences::tooltipTimeToPrefix();
	int maxCount = Preferences::tooltipAlarmCount();
	QDateTime now = QDateTime::currentDateTime();

	// Get today's and tomorrow's alarms, sorted in time order
	QList<TipItem> items;
	KCal::Event::List events = AlarmCalendar::activeCalendar()->eventsWithAlarms(QDateTime(now.date()), now.addDays(1));
	for (KCal::Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
	{
		KCal::Event* kcalEvent = *it;
		event.set(*kcalEvent);
		if (!event.expired()  &&  event.action() == KAEvent::MESSAGE)
		{
			TipItem item;
			DateTime dateTime = event.nextDateTime(false);
			if (dateTime.date() > now.date())
			{
				// Ignore alarms after tomorrow at the current clock time
				if (dateTime.date() != now.date().addDays(1)
				||  dateTime.time() >= now.time())
					continue;
			}
			item.dateTime = dateTime.dateTime();

			// The alarm is due today, or early tomorrow
			bool space = false;
			if (Preferences::showTooltipAlarmTime())
			{
				item.text += KGlobal::locale()->formatTime(item.dateTime.time());
				item.text += QLatin1Char(' ');
				space = true;
			}
			if (Preferences::showTooltipTimeToAlarm())
			{
				int mins = (now.secsTo(item.dateTime) + 59) / 60;
				if (mins < 0)
					mins = 0;
				char minutes[3] = "00";
				minutes[0] = (mins%60) / 10 + '0';
				minutes[1] = (mins%60) % 10 + '0';
				if (Preferences::showTooltipAlarmTime())
					item.text += i18nc("prefix + hours:minutes", "(%1%2:%3)", prefix, mins/60, minutes);
				else
					item.text += i18nc("prefix + hours:minutes", "%1%2:%3", prefix, mins/60, minutes);
				item.text += QLatin1Char(' ');
				space = true;
			}
			if (space)
				item.text += QLatin1Char(' ');
			item.text += AlarmText::summary(event);

			// Insert the item into the list in time-sorted order
			int i = 0;
			for (int iend = items.count();  i < iend;  ++i)
			{
				if (item.dateTime <= items[i].dateTime)
					break;
			}
			items.insert(i, item);
		}
        }
	kDebug(5950) << "TrayWindow::tooltipAlarmText():\n";
	int count = 0;
	for (int i = 0, iend = items.count();  i < iend;  ++i)
	{
		kDebug(5950) << "-- " << (count+1) << ") " << items[i].text << endl;
		text += "\n";
		text += items[i].text;
		if (++count == maxCount)
			break;
	}
}

/******************************************************************************
* Called when the associated main window is closed.
*/
void TrayWindow::removeWindow(MainWindow* win)
{
	if (win == mAssocMainWindow)
		mAssocMainWindow = 0;
}


#ifdef HAVE_X11_HEADERS
	#include <X11/X.h>
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
#include <QX11Info>
#endif

/******************************************************************************
* Check whether the widget is in the system tray.
* Note that it is only sometime AFTER the show event that the system tray
* becomes the widget's parent. So for a definitive status, call this method
* only after waiting a bit...
* Reply = true if the widget is in the system tray, or its status can't be determined.
*       = false if it is not currently in the system tray.
*/
bool TrayWindow::inSystemTray() const
{
#ifdef HAVE_X11_HEADERS
	Window  xParent;    // receives parent window
	Window  root;
	Window* children = 0;
	unsigned int nchildren;
	// Find the X parent window of the widget. This is not the same as the Qt parent widget.
	if (!XQueryTree(QX11Info::display(), winId(), &root, &xParent, &children, &nchildren))
		return true;    // error determining its parent X window
	if (children)
		XFree(children);

	// If it is in the system tray, the system tray window will be its X parent.
	// Otherwise, the root window will be its X parent.
	return xParent != root;
#else
	return true;
#endif // HAVE_X11_HEADERS
}
