/*
 *  alarmlistview.cpp  -  widget showing list of outstanding alarms
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qwhatsthis.h>
#include <qheader.h>
#include <qpainter.h>
#include <qstyle.h>

#include <kglobal.h>
#include <klocale.h>
#include <klistview.h>
#include <kiconloader.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "alarmcalendar.h"
#include "alarmlistview.moc"


class AlarmListWhatsThis : public QWhatsThis
{
	public:
		AlarmListWhatsThis(AlarmListView* lv) : QWhatsThis(lv), listView(lv) { }
		virtual QString text(const QPoint&);
	private:
		AlarmListView* listView;
};


/*=============================================================================
=  Class: AlarmListView
=  Displays the list of outstanding alarms.
=============================================================================*/

AlarmListView::AlarmListView(QWidget* parent, const char* name)
	: KListView(parent, name),
	  drawMessageInColour_(false),
	  mShowExpired(false)
{
	addColumn(i18n("Time"));           // date/time column
	addColumn(i18n("Repeat"));         // repeat count column
	addColumn(QString::null);          // colour column
	addColumn(i18n("Message, File or Command"));
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
*  Refresh the list by clearing it and redisplaying all the current alarms.
*/
void AlarmListView::refresh()
{
	clear();
	KAlarmEvent event;
	QPtrList<Event> events;
	if (mShowExpired)
	{
		AlarmCalendar* calendar = theApp()->expiredCalendar();
		if (!calendar)
			kdError(5950) << "AlarmListView::refresh(): failed to open expired calendar\n";
		else
		{
			events = calendar->events();
			for (Event* kcalEvent = events.first();  kcalEvent;  kcalEvent = events.next())
			{
				event.set(*kcalEvent);
				addEntry(event);
			}
		}
	}
	events = theApp()->getCalendar().events();
	for (Event* kcalEvent = events.first();  kcalEvent;  kcalEvent = events.next())
	{
		event.set(*kcalEvent);
		if (mShowExpired  ||  !event.expired())
			addEntry(event);
	}
	resizeLastColumn();
}

AlarmListViewItem* AlarmListView::getEntry(const QString& eventID)
{
	for (EntryMap::ConstIterator it = entries.begin();  it != entries.end();  ++it)
		if (it.data().event.id() == eventID)
			return it.key();
	return 0;
}

AlarmListViewItem* AlarmListView::addEntry(const KAlarmEvent& event, bool setSize)
{
	if (!mShowExpired  &&  event.expired())
		return 0;
	QDateTime dateTime = event.dateTime();
	AlarmItemData data;
	data.event       = event;
	data.messageText = (event.action() == KAlarmEvent::EMAIL) ? event.emailSubject() : event.cleanText();
	int newline = data.messageText.find('\n');
	if (newline >= 0)
		data.messageText = data.messageText.left(newline) + QString::fromLatin1("...");
	data.dateTimeText = KGlobal::locale()->formatDate(dateTime.date(), true);
	if (!event.anyTime())
	{
		data.dateTimeText += ' ';
		data.dateTimeText += KGlobal::locale()->formatTime(dateTime.time()) + ' ';
	}
	QString dateTimeOrder;
	dateTimeOrder.sprintf("%04d%03d%02d%02d", dateTime.date().year(), dateTime.date().dayOfYear(),
	                                          dateTime.time().hour(), dateTime.time().minute());

	int repeatOrder = 0;
	int repeatInterval = 0;
	data.repeatText = QString();    // text displayed in Repeat column
	if (event.repeatAtLogin())
	{
		repeatOrder = 1;
		data.repeatText = i18n("Login");
	}
	else
	{
		repeatInterval = event.recurInterval();
		switch (event.recurs())
		{
			case KAlarmEvent::MINUTELY:
				repeatOrder = 2;
				if (repeatInterval < 60)
					data.repeatText = i18n("1 Minute", "%n Minutes", repeatInterval);
				else if (repeatInterval % 60 == 0)
					data.repeatText = i18n("1 Hour", "%n Hours", repeatInterval/60);
				else
				{
					QString mins;
					data.repeatText = i18n("Hours and Minutes", "%1H %2M").arg(QString::number(repeatInterval/60)).arg(mins.sprintf("%02d", repeatInterval%60));
				}
				break;
			case KAlarmEvent::DAILY:
				repeatOrder = 3;
				data.repeatText = i18n("1 Day", "%n Days", repeatInterval);
				break;
			case KAlarmEvent::WEEKLY:
				repeatOrder = 4;
				data.repeatText = i18n("1 Week", "%n Weeks", repeatInterval);
				break;
			case KAlarmEvent::MONTHLY_DAY:
			case KAlarmEvent::MONTHLY_POS:
				repeatOrder = 5;
				data.repeatText = i18n("1 Month", "%n Months", repeatInterval);
				break;
			case KAlarmEvent::ANNUAL_DATE:
			case KAlarmEvent::ANNUAL_POS:
			case KAlarmEvent::ANNUAL_DAY:
				repeatOrder = 6;
				data.repeatText = i18n("1 Year", "%n Years", repeatInterval);
				break;
			case KAlarmEvent::NO_RECUR:
			default:
				break;
		}
	}

	// Set the texts to what will be displayed, so as to make the columns the correct width
	AlarmListViewItem* item = new AlarmListViewItem(this, data.dateTimeText, data.messageText);
	data.messageWidth = item->width(fontMetrics(), this, MESSAGE_COLUMN);
	setColumnWidthMode(REPEAT_COLUMN, QListView::Maximum);
	item->setText(REPEAT_COLUMN, data.repeatText);
	setColumnWidthMode(REPEAT_COLUMN, QListView::Manual);

	// Now set the texts so that the columns can be sorted. The visible text is different,
	// being displayed by paintCell().
	item->setText(TIME_COLUMN, dateTimeOrder);
	item->setText(REPEAT_COLUMN, QString().sprintf("%c%08d", '0' + repeatOrder, repeatInterval));
	bool showColour = (event.action() == KAlarmEvent::MESSAGE || event.action() == KAlarmEvent::FILE);
	item->setText(COLOUR_COLUMN, QString().sprintf("%06u", (showColour ? event.colour().rgb() : 0)));
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
		emit itemDeleted();
	}
}

const AlarmItemData* AlarmListView::getData(AlarmListViewItem* item) const
{
	EntryMap::ConstIterator it = entries.find(item);
	if (it == entries.end())
		return 0;
	return &it.data();
}

bool AlarmListView::expired(AlarmListViewItem* item) const
{
	EntryMap::ConstIterator it = entries.find(item);
	if (it == entries.end())
		return false;
	return it.data().event.expired();
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
QPixmap* AlarmListViewItem::textIcon;
QPixmap* AlarmListViewItem::fileIcon;
QPixmap* AlarmListViewItem::commandIcon;
QPixmap* AlarmListViewItem::emailIcon;
int      AlarmListViewItem::iconWidth = 0;

AlarmListViewItem::AlarmListViewItem(QListView* parent, const QString& dateTime, const QString& message)
	:  QListViewItem(parent, dateTime, QString(), message)
{
	if (!iconWidth)
	{
		// Find the width of the widest icon so that the display can be lined up
		textIcon    = new QPixmap(SmallIcon("message"));
		fileIcon    = new QPixmap(SmallIcon("file"));
		commandIcon = new QPixmap(SmallIcon("exec"));
		emailIcon   = new QPixmap(SmallIcon("mail_generic"));
		if (textIcon)
			iconWidth = textIcon->width();
		if (fileIcon  &&  fileIcon->width() > iconWidth)
			iconWidth = fileIcon->width();
		if (commandIcon  &&  commandIcon->width() > iconWidth)
			iconWidth = commandIcon->width();
		if (emailIcon  &&  emailIcon->width() > iconWidth)
			iconWidth = emailIcon->width();
	}
}

void AlarmListViewItem::paintCell(QPainter* painter, const QColorGroup& cg, int column, int width, int /*align*/)
{
	const AlarmListView* listView = alarmListView();
	const AlarmItemData* data   = listView->getData(this);
	int                  margin = listView->itemMargin();
	QRect box(margin, margin, width - margin*2, height() - margin*2);
	bool   selected = isSelected();
	QColor bgColour = selected ? cg.highlight() : cg.base();
	QColor fgColour = selected ? cg.highlightedText()
	                   : data->event.expired() ? theApp()->settings()->expiredColour() : cg.text();
	painter->setPen(fgColour);
	painter->fillRect(0, 0, width, height(), bgColour);
	switch (column)
	{
	case AlarmListView::TIME_COLUMN:
		painter->drawText(box, AlignVCenter, data->dateTimeText);
		break;
	case AlarmListView::REPEAT_COLUMN:
		painter->drawText(box, AlignVCenter | AlignHCenter, data->repeatText);
		break;
	case AlarmListView::COLOUR_COLUMN: {
		// Paint the cell the colour of the alarm message
		if (data->event.action() == KAlarmEvent::MESSAGE || data->event.action() == KAlarmEvent::FILE)
			painter->fillRect(box, data->event.colour());
		break;
	}
	case AlarmListView::MESSAGE_COLUMN:
	{
		QPixmap* pixmap;
		switch (data->event.action())
		{
			case KAlarmAlarm::FILE:     pixmap = fileIcon;     break;
			case KAlarmAlarm::COMMAND:  pixmap = commandIcon;  break;
			case KAlarmAlarm::EMAIL:    pixmap = emailIcon;    break;
			case KAlarmAlarm::MESSAGE:
			default:                    pixmap = textIcon;     break;
		}
		int frameWidth = listView->style().defaultFrameWidth();
		QRect pixmapRect = pixmap->rect();
		int diff = box.height() - pixmap->height();
		if (diff < 0)
		{
			pixmapRect.setTop(-diff / 2);
			pixmapRect.setHeight(box.height());
		}
		QRect iconRect(box.left(), box.top() + (diff > 0 ? diff / 2 : 0), pixmap->width(), (diff > 0 ? pixmap->height() : box.height()));
		QRect textRect = box;
		textRect.setLeft(box.left() + iconWidth + 3*frameWidth);
		if (!selected  &&  listView->drawMessageInColour())
		{
			QColor colour = data->event.colour();
			painter->fillRect(box, colour);
			painter->setBackgroundColor(colour);
//			painter->setPen(data->event->fgColour());
		}
		painter->drawPixmap(QPoint(iconRect.left() + frameWidth, iconRect.top()), *pixmap, pixmapRect);
		painter->drawText(textRect, AlignVCenter, data->messageText);
		break;
	}
	}
}


/*=============================================================================
=  Class: AlarmListWhatsThis
=  Sets What's This? text depending on where in the list view is clicked.
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
			case AlarmListView::MESSAGE_COLUMN:  return i18n("Alarm message text, URL of text file to display, command to execute, or email subject line. The alarm type is indicated by the icon at the left.");
			case AlarmListView::REPEAT_COLUMN:   return i18n("The alarm's repetition type or recurrence interval.");
		}
	}
	return i18n("List of scheduled alarms");
};

