/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#include "kalarm.h"

#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qiconset.h>
#include <qheader.h>
#include <qpainter.h>

#include <kmenubar.h>
#include <kpopupmenu.h>
#include <kstdaction.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kconfig.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "editdlg.h"
#include "prefdlg.h"
#include "prefsettings.h"
#include "mainwindow.h"
#include "mainwindow.moc"


KAlarmMainWindow::KAlarmMainWindow(const char* name)
	: KMainWindow(0L, name, WGroupLeader)
{
	kdDebug() << "KAlarmMainWindow::KAlarmMainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(name);
	initActions();

	listView = new AlarmListView(this, "listView");
	setCentralWidget(listView);
	listView->refresh();          // populate the message list
	connect(listView, SIGNAL(currentChanged(QListViewItem*)), this, SLOT(slotSelection()));
	connect(listView, SIGNAL(rightButtonClicked(QListViewItem*, const QPoint&, int)), this,
	        SLOT(slotListRightClick(QListViewItem*, const QPoint&, int)));
	theApp()->addWindow(this);
}

KAlarmMainWindow::~KAlarmMainWindow()
{
	theApp()->deleteWindow(this);
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void KAlarmMainWindow::resizeEvent(QResizeEvent* re)
{
	listView->resizeLastColumn();
	KMainWindow::resizeEvent(re);
}

/******************************************************************************
*  Called when the window is first displayed.
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void KAlarmMainWindow::showEvent(QShowEvent* se)
{
	listView->resizeLastColumn();
	KMainWindow::showEvent(se);
}

/******************************************************************************
*  Initialise the menu and program actions.
*/
void KAlarmMainWindow::initActions()
{
	actionQuit        = new KAction(i18n("&Quit"), QIconSet(SmallIcon("exit")), KStdAccel::key(KStdAccel::Quit), this, SLOT(slotQuit()), this);
	actionNew         = new KAction(i18n("&New"), "eventnew", Qt::Key_Insert, this, SLOT(slotNew()), this);
	actionModify      = new KAction(i18n("&Modify"), "eventmodify", Qt::CTRL+Qt::Key_M, this, SLOT(slotModify()), this);
	actionDelete      = new KAction(i18n("&Delete"), "eventdelete", Qt::Key_Delete, this, SLOT(slotDelete()), this);
	actionResetDaemon = new KAction(i18n("&Reset Daemon"), "reset", Qt::CTRL+Qt::Key_R, this, SLOT(slotResetDaemon()), this);
	KAction* preferences = KStdAction::preferences(this, SLOT(slotPreferences()), actionCollection());

	KMenuBar* menu = menuBar();
	KPopupMenu* fileMenu = new KPopupMenu(this);
	menu->insertItem(i18n("&File"), fileMenu);
	actionQuit->plug(fileMenu);
	KPopupMenu* actionsMenu = new KPopupMenu(this);
	menu->insertItem(i18n("&Actions"), actionsMenu);
	actionNew->plug(actionsMenu);
	actionModify->plug(actionsMenu);
	actionDelete->plug(actionsMenu);
	actionsMenu->insertSeparator(3);
	actionResetDaemon->plug(actionsMenu);
	KPopupMenu* settingsMenu = new KPopupMenu(this);
	menu->insertItem(i18n("&Settings"), settingsMenu);
	preferences->plug(settingsMenu);
	menu->insertItem(i18n("&Help"), helpMenu());

	actionModify->setEnabled(false);
	actionDelete->setEnabled(false);
}

/******************************************************************************
* Add a message to the displayed list.
*/
void KAlarmMainWindow::addMessage(const KAlarmEvent& event)
{
	listView->addEntry(event, true);
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
* Delete a message from the displayed list.
*/
void KAlarmMainWindow::deleteMessage(const KAlarmEvent& event)
{
kdDebug() << "KAlarmMainWindow::deleteMessage(): "<<event.id()<<endl;
	AlarmListViewItem* item = listView->getEntry(event.id());
kdDebug() << "KAlarmMainWindow::deleteMessage(): " << (item?"Found":"Not found")<<endl;
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
//event->setOrganizer("KAlarm");

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
*  Called when the Reset Daemon menu item is selected.
*/
void KAlarmMainWindow::slotResetDaemon()
{
	theApp()->resetDaemon();
}

/******************************************************************************
*  Called when the Preferences menu item is selected.
*/
void KAlarmMainWindow::slotPreferences()
{
#ifdef MISC_PREFS
	KAlarmPrefDlg* pref = new KAlarmPrefDlg(theApp()->generalSettings(), m_miscSettings);
#else
	KAlarmPrefDlg* pref = new KAlarmPrefDlg(theApp()->generalSettings());
#endif
	if (pref->exec())
	{
		theApp()->generalSettings()->saveSettings();
		KGlobal::config()->sync();
	}
}

/******************************************************************************
*  Called when the Quit menu item is selected.
*/
void KAlarmMainWindow::slotQuit()
{
	theApp()->deleteWindow(this);
}

/******************************************************************************
*  Called when the current item in the ListView changes.
*  Selects the new current item, and enables/disables the actions appropriately.
*/
void KAlarmMainWindow::slotSelection()
{
	bool enable = !!listView->selectedItem();
	AlarmListViewItem* item = listView->currentItem();
	if (item  &&  !listView->selectedItem())
	{
		listView->setSelected(item, true);
		enable = true;
	}
	actionModify->setEnabled(enable);
	actionDelete->setEnabled(enable);
}

/******************************************************************************
*  Called when the right button is clicked over the list view.
*  Displays a context menu to modify or delete the selected item.
*/
void KAlarmMainWindow::slotListRightClick(QListViewItem* item, const QPoint& pt, int)
{
	if (item)
	{
		QPopupMenu* menu = new QPopupMenu(this, "ListContextMenu");
		actionModify->plug(menu);
		actionDelete->plug(menu);
		menu->exec(pt);
	}
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
	addColumn(i18n("Column 1"));
	setColumnText(TIME_COLUMN, i18n("Time"));
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
	for (map<AlarmListViewItem*, AlarmItemData>::const_iterator it = entries.begin();  it != entries.end();  ++it)
		if (it->second.event.id() == eventID)
			return it->first;
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
	map<AlarmListViewItem*, AlarmItemData>::iterator it = entries.find(item);
	if (it != entries.end())
	{
		entries.erase(it);
		delete item;
		if (setSize)
			resizeLastColumn();
	}
}

const AlarmItemData* AlarmListView::getData(AlarmListViewItem* item) const
{
	map<AlarmListViewItem*, AlarmItemData>::const_iterator it = entries.find(item);
	if (it == entries.end())
		return 0L;
	return &it->second;
}

/******************************************************************************
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void AlarmListView::resizeLastColumn()
{
	int messageWidth = lastColumnHeaderWidth_;
	for (map<AlarmListViewItem*, AlarmItemData>::const_iterator it = entries.begin();  it != entries.end();  ++it)
	{
		int mw = it->second.messageWidth;
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
	map<AlarmListViewItem*, AlarmItemData>::const_iterator it = entries.begin();
	if (it == entries.end())
	{
		// The list is empty, so create a temporary item to find its height
		QListViewItem* item = new QListViewItem(this, QString::null);
		int height = item->height();
		delete item;
		return height;
	}
	else
		return it->first->height();
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
	if (listView->header()->frameGeometry().contains(pt))
	{
		switch (listView->header()->sectionAt(pt.x()))
		{
			case AlarmListView::TIME_COLUMN:     return i18n("Next scheduled date and time of the alarm");
			case AlarmListView::COLOUR_COLUMN:   return i18n("Background colour of alarm message");
			case AlarmListView::MESSAGE_COLUMN:  return i18n("Alarm message text");
			case AlarmListView::REPEAT_COLUMN:
				return i18n("Number of scheduled repetitions after the\n"
				            "next scheduled display of the alarm.\n"
				            "'%1' indicates that the alarm is repeated\n"
				            "at every login").arg(repeatAtLoginIndicator);
		}
	}
	return i18n("List of scheduled alarm messages");
};

