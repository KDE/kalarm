/*
 *  alarmlistview.cpp  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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

using namespace KCal;


class AlarmListWhatsThis : public QWhatsThis
{
	public:
		AlarmListWhatsThis(AlarmListView* lv) : QWhatsThis(lv), listView(lv) { }
		virtual QString text(const QPoint&);
	private:
		AlarmListView* listView;
};


class AlarmListViewItem : public QListViewItem
{
	public:
		AlarmListViewItem(QListView* parent, const KAlarmEvent&);
		virtual void         paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*       alarmListView() const     { return (AlarmListView*)listView(); }
		const KAlarmEvent&   event() const             { return mEvent; }
		int                  messageWidth() const      { return mMessageWidth; }
		AlarmListViewItem*   nextSibling() const       { return (AlarmListViewItem*)QListViewItem::nextSibling(); }
		virtual QString      key(int column, bool ascending) const;
	private:
		static QPixmap*      textIcon;
		static QPixmap*      fileIcon;
		static QPixmap*      commandIcon;
		static QPixmap*      emailIcon;
		static int           iconWidth;

		KAlarmEvent          mEvent;           // the event for this item
		QString              mDateTimeOrder;   // controls ordering of date/time column
		QString              mRepeatOrder;     // controls ordering of repeat column
		QString              mColourOrder;     // controls ordering of colour column
		int                  mMessageWidth;    // width required to display message column
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
	setColumnWidthMode(REPEAT_COLUMN, QListView::Maximum);

	// Find the height of the list items, and set the width of the colour column accordingly
	setColumnWidth(COLOUR_COLUMN, itemHeight() * 3/4);
	setColumnWidthMode(COLOUR_COLUMN, QListView::Manual);
	new AlarmListWhatsThis(this);
}

void AlarmListView::clear()
{
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
				if (kcalEvent->alarms().count() > 0)
				{
					event.set(*kcalEvent);
					addEntry(event);
				}
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
	for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
		if (item->event().id() == eventID)
			return item;
	return 0;
}

AlarmListViewItem* AlarmListView::addEntry(const KAlarmEvent& event, bool setSize)
{
	if (!mShowExpired  &&  event.expired())
		return 0;
	AlarmListViewItem* item = new AlarmListViewItem(this, event);
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
	if (item)
	{
		delete item;
		if (setSize)
			resizeLastColumn();
		emit itemDeleted();
	}
}

const KAlarmEvent& AlarmListView::getEvent(AlarmListViewItem* item) const
{
	return item->event();
}

bool AlarmListView::expired(AlarmListViewItem* item) const
{
	return item->event().expired();
}

/******************************************************************************
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void AlarmListView::resizeLastColumn()
{
	int messageWidth = lastColumnHeaderWidth_;
	for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
	{
		int mw = item->messageWidth();
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
	AlarmListViewItem* item = firstChild();
	if (!item)
	{
		// The list is empty, so create a temporary item to find its height
		QListViewItem* item = new QListViewItem(this, QString::null);
		int height = item->height();
		delete item;
		return height;
	}
	else
		return item->height();
}

void AlarmListView::setSelected(QListViewItem* item, bool selected)
{
	KListView::setSelected(item, selected);
}

void AlarmListView::setSelected(AlarmListViewItem* item, bool selected)
{
	KListView::setSelected(item, selected);
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

AlarmListViewItem::AlarmListViewItem(QListView* parent, const KAlarmEvent& event)
	: QListViewItem(parent),
	  mEvent(event)
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

	QString messageText = (event.action() == KAlarmEvent::EMAIL) ? event.emailSubject() : event.cleanText();
	int newline = messageText.find('\n');
	if (newline >= 0)
		messageText = messageText.left(newline) + QString::fromLatin1("...");
	setText(AlarmListView::MESSAGE_COLUMN, messageText);
	mMessageWidth = width(parent->fontMetrics(), parent, AlarmListView::MESSAGE_COLUMN);

	QDateTime dateTime = event.expired() ? event.startDateTime() : event.nextDateTime();
	QString dateTimeText = KGlobal::locale()->formatDate(dateTime.date(), true);
	if (!event.anyTime()  ||  event.deferred())
	{
		dateTimeText += ' ';
		dateTimeText += KGlobal::locale()->formatTime(dateTime.time()) + ' ';
	}
	setText(AlarmListView::TIME_COLUMN, dateTimeText);
	mDateTimeOrder.sprintf("%04d%03d%02d%02d", dateTime.date().year(), dateTime.date().dayOfYear(),
	                                           dateTime.time().hour(), dateTime.time().minute());

	int repeatOrder = 0;
	int repeatInterval = 0;
	QString repeatText;        // text displayed in Repeat column
	if (event.repeatAtLogin())
	{
		repeatOrder = 1;
		repeatText = i18n("Login");
	}
	else
	{
		repeatInterval = event.recurInterval();
		switch (event.recurType())
		{
			case KAlarmEvent::MINUTELY:
				repeatOrder = 2;
				if (repeatInterval < 60)
					repeatText = i18n("1 Minute", "%n Minutes", repeatInterval);
				else if (repeatInterval % 60 == 0)
					repeatText = i18n("1 Hour", "%n Hours", repeatInterval/60);
				else
				{
					QString mins;
					repeatText = i18n("Hours and Minutes", "%1H %2M").arg(QString::number(repeatInterval/60)).arg(mins.sprintf("%02d", repeatInterval%60));
				}
				break;
			case KAlarmEvent::DAILY:
				repeatOrder = 3;
				repeatText = i18n("1 Day", "%n Days", repeatInterval);
				break;
			case KAlarmEvent::WEEKLY:
				repeatOrder = 4;
				repeatText = i18n("1 Week", "%n Weeks", repeatInterval);
				break;
			case KAlarmEvent::MONTHLY_DAY:
			case KAlarmEvent::MONTHLY_POS:
				repeatOrder = 5;
				repeatText = i18n("1 Month", "%n Months", repeatInterval);
				break;
			case KAlarmEvent::ANNUAL_DATE:
			case KAlarmEvent::ANNUAL_POS:
			case KAlarmEvent::ANNUAL_DAY:
				repeatOrder = 6;
				repeatText = i18n("1 Year", "%n Years", repeatInterval);
				break;
			case KAlarmEvent::NO_RECUR:
			default:
				break;
		}
	}
	setText(AlarmListView::REPEAT_COLUMN, repeatText);
	mRepeatOrder.sprintf("%c%08d", '0' + repeatOrder, repeatInterval);

	bool showColour = (event.action() == KAlarmEvent::MESSAGE || event.action() == KAlarmEvent::FILE);
	mColourOrder.sprintf("%06u", (showColour ? event.colour().rgb() : 0));
}

void AlarmListViewItem::paintCell(QPainter* painter, const QColorGroup& cg, int column, int width, int /*align*/)
{
	const AlarmListView* listView = alarmListView();
	int                  margin = listView->itemMargin();
	QRect box(margin, margin, width - margin*2, height() - margin*2);
	bool   selected = isSelected();
	QColor bgColour = selected ? cg.highlight() : cg.base();
	QColor fgColour = selected ? cg.highlightedText()
	                   : mEvent.expired() ? theApp()->settings()->expiredColour() : cg.text();
	painter->setPen(fgColour);
	painter->fillRect(0, 0, width, height(), bgColour);
	switch (column)
	{
	case AlarmListView::TIME_COLUMN:
		painter->drawText(box, AlignVCenter, text(column));
		break;
	case AlarmListView::REPEAT_COLUMN:
		painter->drawText(box, AlignVCenter | AlignHCenter, text(column));
		break;
	case AlarmListView::COLOUR_COLUMN: {
		// Paint the cell the colour of the alarm message
		if (mEvent.action() == KAlarmEvent::MESSAGE || mEvent.action() == KAlarmEvent::FILE)
			painter->fillRect(box, mEvent.colour());
		break;
	}
	case AlarmListView::MESSAGE_COLUMN:
	{
		QPixmap* pixmap;
		switch (mEvent.action())
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
			painter->fillRect(box, mEvent.colour());
			painter->setBackgroundColor(mEvent.colour());
//			painter->setPen(mEvent->fgColour());
		}
		painter->drawPixmap(QPoint(iconRect.left() + frameWidth, iconRect.top()), *pixmap, pixmapRect);
		painter->drawText(textRect, AlignVCenter, text(column));
		break;
	}
	}
}

QString AlarmListViewItem::key(int column, bool) const
{
	switch (column)
	{
		case AlarmListView::TIME_COLUMN:    return mDateTimeOrder;
		case AlarmListView::REPEAT_COLUMN:  return mRepeatOrder;
		case AlarmListView::COLOUR_COLUMN:  return mColourOrder;
		case AlarmListView::MESSAGE_COLUMN:
		default:                            return text(column).lower();
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

