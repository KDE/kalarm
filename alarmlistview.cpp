/*
 *  alarmlistview.cpp  -  list of outstanding alarms
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
#include "alarmcalendar.h"
#include "alarmlistview.moc"


const QString repeatAtLoginIndicator = QString::fromLatin1("L");


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
	  drawMessageInColour_(false)
{
	addColumn(i18n("Time"));           // date/time column
	addColumn(i18n("Rep"));            // repeat count column
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
	data.messageText = event.cleanText();
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
	item->setText(COLOUR_COLUMN, QString().sprintf("%06u", (event.type() == KAlarmAlarm::COMMAND ? 0 : event.colour().rgb())));
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
QPixmap* AlarmListViewItem::textIcon;
QPixmap* AlarmListViewItem::fileIcon;
QPixmap* AlarmListViewItem::commandIcon;

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
		painter->fillRect(box, (data->event.type() == KAlarmAlarm::COMMAND ? bgColour : data->event.colour()));
		break;
	case AlarmListView::MESSAGE_COLUMN:
	{
		QPixmap* pixmap;
		switch (data->event.type())
		{
			case KAlarmAlarm::FILE:
				if (!fileIcon)
					fileIcon = new QPixmap(SmallIcon("file"));
				pixmap = fileIcon;
				break;
			case KAlarmAlarm::COMMAND:
				if (!commandIcon)
					commandIcon = new QPixmap(SmallIcon("exec"));
				pixmap = commandIcon;
				break;
			case KAlarmAlarm::MESSAGE:
			default:
				if (!textIcon)
					textIcon = new QPixmap(SmallIcon("message"));
				pixmap = textIcon;
				break;
		}
		int diff = box.height() - pixmap->height();
		QRect pixmapRect = pixmap->rect();
		if (diff < 0)
		{
			pixmapRect.setTop(-diff / 2);
			pixmapRect.setHeight(box.height());
		}
		QRect iconRect(box.left(), box.top() + (diff > 0 ? diff / 2 : 0), pixmap->width(), (diff > 0 ? pixmap->height() : box.height()));
		QRect textRect = box;
		textRect.setLeft(box.left() + pixmap->width() + 2*listView->style().defaultFrameWidth());
		if (!selected  &&  listView->drawMessageInColour())
		{
			QColor colour = data->event.colour();
			painter->fillRect(box, colour);
			painter->setBackgroundColor(colour);
//			painter->setPen(data->event->fgColour());
		}
		else
		{
//			QListViewItem::paintCell(painter, cg, column, width, align);
			painter->fillRect(box, bgColour);
		}
		painter->drawPixmap(iconRect.topLeft(), *pixmap, pixmapRect);
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
			case AlarmListView::MESSAGE_COLUMN:  return i18n("Alarm message text, URL of text file to display, or command to execute. The alarm type is indicated by the icon at the left.");
			case AlarmListView::REPEAT_COLUMN:
				return i18n("Number of scheduled repetitions after the next scheduled display of the alarm.\n"
				            "'%1' indicates that the alarm is repeated at every login.")
				           .arg(repeatAtLoginIndicator);
		}
	}
	return i18n("List of scheduled alarm messages");
};

