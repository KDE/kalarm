/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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
#include <stdlib.h>

#include <qtooltip.h>

#include <kapplication.h>
#include <klocale.h>
#include <kstdaction.h>
#include <kaboutdata.h>
#include <kpopupmenu.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstdaction.h>
#include <kstdguiitem.h>
#include <kconfig.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "daemon.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "prefdlg.h"
#include "preferences.h"
#include "traywindow.moc"


class TrayTooltip : public QToolTip
{
	public:
		TrayTooltip(QWidget* parent) : QToolTip(parent) { }
	protected:
		virtual void maybeTip(const QPoint&);
};

struct TipItem
{
	QDateTime  dateTime;
	QString    text;
};


/*=============================================================================
= Class: TrayWindow
= The KDE system tray window.
=============================================================================*/

TrayWindow::TrayWindow(KAlarmMainWindow* parent, const char* name)
	: KSystemTray((theApp()->wantRunInSystemTray() ? parent : 0), name),
	  mAssocMainWindow(parent)
{
	kdDebug(5950) << "TrayWindow::TrayWindow()\n";
	// Set up GUI icons
	mPixmapEnabled  = loadIcon("kalarm");
	mPixmapDisabled = loadIcon("kalarm_disabled");
	if (mPixmapEnabled.isNull() || mPixmapDisabled.isNull())
		KMessageBox::sorry(this, i18n("Cannot load system tray icon."),
		                         i18n("KAlarm Error", "%1 Error").arg(kapp->aboutData()->programName()));
	setAcceptDrops(true);         // allow drag-and-drop onto this window

	// Replace the default handler for the Quit context menu item
	KActionCollection* actcol = actionCollection();
	actcol->remove(actcol->action(KStdAction::stdName(KStdAction::Quit)));
	actcol->insert(KStdAction::quit(this, SLOT(slotQuit()), actcol));

	// Set up the context menu
	AlarmEnableAction* a = Daemon::createAlarmEnableAction(actcol, "tAlarmEnable");
	a->plug(contextMenu());
	connect(a, SIGNAL(switched(bool)), SLOT(setEnabledStatus(bool)));
	KAlarm::createNewAlarmAction(i18n("&New Alarm..."), this, SLOT(slotNewAlarm()), actcol, "tNew")->plug(contextMenu());
	KStdAction::preferences(this, SLOT(slotPreferences()), actcol)->plug(contextMenu());

	// Set icon to correspond with the alarms enabled menu status
	Daemon::checkStatus();
	setEnabledStatus(Daemon::monitoringAlarms());

	mTooltip = new TrayTooltip(this);
}

TrayWindow::~TrayWindow()
{
	kdDebug(5950) << "TrayWindow::~TrayWindow()\n";
	delete mTooltip;
	mTooltip = 0;
	theApp()->removeWindow(this);
	emit deleted();
}

/******************************************************************************
* Called just before the context menu is displayed.
* Update the Alarms Enabled item status.
*/
void TrayWindow::contextMenuAboutToShow(KPopupMenu* menu)
{
	KSystemTray::contextMenuAboutToShow(menu);     // needed for KDE <= 3.1 compatibility
	Daemon::checkStatus();
}

/******************************************************************************
*  Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void TrayWindow::slotNewAlarm()
{
	KAlarmMainWindow::executeNew();
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
	kdDebug(5950) << "TrayWindow::setEnabledStatus(" << (int)status << ")\n";
	setPixmap(status ? mPixmapEnabled : mPixmapDisabled);
}

/******************************************************************************
*  Called when the mouse is clicked over the panel icon.
*  A left click displays the KAlarm main window.
*  A middle button click displays the New Alarm window.
*/
void TrayWindow::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == LeftButton  &&  !theApp()->wantRunInSystemTray())
	{
		// Left click: display/hide the first main window
		mAssocMainWindow = KAlarmMainWindow::toggleWindow(mAssocMainWindow);
	}
	else if (e->button() == MidButton)
		KAlarmMainWindow::executeNew();    // display a New Alarm dialog
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
	if (e->button() == LeftButton  &&  mAssocMainWindow  &&  mAssocMainWindow->isVisible())
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
	KAlarmMainWindow::executeDragEnterEvent(e);
}

/******************************************************************************
*  Called when an object is dropped on the panel icon.
*  If the object is recognised, the edit alarm dialog is opened appropriately.
*/
void TrayWindow::dropEvent(QDropEvent* e)
{
	KAlarmMainWindow::executeDropEvent(0, e);
}

/******************************************************************************
*  Return the tooltip text showing alarms due in the next 24 hours.
*  The limit of 24 hours is because only times, not dates, are displayed.
*/
void TrayWindow::tooltipAlarmText(QString& text) const
{
	KAEvent event;
	Preferences* preferences = Preferences::instance();
	const QString& prefix = preferences->tooltipTimeToPrefix();
	int maxCount = preferences->tooltipAlarmCount();
	QDateTime now = QDateTime::currentDateTime();

	// Get today's and tomorrow's alarms, sorted in time order
	QValueList<TipItem> items;
	QValueList<TipItem>::Iterator iit;
	KCal::Event::List events = AlarmCalendar::activeCalendar()->eventsWithAlarms(now.date(), now.addDays(1));
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
			if (preferences->showTooltipAlarmTime())
			{
				item.text += KGlobal::locale()->formatTime(item.dateTime.time());
				item.text += ' ';
				space = true;
			}
			if (preferences->showTooltipTimeToAlarm())
			{
				int mins = (now.secsTo(item.dateTime) + 59) / 60;
				if (mins < 0)
					mins = 0;
				char minutes[3] = "00";
				minutes[0] = (mins%60) / 10 + '0';
				minutes[1] = (mins%60) % 10 + '0';
				if (preferences->showTooltipAlarmTime())
					item.text += i18n("prefix + hours:minutes", "(%1%2:%3)").arg(prefix).arg(mins/60).arg(minutes);
				else
					item.text += i18n("prefix + hours:minutes", "%1%2:%3").arg(prefix).arg(mins/60).arg(minutes);
				item.text += ' ';
				space = true;
			}
			if (space)
				item.text += ' ';
			item.text += AlarmListViewItem::alarmText(event, false);

			// Insert the item into the list in time-sorted order
			for (iit = items.begin();  iit != items.end();  ++iit)
			{
				if (item.dateTime <= (*iit).dateTime)
					break;
			}
			items.insert(iit, item);
		}
        }
	kdDebug(5950) << "TrayWindow::tooltipAlarmText():\n";
	int count = 0;
	for (iit = items.begin();  iit != items.end();  ++iit)
	{
		kdDebug(5950) << "-- " << (count+1) << ") " << (*iit).text << endl;
		text += "\n";
		text += (*iit).text;
		if (++count == maxCount)
			break;
	}
}

/******************************************************************************
* Called when the associated main window is closed.
*/
void TrayWindow::removeWindow(KAlarmMainWindow* win)
{
	if (win == mAssocMainWindow)
		mAssocMainWindow = 0;
}


#ifdef HAVE_X11_HEADERS
	#include <X11/X.h>
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
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
	if (!XQueryTree(qt_xdisplay(), winId(), &root, &xParent, &children, &nchildren))
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


/******************************************************************************
*  Displays the appropriate tooltip depending on preference settings.
*/
void TrayTooltip::maybeTip(const QPoint&)
{
	TrayWindow* parent = (TrayWindow*)parentWidget();
	QString text;
	if (Daemon::monitoringAlarms())
		text = kapp->aboutData()->programName();
	else
		text = i18n("%1 - disabled").arg(kapp->aboutData()->programName());
	kdDebug(5950) << "TrayTooltip::maybeTip(): " << text << endl;
	if (Preferences::instance()->tooltipAlarmCount())
		parent->tooltipAlarmText(text);
	tip(parent->rect(), text);
}
