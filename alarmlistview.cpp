/*
 *  alarmlistview.cpp  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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

#include <qtooltip.h>
#include <qpainter.h>
#include <qstyle.h>
#include <qheader.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmtext.h"
#include "functions.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "alarmlistview.h"


class AlarmListTooltip : public QToolTip
{
	public:
		AlarmListTooltip(QWidget* parent) : QToolTip(parent) { }
	protected:
		virtual void maybeTip(const QPoint&);
};


/*=============================================================================
=  Class: AlarmListView
=  Displays the list of outstanding alarms.
=============================================================================*/
QValueList<EventListViewBase*>  AlarmListView::mInstanceList;


AlarmListView::AlarmListView(QWidget* parent, const char* name)
	: EventListViewBase(parent, name),
	  mTimeColumn(0),
	  mTimeToColumn(1),
	  mRepeatColumn(2),
	  mColourColumn(3),
	  mMessageColumn(4),
	  mDrawMessageInColour(false),
	  mShowExpired(false)
{
	setSelectionMode(QListView::Extended);

	addColumn(i18n("Time"));           // date/time column
	addColumn(i18n("Time To"));        // time-to-alarm column
	addColumn(i18n("Repeat"));         // repeat count column
	addColumn(QString::null);          // colour column
	addLastColumn(i18n("Message, File or Command"));
	setSorting(mTimeColumn);           // sort initially by date/time
	mTimeColumnHeaderWidth   = columnWidth(mTimeColumn);
	mTimeToColumnHeaderWidth = columnWidth(mTimeToColumn);
	setColumnAlignment(mRepeatColumn, Qt::AlignHCenter);
	setColumnWidthMode(mRepeatColumn, QListView::Maximum);

	// Set the width of the colour column in proportion to height
	setColumnWidth(mColourColumn, itemHeight() * 3/4);
	setColumnWidthMode(mColourColumn, QListView::Manual);

	mInstanceList.append(this);

	mTooltip = new AlarmListTooltip(viewport());
}

AlarmListView::~AlarmListView()
{
	delete mTooltip;
	mTooltip = 0;
	mInstanceList.remove(this);
}

/******************************************************************************
*  Add all the current alarms to the list.
*/
void AlarmListView::populate()
{
	KAEvent event;
	KCal::Event::List events;
	KCal::Event::List::ConstIterator it;
	QDateTime now = QDateTime::currentDateTime();
	if (mShowExpired)
	{
		AlarmCalendar* cal = AlarmCalendar::expiredCalendarOpen();
		if (cal)
		{
			events = cal->events();
                        for (it = events.begin();  it != events.end();  ++it)
			{
                                KCal::Event* kcalEvent = *it;
				if (kcalEvent->alarms().count() > 0)
				{
					event.set(*kcalEvent);
					addEntry(event, now);
				}
			}
		}
	}
	events = AlarmCalendar::activeCalendar()->events();
        for (it = events.begin();  it != events.end();  ++it)
	{
                KCal::Event* kcalEvent = *it;
		event.set(*kcalEvent);
		if (mShowExpired  ||  !event.expired())
			addEntry(event, now);
	}
}

/******************************************************************************
*  Set which time columns are to be displayed.
*/
void AlarmListView::selectTimeColumns(bool time, bool timeTo)
{
	if (!time  &&  !timeTo)
		return;       // always show at least one time column
	bool changed = false;
	int w = columnWidth(mTimeColumn);
	if (time  &&  !w)
	{
		// Unhide the time column
		int colWidth = mTimeColumnHeaderWidth;
		QFontMetrics fm = fontMetrics();
		for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
		{
			int w = item->width(fm, this, mTimeColumn);
			if (w > colWidth)
				colWidth = w;
		}
		setColumnWidth(mTimeColumn, colWidth);
		setColumnWidthMode(mTimeColumn, QListView::Maximum);
		changed = true;
	}
	else if (!time  &&  w)
	{
		// Hide the time column
		setColumnWidthMode(mTimeColumn, QListView::Manual);
		setColumnWidth(mTimeColumn, 0);
		changed = true;
	}
	w = columnWidth(mTimeToColumn);
	if (timeTo  &&  !w)
	{
		// Unhide the time-to-alarm column
		setColumnWidthMode(mTimeToColumn, QListView::Maximum);
		updateTimeToAlarms(true);
		if (columnWidth(mTimeToColumn) < mTimeToColumnHeaderWidth)
			setColumnWidth(mTimeToColumn, mTimeToColumnHeaderWidth);
		changed = true;
	}
	else if (!timeTo  &&  w)
	{
		// Hide the time-to-alarm column
		setColumnWidthMode(mTimeToColumn, QListView::Manual);
		setColumnWidth(mTimeToColumn, 0);
		changed = true;
	}
	if (changed)
		resizeLastColumn();
}

/******************************************************************************
*  Update all the values in the time-to-alarm column.
*/
void AlarmListView::updateTimeToAlarms(bool forceDisplay)
{
	if (forceDisplay  ||  columnWidth(mTimeToColumn))
	{
		QDateTime now = QDateTime::currentDateTime();
		for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
			item->updateTimeToAlarm(now, forceDisplay);
	}
}

/******************************************************************************
*  Add an event to every list instance.
*  The selection highlight is moved to the new event in the specified instance only.
*/
void AlarmListView::addEvent(const KAEvent& event, EventListViewBase* view)
{
	QDateTime now = QDateTime::currentDateTime();
	for (InstanceListConstIterator it = mInstanceList.begin();  it != mInstanceList.end();  ++it)
		static_cast<AlarmListView*>(*it)->addEntry(event, now, true, (*it == view));
}

/******************************************************************************
*  Add a new item to the list.
*/
AlarmListViewItem* AlarmListView::addEntry(const KAEvent& event, const QDateTime& now, bool setSize, bool reselect)
{
	if (!mShowExpired  &&  event.expired())
		return 0;
	AlarmListViewItem* item = new AlarmListViewItem(this, event, now);
	return static_cast<AlarmListViewItem*>(EventListViewBase::addEntry(item, setSize, reselect));
}

/******************************************************************************
*  Create a new list item for addEntry().
*/
EventListViewItemBase* AlarmListView::createItem(const KAEvent& event)
{
	return new AlarmListViewItem(this, event, QDateTime::currentDateTime());
}

/******************************************************************************
*  Check whether an item's alarm has expired.
*/
bool AlarmListView::expired(AlarmListViewItem* item) const
{
	return item->event().expired();
}

/******************************************************************************
*  Returns the QWhatsThis text for a specified column.
*/
QString AlarmListView::whatsThisText(int column) const
{
	if (column == mTimeColumn)
		return i18n("Next scheduled date and time of the alarm");
	if (column == mTimeToColumn)
		return i18n("How long until the next scheduled trigger of the alarm");
	if (column == mRepeatColumn)
		return i18n("How often the alarm recurs");
	if (column == mColourColumn)
		return i18n("Background color of alarm message");
	if (column == mMessageColumn)
		return i18n("Alarm message text, URL of text file to display, command to execute, or email subject line. The alarm type is indicated by the icon at the left.");
	return i18n("List of scheduled alarms");
}


/*=============================================================================
=  Class: AlarmListViewItem
=  Contains the details of one alarm for display in the AlarmListView.
=============================================================================*/

AlarmListViewItem::AlarmListViewItem(AlarmListView* parent, const KAEvent& event, const QDateTime& now)
	: EventListViewItemBase(parent, event),
	  mMessageLfStripped(false),
	  mTimeToAlarmShown(false)
{
	setLastColumnText();     // set the message column text

	DateTime dateTime = event.expired() ? event.startDateTime() : event.nextDateTime(false);
	if (parent->timeColumn() >= 0)
		setText(parent->timeColumn(), alarmTimeText(dateTime));
	if (parent->timeToColumn() >= 0)
	{
		QString tta = timeToAlarmText(now);
		setText(parent->timeToColumn(), tta);
		mTimeToAlarmShown = !tta.isNull();
	}
	QTime t = dateTime.time();
	mDateTimeOrder.sprintf("%04d%03d%02d%02d", dateTime.date().year(), dateTime.date().dayOfYear(),
	                                           t.hour(), t.minute());

	int repeatOrder = 0;
	int repeatInterval = 0;
	QString repeatText = event.recurrenceText(true);     // text displayed in Repeat column
	if (repeatText.isEmpty())
		repeatText = event.repetitionText(true);
	if (event.repeatAtLogin())
		repeatOrder = 1;
	else
	{
		repeatInterval = event.recurInterval();
		switch (event.recurType())
		{
			case KAEvent::MINUTELY:
				repeatOrder = 2;
				break;
			case KAEvent::DAILY:
				repeatOrder = 3;
				break;
			case KAEvent::WEEKLY:
				repeatOrder = 4;
				break;
			case KAEvent::MONTHLY_DAY:
			case KAEvent::MONTHLY_POS:
				repeatOrder = 5;
				break;
			case KAEvent::ANNUAL_DATE:
			case KAEvent::ANNUAL_POS:
			case KAEvent::ANNUAL_DAY:
				repeatOrder = 6;
				break;
			case KAEvent::NO_RECUR:
			default:
				break;
		}
	}
	setText(parent->repeatColumn(), repeatText);
	mRepeatOrder.sprintf("%c%08d", '0' + repeatOrder, repeatInterval);

	bool showColour = (event.action() == KAEvent::MESSAGE || event.action() == KAEvent::FILE);
	mColourOrder.sprintf("%06u", (showColour ? event.bgColour().rgb() : 0));
}

/******************************************************************************
*  Return the alarm summary text.
*  If 'full' is false, only a single line is returned.
*  If 'lfStripped' is non-null, it will be set true if the text is multi-line,
*  else false.
*/
QString AlarmListViewItem::alarmText(const KAEvent& event) const
{
	bool lfStripped;
	QString text = alarmText(event, false, &lfStripped);
	mMessageLfStripped = lfStripped;
	return text;
}

QString AlarmListViewItem::alarmText(const KAEvent& event, bool full, bool* lfStripped)
{
	QString text = (event.action() == KAEvent::EMAIL) ? event.emailSubject() : event.cleanText();
	if (event.action() == KAEvent::MESSAGE)
	{
		// If the message is the text of an email, return its headers or just subject line
		QString subject = AlarmText::emailHeaders(text, !full);
		if (!subject.isNull())
		{
			if (lfStripped)
				*lfStripped = true;
			return subject;
		}
	}
	if (full)
		return text;
	if (lfStripped)
		*lfStripped = false;
	int newline = text.find('\n');
	if (newline < 0)
		return text;       // it's a single-line text
	if (lfStripped)
		*lfStripped = true;
	return text.left(newline) + QString::fromLatin1("...");
}

/******************************************************************************
*  Return the alarm time text in the form "date time".
*/
QString AlarmListViewItem::alarmTimeText(const DateTime& dateTime) const
{
	QString dateTimeText = KGlobal::locale()->formatDate(dateTime.date(), true);
	if (!dateTime.isDateOnly())
	{
		dateTimeText += ' ';
		dateTimeText += KGlobal::locale()->formatTime(dateTime.time());
	}
	return dateTimeText + ' ';
}

/******************************************************************************
*  Return the time-to-alarm text.
*/
QString AlarmListViewItem::timeToAlarmText(const QDateTime& now) const
{
	if (event().expired())
{kdDebug(5950)<<" --- timeToAlarmText(): expired\n";
		return QString::null;
}
	DateTime dateTime = event().nextDateTime(false);
	if (dateTime.isDateOnly())
	{
		int days = now.date().daysTo(dateTime.date());
		return i18n("n days", " %1d ").arg(days);
	}
	int mins = (now.secsTo(dateTime.dateTime()) + 59) / 60;
	if (mins < 0)
{kdDebug(5950)<<" --- timeToAlarmText(): past\n";
		return QString::null;
}
	char minutes[3] = "00";
	minutes[0] = (mins%60) / 10 + '0';
	minutes[1] = (mins%60) % 10 + '0';
	if (mins < 24*60)
		return i18n("hours:minutes", " %1:%2 ").arg(mins/60).arg(minutes);
	int days = mins / (24*60);
	mins = mins % (24*60);
	return i18n("days hours:minutes", " %1d %2:%3 ").arg(days).arg(mins/60).arg(minutes);
}

/******************************************************************************
*  Update the displayed time-to-alarm value.
*  The updated value is only displayed if it is different from the existing value,
*  or if 'forceDisplay' is true.
*/
void AlarmListViewItem::updateTimeToAlarm(const QDateTime& now, bool forceDisplay)
{
	if (event().expired())
	{
		if (forceDisplay  ||  mTimeToAlarmShown)
		{
			setText(alarmListView()->timeToColumn(), QString::null);
			mTimeToAlarmShown = false;
		}
	}
	else
	{
		QString tta = timeToAlarmText(now);
kdDebug(5950)<<"---updateTimeToAlarm(): "<<tta<<endl;
		if (forceDisplay  ||  tta != text(alarmListView()->timeToColumn()))
			setText(alarmListView()->timeToColumn(), tta);
		mTimeToAlarmShown = !tta.isNull();
	}
}

/******************************************************************************
*  Paint one value in one of the columns in the list view.
*/
void AlarmListViewItem::paintCell(QPainter* painter, const QColorGroup& cg, int column, int width, int /*align*/)
{
	const AlarmListView* listView = alarmListView();
	int    margin = listView->itemMargin();
	QRect  box(margin, margin, width - margin*2, height() - margin*2);
	bool   selected = isSelected();
	QColor bgColour = selected ? cg.highlight() : cg.base();
	QColor fgColour = selected ? cg.highlightedText()
	                : !event().enabled() ? Preferences::instance()->disabledColour()
	                : event().expired() ? Preferences::instance()->expiredColour() : cg.text();
	painter->setPen(fgColour);
	painter->fillRect(0, 0, width, height(), bgColour);

	if (column == listView->timeColumn()
	||  column == listView->timeToColumn())
		painter->drawText(box, AlignVCenter, text(column));
	else if (column == listView->repeatColumn())
		painter->drawText(box, AlignVCenter | AlignHCenter, text(column));
	else if (column == listView->colourColumn())
	{
		// Paint the cell the colour of the alarm message
		if (event().action() == KAEvent::MESSAGE || event().action() == KAEvent::FILE)
			painter->fillRect(box, event().bgColour());
	}
	else if (column == listView->messageColumn())
	{
		QPixmap* pixmap = eventIcon();
		int frameWidth = listView->style().pixelMetric(QStyle::PM_DefaultFrameWidth);
		QRect pixmapRect = pixmap->rect();
		int diff = box.height() - pixmap->height();
		if (diff < 0)
		{
			pixmapRect.setTop(-diff / 2);
			pixmapRect.setHeight(box.height());
		}
		QRect iconRect(box.left(), box.top() + (diff > 0 ? diff / 2 : 0), pixmap->width(), (diff > 0 ? pixmap->height() : box.height()));
		QRect textRect = box;
		textRect.setLeft(box.left() + iconWidth() + 3*frameWidth);
		if (!selected  &&  listView->drawMessageInColour())
		{
			painter->fillRect(box, event().bgColour());
			painter->setBackgroundColor(event().bgColour());
//			painter->setPen(event().fgColour());
		}
		painter->drawPixmap(QPoint(iconRect.left() + frameWidth, iconRect.top()), *pixmap, pixmapRect);
		QString txt = text(column);
		painter->drawText(textRect, AlignVCenter, txt);
		mMessageColWidth = textRect.left() + listView->fontMetrics().boundingRect(txt).width();
	}
}

/******************************************************************************
*  Return the column sort order for one item in the list.
*/
QString AlarmListViewItem::key(int column, bool) const
{
	AlarmListView* listView = alarmListView();
	if (column == listView->timeColumn()
	||  column == listView->timeToColumn())
		return mDateTimeOrder;
	if (column == listView->repeatColumn())
		return mRepeatOrder;
	if (column == listView->colourColumn())
		return mColourOrder;
	return text(column).lower();
}


/*=============================================================================
=  Class: AlarmListTooltip
=  Displays the full alarm text in a tooltip when necessary.
=============================================================================*/

/******************************************************************************
*  Displays the full alarm text in a tooltip, if not all the text is displayed.
*/
void AlarmListTooltip::maybeTip(const QPoint& pt)
{
	AlarmListView* listView = (AlarmListView*)parentWidget()->parentWidget();
	int column = listView->messageColumn();
	int xOffset = listView->contentsX();
	if (listView->header()->sectionAt(pt.x() + xOffset) == column)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)listView->itemAt(pt);
		if (item)
		{
			int columnX = listView->header()->sectionPos(column) - xOffset;
			int columnWidth = listView->columnWidth(column);
			int widthNeeded = item->messageColWidthNeeded();
			if (!item->messageLfStripped()  &&  columnWidth >= widthNeeded)
			{
				if (columnX + widthNeeded <= listView->viewport()->width())
					return;
			}
			QRect rect = listView->itemRect(item);
			rect.setLeft(columnX);
			rect.setWidth(columnWidth);
			kdDebug(5950) << "AlarmListTooltip::maybeTip(): display\n";
			tip(rect, item->alarmText(item->event(), true));
		}
	}
}

