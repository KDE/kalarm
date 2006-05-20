/*
 *  alarmlistview.cpp  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  Copyright © 2001-2005 by David Jarvie <software@astrojar.org.uk>
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

#include <QToolTip>
#include <QPainter>
#include <QStyle>
#include <QRegExp>
//Added by qt3to4:
#include <q3header.h>
#include <QPixmap>
#include <QList>
#include <QMouseEvent>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include <libkcal/icaldrag.h>
#include <libkcal/calendarlocal.h>

#include "alarmcalendar.h"
#include "alarmtext.h"
#include "functions.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "alarmlistview.moc"


/*=============================================================================
=  Class: AlarmListView
=  Displays the list of outstanding alarms.
=============================================================================*/
QList<EventListViewBase*>  AlarmListView::mInstanceList;
bool                       AlarmListView::mDragging = false;


AlarmListView::AlarmListView(QWidget* parent)
	: EventListViewBase(parent),
	  mTimeColumn(0),
	  mTimeToColumn(1),
	  mRepeatColumn(2),
	  mColourColumn(3),
	  mTypeColumn(4),
	  mMessageColumn(5),
	  mMousePressed(false),
	  mDrawMessageInColour(false),
	  mShowArchived(false)
{
	setSelectionMode(Q3ListView::Extended);

	addColumn(i18n("Time"));           // date/time column
	addColumn(i18n("Time To"));        // time-to-alarm column
	addColumn(i18n("Repeat"));         // repeat count column
	addColumn(QString());          // colour column
	addColumn(QString());          // alarm type column
	addLastColumn(i18n("Message, File or Command"));
	setSorting(mTimeColumn);           // sort initially by date/time
	mTimeColumnHeaderWidth   = columnWidth(mTimeColumn);
	mTimeToColumnHeaderWidth = columnWidth(mTimeToColumn);
	setColumnAlignment(mRepeatColumn, Qt::AlignHCenter);
	setColumnWidthMode(mRepeatColumn, Q3ListView::Maximum);

	// Set the width of the colour column in proportion to height
	setColumnWidth(mColourColumn, itemHeight() * 3/4);
	setColumnWidthMode(mColourColumn, Q3ListView::Manual);

	// Set the width of the alarm type column to exactly accommodate the icons
	setColumnWidth(mTypeColumn, AlarmListViewItem::typeIconWidth(this));
	setColumnWidthMode(mTypeColumn, Q3ListView::Manual);

	mInstanceList.append(this);
}

AlarmListView::~AlarmListView()
{
	mInstanceList.removeAt(mInstanceList.indexOf(this));
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
	if (mShowArchived)
	{
		AlarmCalendar* cal = AlarmCalendar::archiveCalendarOpen();
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
		if (mShowArchived  ||  !event.expired())
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
		setColumnWidthMode(mTimeColumn, Q3ListView::Maximum);
		changed = true;
	}
	else if (!time  &&  w)
	{
		// Hide the time column
		setColumnWidthMode(mTimeColumn, Q3ListView::Manual);
		setColumnWidth(mTimeColumn, 0);
		changed = true;
	}
	w = columnWidth(mTimeToColumn);
	if (timeTo  &&  !w)
	{
		// Unhide the time-to-alarm column
		setColumnWidthMode(mTimeToColumn, Q3ListView::Maximum);
		updateTimeToAlarms(true);
		if (columnWidth(mTimeToColumn) < mTimeToColumnHeaderWidth)
			setColumnWidth(mTimeToColumn, mTimeToColumnHeaderWidth);
		changed = true;
	}
	else if (!timeTo  &&  w)
	{
		// Hide the time-to-alarm column
		setColumnWidthMode(mTimeToColumn, Q3ListView::Manual);
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
	for (int i = 0, end = mInstanceList.count();  i < end;  ++i)
		static_cast<AlarmListView*>(mInstanceList[i])->addEntry(event, now, true, (mInstanceList[i] == view));
}

/******************************************************************************
*  Add a new item to the list.
*/
AlarmListViewItem* AlarmListView::addEntry(const KAEvent& event, const QDateTime& now, bool setSize, bool reselect)
{
	if (!mShowArchived  &&  event.expired())
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
*  Check whether an item's alarm is archived.
*/
bool AlarmListView::archived(AlarmListViewItem* item) const
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
	if (column == mTypeColumn)
		return i18n("Alarm type (message, file, command or email)");
	if (column == mMessageColumn)
		return i18n("Alarm message text, URL of text file to display, command to execute, or email subject line");
	return i18n("List of scheduled alarms");
}

/******************************************************************************
*  Called when the mouse button is pressed.
*  Records the position of the mouse when the left button is pressed, for use
*  in drag operations.
*/
void AlarmListView::contentsMousePressEvent(QMouseEvent* e)
{
	Q3ListView::contentsMousePressEvent(e);
	if (e->button() == Qt::LeftButton)
	{
		QPoint p(contentsToViewport(e->pos()));
		if (itemAt(p))
		{
			mMousePressPos = e->pos();
			mMousePressed  = true;
		}
		mDragging = false;
	}
}

/******************************************************************************
*  Called when the mouse is moved.
*  Creates a drag object when the mouse drags one or more selected items.
*/
void AlarmListView::contentsMouseMoveEvent(QMouseEvent* e)
{
	Q3ListView::contentsMouseMoveEvent(e);
	if (mMousePressed
	&&  (mMousePressPos - e->pos()).manhattanLength() > QApplication::startDragDistance())
	{
		// Create a calendar object containing all the currently selected alarms
		kDebug(5950) << "AlarmListView::contentsMouseMoveEvent(): drag started" << endl;
		mMousePressed = false;
		KCal::CalendarLocal cal(QLatin1String("UTC"));
		cal.setLocalTime();    // write out using local time (i.e. no time zone)
		QList<EventListViewItemBase*> items = selectedItems();
		if (items.isEmpty())
			return;
		for (int i = 0, end = items.count();  i < end;  ++i)
		{
			const KAEvent& event = items[i]->event();
			KCal::Event* kcalEvent = new KCal::Event;
			event.updateKCalEvent(*kcalEvent, false, true);
			kcalEvent->setUid(event.id());
			cal.addEvent(kcalEvent);
		}

		// Create the drag object for the destination program to receive
		mDragging = true;
		KCal::ICalDrag* dobj = new KCal::ICalDrag(&cal, this);
		dobj->dragCopy();       // the drag operation will copy the alarms
	}
}

/******************************************************************************
*  Called when the mouse button is released.
*  Notes that the mouse left button is no longer pressed, for use in drag
*  operations.
*/
void AlarmListView::contentsMouseReleaseEvent(QMouseEvent *e)
{
	Q3ListView::contentsMouseReleaseEvent(e);
	mMousePressed = false;
	mDragging     = false;
}

/******************************************************************************
*  Called when any event occurs.
*  Displays the full alarm text in a tooltip, if not all the text is displayed.
*/
bool AlarmListView::event(QEvent *e)
{
	if (e->type() == QEvent::ToolTip)
	{
		QHelpEvent* he = (QHelpEvent*)e;
		QPoint pt = he->pos();
		int xOffset = contentsX();
		if (header()->sectionAt(pt.x() + xOffset) == mMessageColumn)
		{
			AlarmListViewItem* item = (AlarmListViewItem*)itemAt(pt);
			if (item)
			{
				int columnX = header()->sectionPos(mMessageColumn) - xOffset;
				int colWidth = columnWidth(mMessageColumn);
				int widthNeeded = item->messageColWidthNeeded();
				if (!item->messageTruncated()  &&  colWidth >= widthNeeded)
				{
					if (columnX + widthNeeded <= viewport()->width())
						return EventListViewBase::event(e);
				}
//				QRect rect = itemRect(item);
//				rect.setLeft(columnX);
//				rect.setWidth(colWidth);
				kDebug(5950) << "AlarmListView::event(): display\n";
				QToolTip::showText(pt, AlarmText::summary(item->event(), 10));    // display up to 10 lines of text
				return true;
			}
		}
	}
	return EventListViewBase::event(e);
}


/*=============================================================================
=  Class: AlarmListViewItem
=  Contains the details of one alarm for display in the AlarmListView.
=============================================================================*/
int AlarmListViewItem::mTimeHourPos = -2;
int AlarmListViewItem::mDigitWidth  = -1;

AlarmListViewItem::AlarmListViewItem(AlarmListView* parent, const KAEvent& event, const QDateTime& now)
	: EventListViewItemBase(parent, event),
	  mMessageTruncated(false),
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
			case KARecurrence::MINUTELY:
				repeatOrder = 2;
				break;
			case KARecurrence::DAILY:
				repeatOrder = 3;
				break;
			case KARecurrence::WEEKLY:
				repeatOrder = 4;
				break;
			case KARecurrence::MONTHLY_DAY:
			case KARecurrence::MONTHLY_POS:
				repeatOrder = 5;
				break;
			case KARecurrence::ANNUAL_DATE:
			case KARecurrence::ANNUAL_POS:
				repeatOrder = 6;
				break;
			case KARecurrence::NO_RECUR:
			default:
				break;
		}
	}
	setText(parent->repeatColumn(), repeatText);
	mRepeatOrder.sprintf("%c%08d", '0' + repeatOrder, repeatInterval);

	bool showColour = (event.action() == KAEvent::MESSAGE || event.action() == KAEvent::FILE);
	mColourOrder.sprintf("%06u", (showColour ? event.bgColour().rgb() : 0));

	mTypeOrder.sprintf("%02d", event.action());
}

/******************************************************************************
*  Return the single line alarm summary text.
*/
QString AlarmListViewItem::alarmText(const KAEvent& event) const
{
	bool truncated;
	QString text = AlarmText::summary(event, 1, &truncated);
	mMessageTruncated = truncated;
	return text;
}

/******************************************************************************
*  Return the alarm time text in the form "date time".
*/
QString AlarmListViewItem::alarmTimeText(const DateTime& dateTime) const
{
	KLocale* locale = KGlobal::locale();
	QString dateTimeText = locale->formatDate(dateTime.date(), true);
	if (!dateTime.isDateOnly())
	{
		dateTimeText += QLatin1Char(' ');
		QString time = locale->formatTime(dateTime.time());
		if (mTimeHourPos == -2)
		{
			// Initialise the position of the hour within the time string, if leading
			// zeroes are omitted, so that displayed times can be aligned with each other.
			mTimeHourPos = -1;     // default = alignment isn't possible/sensible
			if (QApplication::isLeftToRight())    // don't try to align right-to-left languages
			{
				QString fmt = locale->timeFormat();
				int i = fmt.indexOf(QRegExp("%[kl]"));   // check if leading zeroes are omitted
				if (i >= 0  &&  i == fmt.indexOf(QLatin1Char('%')))   // and whether the hour is first
					mTimeHourPos = i;             // yes, so need to align
			}
		}
		if (mTimeHourPos >= 0  &&  (int)time.length() > mTimeHourPos + 1
		&&  time[mTimeHourPos].isDigit()  &&  !time[mTimeHourPos + 1].isDigit())
			dateTimeText += QLatin1Char('~');     // improve alignment of times with no leading zeroes
		dateTimeText += time;
	}
	return dateTimeText + QLatin1Char(' ');
}

/******************************************************************************
*  Return the time-to-alarm text.
*/
QString AlarmListViewItem::timeToAlarmText(const QDateTime& now) const
{
	if (event().expired())
		return QString();
	DateTime dateTime = event().nextDateTime(false);
	if (dateTime.isDateOnly())
	{
		int days = now.date().daysTo(dateTime.date());
		return i18nc("n days", " %1d ", days);
	}
	int mins = (now.secsTo(dateTime.dateTime()) + 59) / 60;
	if (mins < 0)
		return QString();
	char minutes[3] = "00";
	minutes[0] = (mins%60) / 10 + '0';
	minutes[1] = (mins%60) % 10 + '0';
	if (mins < 24*60)
		return i18nc("hours:minutes", " %1:%2 ", mins/60, minutes);
	int days = mins / (24*60);
	mins = mins % (24*60);
	return i18nc("days hours:minutes", " %1d %2:%3 ", days, mins/60, minutes);
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
			setText(alarmListView()->timeToColumn(), QString());
			mTimeToAlarmShown = false;
		}
	}
	else
	{
		QString tta = timeToAlarmText(now);
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
	QRect  box(margin, margin, width - margin*2, height() - margin*2);   // area within which to draw
	bool   selected = isSelected();
	QColor bgColour = selected ? cg.color(QPalette::Highlight) : cg.color(QPalette::Base);
	QColor fgColour = selected ? cg.color(QPalette::HighlightedText)
	                : !event().enabled() ? Preferences::disabledColour()
	                : event().expired() ? Preferences::archivedColour() : cg.color(QPalette::Text);
	painter->setPen(fgColour);
	painter->fillRect(0, 0, width, height(), bgColour);

	if (column == listView->timeColumn())
	{
		int i = -1;
		QString str = text(column);
		if (mTimeHourPos >= 0)
		{
			// Need to pad out spacing to align times without leading zeroes
			i = str.indexOf(" ~");
			if (i >= 0)
			{
				if (mDigitWidth < 0)
					mDigitWidth = painter->fontMetrics().width("0");
				QString date = str.left(i + 1);
				int w = painter->fontMetrics().width(date) + mDigitWidth;
				painter->drawText(box, Qt::AlignVCenter, date);
				box.setLeft(box.left() + w);
				painter->drawText(box, Qt::AlignVCenter, str.mid(i + 2));
			}
		}
		if (i < 0)
			painter->drawText(box, Qt::AlignVCenter, str);
	}
	else if (column == listView->timeToColumn())
		painter->drawText(box, Qt::AlignVCenter | Qt::AlignRight, text(column));
	else if (column == listView->repeatColumn())
		painter->drawText(box, Qt::AlignVCenter | Qt::AlignHCenter, text(column));
	else if (column == listView->colourColumn())
	{
		// Paint the cell the colour of the alarm message
		if (event().action() == KAEvent::MESSAGE || event().action() == KAEvent::FILE)
			painter->fillRect(box, event().bgColour());
	}
	else if (column == listView->typeColumn())
	{
		// Display the alarm type icon, horizontally and vertically centred in the cell
		QPixmap* pixmap = eventIcon();
		QRect pixmapRect = pixmap->rect();
		int diff = box.height() - pixmap->height();
		if (diff < 0)
		{
			pixmapRect.setTop(-diff / 2);
			pixmapRect.setHeight(box.height());
		}
		QPoint iconTopLeft(box.left() + (box.width() - pixmapRect.width()) / 2,
		                   box.top() + (diff > 0 ? diff / 2 : 0));
		painter->drawPixmap(iconTopLeft, *pixmap, pixmapRect);
	}
	else if (column == listView->messageColumn())
	{
		if (!selected  &&  listView->drawMessageInColour())
		{
			painter->fillRect(box, event().bgColour());
			painter->setBackground(event().bgColour());
//			painter->setPen(event().fgColour());
		}
		QString txt = text(column);
		painter->drawText(box, Qt::AlignVCenter, txt);
		mMessageColWidth = listView->fontMetrics().boundingRect(txt).width();
	}
}

/******************************************************************************
*  Return the width needed for the icons in the alarm type column.
*/
int AlarmListViewItem::typeIconWidth(AlarmListView* v)
{
	return iconWidth() +  2 * v->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
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
	if (column == listView->typeColumn())
		return mTypeOrder;
	return text(column).toLower();
}
