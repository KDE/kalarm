/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "kalarm.h"

#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qiconset.h>
#include <qheader.h>
#include <qpainter.h>

#include <kmenubar.h>
#include <ktoolbar.h>
#include <kpopupmenu.h>
#include <kaccel.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <klistview.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "daemongui.h"
#include "traywindow.h"
#include "editdlg.h"
#include "prefdlg.h"
#include "prefsettings.h"
#include "mainwindow.h"
#include "mainwindow.moc"


class AlarmListViewItem;
struct AlarmItemData
{
		KAlarmEvent event;
		QString     messageText;       // message as displayed
		QString     dateTimeText;      // date/time as displayed
		QString     repeatCountText;   // repeat count as displayed
		QString     repeatCountOrder;  // repeat count item ordering text
		int         messageWidth;      // width required to display 'messageText'
};


class AlarmListView : public KListView
{
	public:
		enum { TIME_COLUMN, REPEAT_COLUMN, COLOUR_COLUMN, MESSAGE_COLUMN };

		AlarmListView(QWidget* parent = 0L, const char* name = 0L);
		virtual void         clear();
		void                 refresh();
		AlarmListViewItem*   addEntry(const KAlarmEvent&, bool setSize = false);
		AlarmListViewItem*   updateEntry(AlarmListViewItem*, const KAlarmEvent& newEvent, bool setSize = false);
		void                 deleteEntry(AlarmListViewItem*, bool setSize = false);
		const KAlarmEvent    getEntry(AlarmListViewItem* item) const	{ return getData(item)->event; }
		AlarmListViewItem*   getEntry(const QString& eventID);
		const AlarmItemData* getData(AlarmListViewItem*) const;
		void                 resizeLastColumn();
		int                  itemHeight();
		bool                 drawMessageInColour() const		    { return drawMessageInColour_; }
		void                 setDrawMessageInColour(bool inColour)	{ drawMessageInColour_ = inColour; }
		AlarmListViewItem*   selectedItem() const	{ return (AlarmListViewItem*)KListView::selectedItem(); }
		AlarmListViewItem*   currentItem() const	{ return (AlarmListViewItem*)KListView::currentItem(); }
	private:
		typedef QMap<AlarmListViewItem*, AlarmItemData> EntryMap;
		EntryMap             entries;
		int                  lastColumnHeaderWidth_;
		bool                 drawMessageInColour_;
};


class AlarmListViewItem : public QListViewItem
{
	public:
		AlarmListViewItem(QListView* parent, const QString&, const QString&);
		virtual void         paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*       alarmListView() const		{ return (AlarmListView*)listView(); }
};



/*=============================================================================
=  Class: KAlarmMainWindow
=============================================================================*/

QPtrList<KAlarmMainWindow> KAlarmMainWindow::windowList;


KAlarmMainWindow::KAlarmMainWindow()
	: MainWindowBase(0L, 0L, WGroupLeader | WStyle_ContextHelp | WDestructiveClose)
{
	kdDebug(5950) << "KAlarmMainWindow::KAlarmMainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	initActions();

	listView = new AlarmListView(this, "listView");
	setCentralWidget(listView);
	listView->refresh();          // populate the message list
	listView->clearSelection();
	connect(listView, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(slotSelection(QListViewItem*)));
	connect(listView, SIGNAL(mouseButtonClicked(int, QListViewItem*, const QPoint&, int)),
	        this, SLOT(slotMouseClicked(int, QListViewItem*, const QPoint&, int)));
	connect(listView, SIGNAL(rightButtonClicked(QListViewItem*, const QPoint&, int)),
	        this, SLOT(slotListRightClick(QListViewItem*, const QPoint&, int)));
	windowList.append(this);
}

KAlarmMainWindow::~KAlarmMainWindow()
{
	kdDebug(5950) << "KAlarmMainWindow::~KAlarmMainWindow()\n";
	if (findWindow(this))
		windowList.remove();
	if (theApp()->trayWindow())
		theApp()->trayWindow()->removeWindow(this);
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void KAlarmMainWindow::resizeEvent(QResizeEvent* re)
{
	listView->resizeLastColumn();
	MainWindowBase::resizeEvent(re);
}

/******************************************************************************
*  Called when the window is first displayed.
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void KAlarmMainWindow::showEvent(QShowEvent* se)
{
	listView->resizeLastColumn();
	MainWindowBase::showEvent(se);
}

/******************************************************************************
*  Initialise the menu, toolbar and main window actions.
*/
void KAlarmMainWindow::initActions()
{
	actionQuit           = KStdAction::quit(this, SLOT(slotQuit()), this);
	actionNew            = new KAction(i18n("&New..."), "eventnew", Qt::Key_Insert, this, SLOT(slotNew()), this, "new");
	actionModify         = new KAction(i18n("&Modify..."), "pencil", Qt::CTRL+Qt::Key_M, this, SLOT(slotModify()), this, "modify");
	actionDelete         = new KAction(i18n("&Delete"), "eventdelete", Qt::Key_Delete, this, SLOT(slotDelete()), this, "delete");
	actionToggleTrayIcon = new KAction(QString(), "kalarm", Qt::CTRL+Qt::Key_T, this, SLOT(slotToggleTrayIcon()), this, "tray");
	actionResetDaemon    = new KAction(i18n("&Reset Daemon"), "reload", Qt::CTRL+Qt::Key_R, this, SLOT(slotResetDaemon()), this, "reset");

	// Set up the menu bar

	KMenuBar* menu = menuBar();
	KPopupMenu* submenu = new KPopupMenu(this, "file");
	menu->insertItem(i18n("&File"), submenu);
	actionQuit->plug(submenu);

	submenu = new KPopupMenu(this, "view");
	menu->insertItem(i18n("&View"), submenu);
	actionToggleTrayIcon->plug(submenu);
	connect(theApp(), SIGNAL(trayIconToggled()), this, SLOT(updateTrayIconAction()));
	updateTrayIconAction();         // set the correct text for this action

	mActionsMenu = new KPopupMenu(this, "actions");
	menu->insertItem(i18n("&Actions"), mActionsMenu);
	actionNew->plug(mActionsMenu);
	actionModify->plug(mActionsMenu);
	actionDelete->plug(mActionsMenu);
	mActionsMenu->insertSeparator(3);

	ActionAlarmsEnabled* a = theApp()->actionAlarmEnable();
	mAlarmsEnabledId = a->itemId(a->plug(mActionsMenu));
	connect(a, SIGNAL(alarmsEnabledChange(bool)), this, SLOT(setAlarmEnabledStatus(bool)));
	DaemonGuiHandler* daemonGui = theApp()->daemonGuiHandler();
	if (daemonGui)
	{
		daemonGui->checkStatus();
		setAlarmEnabledStatus(daemonGui->monitoringAlarms());
	}

	actionResetDaemon->plug(mActionsMenu);
	connect(mActionsMenu, SIGNAL(aboutToShow()), this, SLOT(updateActionsMenu()));

	submenu = new KPopupMenu(this, "settings");
	menu->insertItem(i18n("&Settings"), submenu);
	theApp()->actionPreferences()->plug(submenu);
	theApp()->actionDaemonPreferences()->plug(submenu);

	menu->insertItem(i18n("&Help"), helpMenu());

	// Set up the toolbar

	KToolBar* toolbar = toolBar();
	actionNew->plug(toolbar);
	actionModify->plug(toolbar);
	actionDelete->plug(toolbar);
	actionToggleTrayIcon->plug(toolbar);


	actionModify->setEnabled(false);
	actionDelete->setEnabled(false);
	if (!theApp()->KDEDesktop())
		actionToggleTrayIcon->setEnabled(false);
}

/******************************************************************************
* Add a new alarm message to every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::addMessage(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmMainWindow::addMessage(): " << event.id() << endl;
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->listView->addEntry(event, true);
}

/******************************************************************************
* Add a message to the displayed list.
*/
/*void KAlarmMainWindow::addMessage(const KAlarmEvent& event)
{
	listView->addEntry(event, true);
}*/

/******************************************************************************
* Modify a message in every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->modifyMessage(oldEventID, newEvent);
}

/******************************************************************************
* Modify a message in the displayed list.
*/
void KAlarmMainWindow::modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent)
{
	AlarmListViewItem* item = listView->getEntry(oldEventID);
	if (item)
	{
		listView->deleteEntry(item);
		listView->addEntry(newEvent, true);
	}
	else
		listView->refresh();
}

/******************************************************************************
* Delete a message from every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::deleteMessage(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->deleteMessage(event);
}

/******************************************************************************
* Delete a message from the displayed list.
*/
void KAlarmMainWindow::deleteMessage(const KAlarmEvent& event)
{
	AlarmListViewItem* item = listView->getEntry(event.id());
	if (item)
		listView->deleteEntry(item, true);
	else
		listView->refresh();
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

/******************************************************************************
*  Called when the New button is clicked to edit a new message to add to the
*  list.
*/
void KAlarmMainWindow::slotNew()
{
	EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("New message"), this, "editDlg");
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAlarmEvent event;
		editDlg->getEvent(event);

		// Add the message to the displayed lists and to the calendar file
		theApp()->addMessage(event, this);
		AlarmListViewItem* item = listView->addEntry(event, true);
		listView->setSelected(item, true);
	}
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  message in the list.
*/
void KAlarmMainWindow::slotModify()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEntry(item);
		EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("Edit message"), this, "editDlg", &event);
		if (editDlg->exec() == QDialog::Accepted)
		{
			KAlarmEvent newEvent;
			editDlg->getEvent(newEvent);

			// Update the event in the displays and in the calendar file
			theApp()->modifyMessage(event.id(), newEvent, this);
			item = listView->updateEntry(item, newEvent, true);
			listView->setSelected(item, true);
		}
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  message in the list.
*/
void KAlarmMainWindow::slotDelete()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEntry(item);

		// Delete the event from the displays
		theApp()->deleteMessage(event, this);
		listView->deleteEntry(item, true);
	}
}

/******************************************************************************
*  Called when the Display System Tray Icon menu item is selected.
*/
void KAlarmMainWindow::slotToggleTrayIcon()
{
	theApp()->displayTrayIcon(!theApp()->trayIconDisplayed(), this);
}

/******************************************************************************
* Called when the system tray icon is created or destroyed.
* Set the system tray icon menu text according to whether or not the system
* tray icon is currently visible.
*/
void KAlarmMainWindow::updateTrayIconAction()
{
	actionToggleTrayIcon->setText(theApp()->trayIconDisplayed() ? i18n("Hide System &Tray Icon") : i18n("Show System &Tray Icon"));
}

/******************************************************************************
* Called when the Actions menu is about to be displayed.
* Update the status of the Alarms Enabled menu item.
*/
void KAlarmMainWindow::updateActionsMenu()
{
	theApp()->daemonGuiHandler()->checkStatus();   // update the Alarms Enabled item status
}

/******************************************************************************
*  Called when the Reset Daemon menu item is selected.
*/
void KAlarmMainWindow::slotResetDaemon()
{
	theApp()->resetDaemon();
}

/******************************************************************************
*  Called when the Quit menu item is selected.
*/
void KAlarmMainWindow::slotQuit()
{
	close();
}

/******************************************************************************
*  Called when the selected item in the ListView changes.
*  Selects the new current item, and enables the actions appropriately.
*/
void KAlarmMainWindow::slotSelection(QListViewItem* item)
{
	if (item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotSelection(true)\n";
		listView->setSelected(item, true);
		actionModify->setEnabled(true);
		actionDelete->setEnabled(true);
	}
}

/******************************************************************************
*  Called when the mouse is clicked on the ListView.
*  Deselects the current item and disables the actions if appropriate.
*/
void KAlarmMainWindow::slotMouseClicked(int, QListViewItem* item, const QPoint&, int)
{
	if (!item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouse(false)\n";
		listView->clearSelection();
		actionModify->setEnabled(false);
		actionDelete->setEnabled(false);
	}
}

/******************************************************************************
*  Called when the right button is clicked over the list view.
*  Displays a context menu to modify or delete the selected item.
*/
void KAlarmMainWindow::slotListRightClick(QListViewItem* item, const QPoint& pt, int)
{
	if (item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotListRightClick(true)\n";
		QPopupMenu* menu = new QPopupMenu(this, "ListContextMenu");
		actionModify->plug(menu);
		actionDelete->plug(menu);
		menu->exec(pt);
	}
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state.
*/
void KAlarmMainWindow::setAlarmEnabledStatus(bool status)
{
	kdDebug(5950) << "KAlarmMainWindow::setAlarmEnabledStatus(" << (int)status << ")\n";
	mActionsMenu->setItemChecked(mAlarmsEnabledId, status);
}

/******************************************************************************
*  Display or hide the specified main window.
*/
KAlarmMainWindow* KAlarmMainWindow::toggleWindow(KAlarmMainWindow* win)
{
	if (win  &&  findWindow(win))
	{
		// A window is specified (and it exists)
		if (win->isVisible())
		{
			// The window is visible, so close it
			win->close();
			return 0L;
		}
		else
		{
			// The window is hidden, so display it
			win->showNormal();
			win->raise();
			win->setActiveWindow();
			return win;
		}
	}

	// No window is specified, or the window doesn't exist. Open a new one.
	win = new KAlarmMainWindow;
	win->show();
	return win;
}

/******************************************************************************
* Find the specified window in the main window list.
*/
bool KAlarmMainWindow::findWindow(KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w == win)
			return true;
	return false;
}


/*=============================================================================
=  Class: AlarmListView
=  Displays the list of outstanding alarms.
=============================================================================*/
class AlarmListWhatsThis : public QWhatsThis
{
	public:
		AlarmListWhatsThis(AlarmListView* lv) : QWhatsThis(lv), listView(lv) { }
		virtual QString text(const QPoint&);
	private:
		AlarmListView* listView;
};

const QString repeatAtLoginIndicator = QString::fromLatin1("L");


AlarmListView::AlarmListView(QWidget* parent, const char* name)
	: KListView(parent, name),
	  drawMessageInColour_(false)
{
	addColumn(i18n("Time"));           // date/time column
	addColumn(i18n("Rep"));            // repeat count column
	addColumn(QString::null);          // colour column
	addColumn(i18n("Message or File"));
	setColumnWidthMode(MESSAGE_COLUMN, QListView::Maximum);
	setAllColumnsShowFocus(true);
	setSorting(TIME_COLUMN);           // sort initially by date/time
	setShowSortIndicator(true);
	lastColumnHeaderWidth_ = columnWidth(MESSAGE_COLUMN);
	setColumnAlignment(REPEAT_COLUMN, Qt::AlignHCenter);
	setColumnWidthMode(REPEAT_COLUMN, QListView::Manual);

	// Find the height of the list items, and set the width of the colour column accordingly
	setColumnWidth(COLOUR_COLUMN, itemHeight() * 3/4);
	setColumnWidthMode(COLOUR_COLUMN, QListView::Manual);
	new AlarmListWhatsThis(this);
}

void AlarmListView::clear()
{
	entries.clear();
	KListView::clear();
}

/******************************************************************************
*  Refresh the list by clearing it and redisplaying all the current messages.
*/
void AlarmListView::refresh()
{
	QPtrList<Event> messages = theApp()->getCalendar().getAllEvents();
	clear();
	for (Event* msg = messages.first();  msg;  msg = messages.next())
		addEntry(KAlarmEvent(*msg));
	resizeLastColumn();
}

AlarmListViewItem* AlarmListView::getEntry(const QString& eventID)
{
	for (EntryMap::ConstIterator it = entries.begin();  it != entries.end();  ++it)
		if (it.data().event.id() == eventID)
			return it.key();
	return 0L;
}

AlarmListViewItem* AlarmListView::addEntry(const KAlarmEvent& event, bool setSize)
{
	QDateTime dateTime = event.dateTime();
	AlarmItemData data;
	data.event = event;
	data.messageText = event.messageOrFile();
	int newline = data.messageText.find('\n');
	if (newline >= 0)
		data.messageText = data.messageText.left(newline) + QString::fromLatin1("...");
	data.dateTimeText = KGlobal::locale()->formatDate(dateTime.date(), true) + ' '
	                  + KGlobal::locale()->formatTime(dateTime.time()) + ' ';
	data.repeatCountText = event.repeatCount() ? QString::number(event.repeatCount()) : QString();
	if (event.repeatAtLogin())
		data.repeatCountText += repeatAtLoginIndicator;
	data.repeatCountOrder.sprintf("%010d%1d", event.repeatCount(), (event.repeatAtLogin() ? 1 : 0));
	QString dateTimeText;
	dateTimeText.sprintf("%04d%03d%02d%02d", dateTime.date().year(), dateTime.date().dayOfYear(),
	                                         dateTime.time().hour(), dateTime.time().minute());

	// Set the texts to what will be displayed, so as to make the columns the correct width
	AlarmListViewItem* item = new AlarmListViewItem(this, data.dateTimeText, data.messageText);
	data.messageWidth = item->width(fontMetrics(), this, MESSAGE_COLUMN);
	setColumnWidthMode(REPEAT_COLUMN, QListView::Maximum);
	item->setText(REPEAT_COLUMN, data.repeatCountText);
	setColumnWidthMode(REPEAT_COLUMN, QListView::Manual);

	// Now set the texts so that the columns can be sorted. The visible text is different,
	// being displayed by paintCell().
	item->setText(TIME_COLUMN, dateTimeText);
	item->setText(REPEAT_COLUMN, data.repeatCountOrder);
	item->setText(COLOUR_COLUMN, QString().sprintf("%06u", event.colour().rgb()));
	item->setText(MESSAGE_COLUMN, data.messageText.lower());
	entries[item] = data;
	if (setSize)
		resizeLastColumn();
	return item;
}

AlarmListViewItem* AlarmListView::updateEntry(AlarmListViewItem* item, const KAlarmEvent& newEvent, bool setSize)
{
	deleteEntry(item);
	return addEntry(newEvent, setSize);
}

void AlarmListView::deleteEntry(AlarmListViewItem* item, bool setSize)
{
	EntryMap::Iterator it = entries.find(item);
	if (it != entries.end())
	{
		entries.remove(it);
		delete item;
		if (setSize)
			resizeLastColumn();
	}
}

const AlarmItemData* AlarmListView::getData(AlarmListViewItem* item) const
{
	EntryMap::ConstIterator it = entries.find(item);
	if (it == entries.end())
		return 0L;
	return &it.data();
}

/******************************************************************************
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void AlarmListView::resizeLastColumn()
{
	int messageWidth = lastColumnHeaderWidth_;
	for (EntryMap::ConstIterator it = entries.begin();  it != entries.end();  ++it)
	{
		int mw = it.data().messageWidth;
		if (mw > messageWidth)
			messageWidth = mw;
	}
	int x = header()->sectionPos(AlarmListView::MESSAGE_COLUMN);
	int width = visibleWidth();
	width -= x;
	if (width < messageWidth)
		width = messageWidth;
	setColumnWidth(AlarmListView::MESSAGE_COLUMN, width);
	if (contentsWidth() > x + width)
		resizeContents(x + width, contentsHeight());
}

int AlarmListView::itemHeight()
{
	EntryMap::ConstIterator it = entries.begin();
	if (it == entries.end())
	{
		// The list is empty, so create a temporary item to find its height
		QListViewItem* item = new QListViewItem(this, QString::null);
		int height = item->height();
		delete item;
		return height;
	}
	else
		return it.key()->height();
}

/*=============================================================================
=  Class: AlarmListViewItem
=  Contains the details of one alarm for display in the AlarmListView.
=============================================================================*/

AlarmListViewItem::AlarmListViewItem(QListView* parent, const QString& dateTime, const QString& message)
	:  QListViewItem(parent, dateTime, QString(), message)
{
}

void AlarmListViewItem::paintCell(QPainter* painter, const QColorGroup& cg, int column, int width, int /*align*/)
{
	const AlarmListView* listView = alarmListView();
	const AlarmItemData* data   = listView->getData(this);
	int                  margin = listView->itemMargin();
	QRect box (margin, margin, width - margin*2, height() - margin*2);
	bool   selected = isSelected();
	QColor bgColour = selected ? cg.highlight() : cg.base();
	painter->setPen(selected ? cg.highlightedText() : cg.text());
	switch (column)
	{
	case AlarmListView::TIME_COLUMN:
		painter->fillRect(box, bgColour);
		painter->drawText(box, AlignVCenter, data->dateTimeText);
		break;
	case AlarmListView::REPEAT_COLUMN:
		painter->fillRect(box, bgColour);
		painter->drawText(box, AlignVCenter | AlignHCenter, data->repeatCountText);
		break;
	case AlarmListView::COLOUR_COLUMN:
		// Paint the cell the colour of the alarm message
		painter->fillRect(box, data->event.colour());
		break;
	case AlarmListView::MESSAGE_COLUMN:
		if (!selected  &&  listView->drawMessageInColour())
		{
			QColor colour = data->event.colour();
			painter->fillRect(box, colour);
			painter->setBackgroundColor(colour);
//			painter->setPen(data->event->fgColour());
			painter->drawText(box, AlignVCenter, data->messageText);
			break;
		}
//		QListViewItem::paintCell(painter, cg, column, width, align);
		painter->fillRect(box, bgColour);
		painter->drawText(box, AlignVCenter, data->messageText);
		break;
	}
}


/*=============================================================================
=  Class: AlarmListWhatsThis
=============================================================================*/
QString AlarmListWhatsThis::text(const QPoint& pt)
{
	QRect frame = listView->header()->frameGeometry();
	if (frame.contains(pt)
	||  listView->itemAt(QPoint(listView->itemMargin(), pt.y())) && frame.contains(QPoint(pt.x(), frame.y())))
	{
		switch (listView->header()->sectionAt(pt.x()))
		{
			case AlarmListView::TIME_COLUMN:     return i18n("Next scheduled date and time of the alarm");
			case AlarmListView::COLOUR_COLUMN:   return i18n("Background color of alarm message");
			case AlarmListView::MESSAGE_COLUMN:  return i18n("Alarm message text or URL of text file to display");
			case AlarmListView::REPEAT_COLUMN:
				return i18n("Number of scheduled repetitions after the next scheduled display of the alarm.\n"
				            "'%1' indicates that the alarm is repeated at every login.")
				           .arg(repeatAtLoginIndicator);
		}
	}
	return i18n("List of scheduled alarm messages");
};

