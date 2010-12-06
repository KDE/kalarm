/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright © 2002-2010 by David Jarvie <djarvie@kde.org>
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
#ifdef USE_AKONADI
#include "akonadimodel.h"
#include "collectionmodel.h"
#else
#include "alarmresources.h"
#endif
#include "alarmtext.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "newalarmaction.h"
#include "prefdlg.h"
#include "preferences.h"
#include "synchtimer.h"
#include "templatemenuaction.h"

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

#include <QToolTip>
#include <QMouseEvent>
#include <QList>
#include <QTimer>

#include <stdlib.h>


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
	: KStatusNotifierItem(parent),
	  mAssocMainWindow(parent),
#ifdef USE_AKONADI
	  mAlarmsModel(0),
#endif
	  mHaveDisabledAlarms(false)
{
	kDebug();
	setToolTipIconByName("kalarm");
	setToolTipTitle(KGlobal::mainComponent().aboutData()->programName());
	setIconByName("kalarm");
	// Load the disabled icon for use by setIconByPixmap()
	// - setIconByName() doesn't work for this one!
	mIconDisabled.addPixmap(KIconLoader::global()->loadIcon("kalarm-disabled", KIconLoader::Panel));
	setStatus(KStatusNotifierItem::Active);

	// Set up the context menu
	KActionCollection* actions = actionCollection();
	mActionEnabled = KAlarm::createAlarmEnableAction(this);
	actions->addAction(QLatin1String("tAlarmsEnable"), mActionEnabled);
	contextMenu()->addAction(mActionEnabled);
	connect(theApp(), SIGNAL(alarmEnabledToggled(bool)), SLOT(setEnabledStatus(bool)));
	contextMenu()->addSeparator();

	mActionNew = new NewAlarmAction(false, i18nc("@action", "&New Alarm"), this);
	actions->addAction(QLatin1String("tNew"), mActionNew);
	contextMenu()->addAction(mActionNew);
	connect(mActionNew, SIGNAL(selected(EditAlarmDlg::Type)), SLOT(slotNewAlarm(EditAlarmDlg::Type)));

	mActionNewFromTemplate = KAlarm::createNewFromTemplateAction(i18nc("@action", "New Alarm From &Template"), actions, QLatin1String("tNewFromTempl"));
	contextMenu()->addAction(mActionNewFromTemplate);
	connect(mActionNewFromTemplate, SIGNAL(selected(const KAEvent*)), SLOT(slotNewFromTemplate(const KAEvent*)));
	contextMenu()->addSeparator();

	KAction* a = KAlarm::createStopPlayAction(this);
	actions->addAction(QLatin1String("tStopPlay"), a);
	contextMenu()->addAction(a);
	QObject::connect(theApp(), SIGNAL(audioPlaying(bool)), a, SLOT(setVisible(bool)));

	a = KAlarm::createSpreadWindowsAction(this);
	actions->addAction(QLatin1String("tSpread"), a);
	contextMenu()->addAction(a);
	contextMenu()->addSeparator();
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

#ifdef USE_AKONADI
	connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool)), SLOT(slotCalendarStatusChanged()));
#else
	connect(AlarmResources::instance(), SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotCalendarStatusChanged()));
#endif
	connect(AlarmCalendar::resources(), SIGNAL(haveDisabledAlarmsChanged(bool)), SLOT(slotHaveDisabledAlarms(bool)));
	connect(this, SIGNAL(activateRequested(bool, const QPoint&)), SLOT(slotActivateRequested()));
	connect(this, SIGNAL(secondaryActivateRequested(const QPoint&)), SLOT(slotSecondaryActivateRequested()));
	slotCalendarStatusChanged();   // initialise action states
	slotHaveDisabledAlarms(AlarmCalendar::resources()->haveDisabledAlarms());

	// Hack: KSNI does not let us know when it is about to show the tooltip,
	// so we need to update it whenever something change in it.

	// This timer ensure updateToolTip() is not called several times in a row
	mToolTipUpdateTimer = new QTimer(this);
	mToolTipUpdateTimer->setInterval(0);
	mToolTipUpdateTimer->setSingleShot(true);
	connect(mToolTipUpdateTimer, SIGNAL(timeout()), SLOT(updateToolTip()));

	// Update every minute to show accurate deadlines
	MinuteTimer::connect(mToolTipUpdateTimer, SLOT(start()));

	// Update when alarms are modified
#ifdef USE_AKONADI
	connect(AlarmListModel::all(), SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(AlarmListModel::all(), SIGNAL(rowsInserted(const QModelIndex&, int, int)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(AlarmListModel::all(), SIGNAL(rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(AlarmListModel::all(), SIGNAL(rowsRemoved(const QModelIndex&, int, int)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(AlarmListModel::all(), SIGNAL(modelReset()),
	        mToolTipUpdateTimer, SLOT(start()));
#else
	connect(EventListModel::alarms(), SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(EventListModel::alarms(), SIGNAL(rowsInserted(const QModelIndex&, int, int)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(EventListModel::alarms(), SIGNAL(rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(EventListModel::alarms(), SIGNAL(rowsRemoved(const QModelIndex&, int, int)),
	        mToolTipUpdateTimer, SLOT(start()));
	connect(EventListModel::alarms(), SIGNAL(modelReset()),
	        mToolTipUpdateTimer, SLOT(start()));
#endif

	// Update when tooltip preferences are modified
	Preferences::connect(SIGNAL(tooltipPreferencesChanged()), mToolTipUpdateTimer, SLOT(start()));
}

TrayWindow::~TrayWindow()
{
	kDebug();
	theApp()->removeWindow(this);
	emit deleted();
}

/******************************************************************************
* Called when the status of a calendar has changed.
* Enable or disable actions appropriately.
*/
void TrayWindow::slotCalendarStatusChanged()
{
	// Find whether there are any writable active alarm calendars
#ifdef USE_AKONADI
	bool active = !CollectionControlModel::enabledCollections(KAlarm::CalEvent::ACTIVE, true).isEmpty();
	mActionNewFromTemplate->setEnabled(active && TemplateListModel::all()->haveEvents());
#else
	bool active = AlarmResources::instance()->activeCount(KAlarm::CalEvent::ACTIVE, true);
	mActionNewFromTemplate->setEnabled(active && EventListModel::templates()->haveEvents());
#endif
	mActionNew->setEnabled(active);
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
	// FIXME: Do we really need a slotQuit()?
	theApp()->doQuit(static_cast<QWidget*>(parent()));
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state, and the icon pixmap.
*/
void TrayWindow::setEnabledStatus(bool status)
{
	kDebug() << (int)status;
	updateIcon();
	updateToolTip();
}

/******************************************************************************
* Called when individual alarms are enabled or disabled.
* Set the enabled icon to show or hide a disabled indication.
*/
void TrayWindow::slotHaveDisabledAlarms(bool haveDisabled)
{
	kDebug() << haveDisabled;
	mHaveDisabledAlarms = haveDisabled;
	updateIcon();
	updateToolTip();
}

/******************************************************************************
*  A left click displays the KAlarm main window.
*/
void TrayWindow::slotActivateRequested()
{
	// Left click: display/hide the first main window
	if (mAssocMainWindow  &&  mAssocMainWindow->isVisible())
	{
		mAssocMainWindow->raise();
		mAssocMainWindow->activateWindow();
	}
}

/******************************************************************************
*  A middle button click displays the New Alarm window.
*/
void TrayWindow::slotSecondaryActivateRequested()
{
	if (mActionNew->isEnabled())
		mActionNew->trigger();    // display a New Alarm dialog
}

/******************************************************************************
*  Adjust tooltip according to the app state.
*  The tooltip text shows alarms due in the next 24 hours. The limit of 24
*  hours is because only times, not dates, are displayed.
*/
void TrayWindow::updateToolTip()
{
	bool enabled = theApp()->alarmsEnabled();
	QString subTitle;
	if (enabled && Preferences::tooltipAlarmCount())
		subTitle = tooltipAlarmText();

	if (!enabled)
		subTitle = i18n("Disabled");
	else if (mHaveDisabledAlarms)
	{
		if (!subTitle.isEmpty())
			subTitle += "<br/>";
		subTitle += i18nc("@info:tooltip Brief: some alarms are disabled", "(Some alarms disabled)");
	}
	setToolTipSubTitle(subTitle);
}

/******************************************************************************
*  Adjust icon according to the app state.
*/
void TrayWindow::updateIcon()
{
	if (theApp()->alarmsEnabled())
		setIconByName(mHaveDisabledAlarms ? "kalarm-partdisabled" : "kalarm");
	else
		setIconByPixmap(mIconDisabled);
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
#ifdef USE_AKONADI
	if (!mAlarmsModel)
	{
		mAlarmsModel = new AlarmListModel(const_cast<TrayWindow*>(this));
		mAlarmsModel->setEventTypeFilter(KAlarm::CalEvent::ACTIVE);
		mAlarmsModel->sort(AlarmListModel::TimeColumn);
	}
	for (i = 0, iend = mAlarmsModel->rowCount();  i < iend;  ++i)
#else
	KAEvent::List events = AlarmCalendar::resources()->events(KDateTime(now.date(), QTime(0,0,0), KDateTime::LocalZone), tomorrow, KAlarm::CalEvent::ACTIVE);
	for (i = 0, iend = events.count();  i < iend;  ++i)
#endif
	{
#ifdef USE_AKONADI
		KAEvent mevent = mAlarmsModel->event(i);
		KAEvent* event = &mevent;
#else
		KAEvent* event = events[i];
#endif
		if (event->enabled()  &&  !event->expired()  &&  event->action() == KAEvent::MESSAGE)
		{
			TipItem item;
			QDateTime dateTime = event->nextTrigger(KAEvent::DISPLAY_TRIGGER).effectiveKDateTime().toLocalZone().dateTime();
			if (dateTime > tomorrow.dateTime())
#ifdef USE_AKONADI
				break;   // ignore alarms after tomorrow at the current clock time
#else
				continue;   // ignore alarms after tomorrow at the current clock time
#endif
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
			item.text += AlarmText::summary(*event);

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
		if (i > 0)
			text += "<br />";
		text += items[i].text;
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
