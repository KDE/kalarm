/*
 *  alarmlistview.cpp  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <qtooltip.h>
#include <qpainter.h>
#include <qstyle.h>
#include <qheader.h>
#include <qregexp.h>

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


class AlarmListTooltip : public QToolTip
{
	public:
		AlarmListTooltip(QWidget* parent) : QToolTip(parent) { }
		virtual ~AlarmListTooltip() {}
	protected:
		virtual void maybeTip(const QPoint&);
};


/*=============================================================================
=  Class: AlarmListView
=  Displays the list of outstanding alarms.
=============================================================================*/
QValueList<EventListViewBase*>  AlarmListView::mInstanceList;
bool                            AlarmListView::mDragging = false;


AlarmListView::AlarmListView(const QValueList<int>& order, QWidget* parent, const char* name)
	: EventListViewBase(parent, name),
	  mMousePressed(false),
	  mDrawMessageInColour(false),
	  mShowExpired(false)
{
	static QString titles[COLUMN_COUNT] = {
		i18n("Time"),
		i18n("Time To"),
		i18n("Repeat"),
		QString::null,
		QString::null,
		i18n("Message, File or Command")
	};

	setSelectionMode(QListView::Extended);

	// Set the column order
	int i;
	bool ok = false;
	if (order.count() >= COLUMN_COUNT)
	{
		// The column order is specified
		bool posns[COLUMN_COUNT];
		for (i = 0;  i < COLUMN_COUNT;  ++i)
			posns[i] = false;
		for (i = 0;  i < COLUMN_COUNT;  ++i)
		{
			int ord = order[i];
			if (ord < COLUMN_COUNT  &&  ord >= 0)
			{
				mColumn[i] = ord;
				posns[ord] = true;
			}
		}
		ok = true;
		for (i = 0;  i < COLUMN_COUNT;  ++i)
			if (!posns[i])
				ok = false;
		if (ok  &&  mColumn[MESSAGE_COLUMN] != MESSAGE_COLUMN)
		{
			// Shift the message column to be last, since otherwise
			// column widths get screwed up.
			int messageCol = mColumn[MESSAGE_COLUMN];
			for (i = 0;  i < COLUMN_COUNT;  ++i)
				if (mColumn[i] > messageCol)
					--mColumn[i];
			mColumn[MESSAGE_COLUMN] = MESSAGE_COLUMN;
		}
	}
	if (!ok)
	{
		// Either no column order was specified, or it was invalid,
		// so use the default order
		for (i = 0;  i < COLUMN_COUNT;  ++i)
			mColumn[i] = i;
	}

	// Initialise the columns
	for (i = 0;  i < COLUMN_COUNT;  ++i)
	{
		for (int j = 0;  j < COLUMN_COUNT;  ++j)
			if (mColumn[j] == i)
			{
				if (j != MESSAGE_COLUMN)
					addColumn(titles[j]);
				break;
			}
	}
	addLastColumn(titles[MESSAGE_COLUMN]);

	setSorting(mColumn[TIME_COLUMN]);           // sort initially by date/time
	mTimeColumnHeaderWidth   = columnWidth(mColumn[TIME_COLUMN]);
	mTimeToColumnHeaderWidth = columnWidth(mColumn[TIME_TO_COLUMN]);
	setColumnAlignment(mColumn[REPEAT_COLUMN], Qt::AlignHCenter);
	setColumnWidthMode(mColumn[REPEAT_COLUMN], QListView::Maximum);

	// Set the width of the colour column in proportion to height
	setColumnWidth(mColumn[COLOUR_COLUMN], itemHeight() * 3/4);
	setColumnWidthMode(mColumn[COLOUR_COLUMN], QListView::Manual);

	// Set the width of the alarm type column to exactly accommodate the icons.
	// Don't allow the user to resize it (to avoid refresh problems, and bearing
	// in mind that resizing doesn't seem very useful anyway).
	setColumnWidth(mColumn[TYPE_COLUMN], AlarmListViewItem::typeIconWidth(this));
	setColumnWidthMode(mColumn[TYPE_COLUMN], QListView::Manual);
	header()->setResizeEnabled(false, mColumn[TYPE_COLUMN]);

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
	int w = columnWidth(mColumn[TIME_COLUMN]);
	if (time  &&  !w)
	{
		// Unhide the time column
		int colWidth = mTimeColumnHeaderWidth;
		QFontMetrics fm = fontMetrics();
		for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
		{
			int w = item->width(fm, this, mColumn[TIME_COLUMN]);
			if (w > colWidth)
				colWidth = w;
		}
		setColumnWidth(mColumn[TIME_COLUMN], colWidth);
		setColumnWidthMode(mColumn[TIME_COLUMN], QListView::Maximum);
		changed = true;
	}
	else if (!time  &&  w)
	{
		// Hide the time column
		setColumnWidthMode(mColumn[TIME_COLUMN], QListView::Manual);
		setColumnWidth(mColumn[TIME_COLUMN], 0);
		changed = true;
	}
	w = columnWidth(mColumn[TIME_TO_COLUMN]);
	if (timeTo  &&  !w)
	{
		// Unhide the time-to-alarm column
		setColumnWidthMode(mColumn[TIME_TO_COLUMN], QListView::Maximum);
		updateTimeToAlarms(true);
		if (columnWidth(mColumn[TIME_TO_COLUMN]) < mTimeToColumnHeaderWidth)
			setColumnWidth(mColumn[TIME_TO_COLUMN], mTimeToColumnHeaderWidth);
		changed = true;
	}
	else if (!timeTo  &&  w)
	{
		// Hide the time-to-alarm column
		setColumnWidthMode(mColumn[TIME_TO_COLUMN], QListView::Manual);
		setColumnWidth(mColumn[TIME_TO_COLUMN], 0);
		changed = true;
	}
	if (changed)
	{
		resizeLastColumn();
		triggerUpdate();   // ensure scroll bar appears if needed
	}
}

/******************************************************************************
*  Update all the values in the time-to-alarm column.
*/
void AlarmListView::updateTimeToAlarms(bool forceDisplay)
{
	if (forceDisplay  ||  columnWidth(mColumn[TIME_TO_COLUMN]))
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
*  Return the column order.
*/
QValueList<int> AlarmListView::columnOrder() const
{
	QHeader* hdr = header();
	int order[COLUMN_COUNT];
	order[TIME_COLUMN]    = hdr->mapToIndex(mColumn[TIME_COLUMN]);
	order[TIME_TO_COLUMN] = hdr->mapToIndex(mColumn[TIME_TO_COLUMN]); 
	order[REPEAT_COLUMN]  = hdr->mapToIndex(mColumn[REPEAT_COLUMN]); 
	order[COLOUR_COLUMN]  = hdr->mapToIndex(mColumn[COLOUR_COLUMN]); 
	order[TYPE_COLUMN]    = hdr->mapToIndex(mColumn[TYPE_COLUMN]);
	order[MESSAGE_COLUMN] = hdr->mapToIndex(mColumn[MESSAGE_COLUMN]);
	QValueList<int> orderList;
	for (int i = 0;  i < COLUMN_COUNT;  ++i)
		orderList += order[i];
	return orderList;
}

/******************************************************************************
*  Returns the QWhatsThis text for a specified column.
*/
QString AlarmListView::whatsThisText(int column) const
{
	if (column == mColumn[TIME_COLUMN])
		return i18n("Next scheduled date and time of the alarm");
	if (column == mColumn[TIME_TO_COLUMN])
		return i18n("How long until the next scheduled trigger of the alarm");
	if (column == mColumn[REPEAT_COLUMN])
		return i18n("How often the alarm recurs");
	if (column == mColumn[COLOUR_COLUMN])
		return i18n("Background color of alarm message");
	if (column == mColumn[TYPE_COLUMN])
		return i18n("Alarm type (message, file, command or email)");
	if (column == mColumn[MESSAGE_COLUMN])
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
	QListView::contentsMousePressEvent(e);
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
	QListView::contentsMouseMoveEvent(e);
	if (mMousePressed
	&&  (mMousePressPos - e->pos()).manhattanLength() > QApplication::startDragDistance())
	{
		// Create a calendar object containing all the currently selected alarms
		kdDebug(5950) << "AlarmListView::contentsMouseMoveEvent(): drag started" << endl;
		mMousePressed = false;
		KCal::CalendarLocal cal(QString::fromLatin1("UTC"));
		cal.setLocalTime();    // write out using local time (i.e. no time zone)
		QValueList<EventListViewItemBase*> items = selectedItems();
		if (!items.count())
			return;
		for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
		{
			const KAEvent& event = (*it)->event();
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
	QListView::contentsMouseReleaseEvent(e);
	mMousePressed = false;
	mDragging     = false;
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

	DateTime dateTime = event.expired() ? event.startDateTime() : event.displayDateTime();
	if (parent->column(AlarmListView::TIME_COLUMN) >= 0)
		setText(parent->column(AlarmListView::TIME_COLUMN), alarmTimeText(dateTime));
	if (parent->column(AlarmListView::TIME_TO_COLUMN) >= 0)
	{
		QString tta = timeToAlarmText(now);
		setText(parent->column(AlarmListView::TIME_TO_COLUMN), tta);
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
	setText(parent->column(AlarmListView::REPEAT_COLUMN), repeatText);
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
		dateTimeText += ' ';
		QString time = locale->formatTime(dateTime.time());
		if (mTimeHourPos == -2)
		{
			// Initialise the position of the hour within the time string, if leading
			// zeroes are omitted, so that displayed times can be aligned with each other.
			mTimeHourPos = -1;     // default = alignment isn't possible/sensible
			if (!QApplication::reverseLayout())    // don't try to align right-to-left languages
			{
				QString fmt = locale->timeFormat();
				int i = fmt.find(QRegExp("%[kl]"));   // check if leading zeroes are omitted
				if (i >= 0  &&  i == fmt.find('%'))   // and whether the hour is first
					mTimeHourPos = i;             // yes, so need to align
			}
		}
		if (mTimeHourPos >= 0  &&  (int)time.length() > mTimeHourPos + 1
		&&  time[mTimeHourPos].isDigit()  &&  !time[mTimeHourPos + 1].isDigit())
			dateTimeText += '~';     // improve alignment of times with no leading zeroes
		dateTimeText += time;
	}
	return dateTimeText + ' ';
}

/******************************************************************************
*  Return the time-to-alarm text.
*/
QString AlarmListViewItem::timeToAlarmText(const QDateTime& now) const
{
	if (event().expired())
		return QString::null;
	DateTime dateTime = event().displayDateTime();
	if (dateTime.isDateOnly())
	{
		int days = now.date().daysTo(dateTime.date());
		return i18n("n days", " %1d ").arg(days);
	}
	int mins = (now.secsTo(dateTime.dateTime()) + 59) / 60;
	if (mins < 0)
		return QString::null;
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
			setText(alarmListView()->column(AlarmListView::TIME_TO_COLUMN), QString::null);
			mTimeToAlarmShown = false;
		}
	}
	else
	{
		QString tta = timeToAlarmText(now);
		if (forceDisplay  ||  tta != text(alarmListView()->column(AlarmListView::TIME_TO_COLUMN)))
			setText(alarmListView()->column(AlarmListView::TIME_TO_COLUMN), tta);
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
	QColor bgColour = selected ? cg.highlight() : cg.base();
	QColor fgColour = selected ? cg.highlightedText()
	                : !event().enabled() ? Preferences::disabledColour()
	                : event().expired() ? Preferences::expiredColour() : cg.text();
	painter->setPen(fgColour);
	painter->fillRect(0, 0, width, height(), bgColour);

	if (column == listView->column(AlarmListView::TIME_COLUMN))
	{
		int i = -1;
		QString str = text(column);
		if (mTimeHourPos >= 0)
		{
			// Need to pad out spacing to align times without leading zeroes
			i = str.find(" ~");
			if (i >= 0)
			{
				if (mDigitWidth < 0)
					mDigitWidth = painter->fontMetrics().width("0");
				QString date = str.left(i + 1);
				int w = painter->fontMetrics().width(date) + mDigitWidth;
				painter->drawText(box, AlignVCenter, date);
				box.setLeft(box.left() + w);
				painter->drawText(box, AlignVCenter, str.mid(i + 2));
			}
		}
		if (i < 0)
			painter->drawText(box, AlignVCenter, str);
	}
	else if (column == listView->column(AlarmListView::TIME_TO_COLUMN))
		painter->drawText(box, AlignVCenter | AlignRight, text(column));
	else if (column == listView->column(AlarmListView::REPEAT_COLUMN))
		painter->drawText(box, AlignVCenter | AlignHCenter, text(column));
	else if (column == listView->column(AlarmListView::COLOUR_COLUMN))
	{
		// Paint the cell the colour of the alarm message
		if (event().action() == KAEvent::MESSAGE || event().action() == KAEvent::FILE)
			painter->fillRect(box, event().bgColour());
	}
	else if (column == listView->column(AlarmListView::TYPE_COLUMN))
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
	else if (column == listView->column(AlarmListView::MESSAGE_COLUMN))
	{
		if (!selected  &&  listView->drawMessageInColour())
		{
			painter->fillRect(box, event().bgColour());
			painter->setBackgroundColor(event().bgColour());
//			painter->setPen(event().fgColour());
		}
		QString txt = text(column);
		painter->drawText(box, AlignVCenter, txt);
		mMessageColWidth = listView->fontMetrics().boundingRect(txt).width();
	}
}

/******************************************************************************
*  Return the width needed for the icons in the alarm type column.
*/
int AlarmListViewItem::typeIconWidth(AlarmListView* v)
{
	return iconWidth() +  2 * v->style().pixelMetric(QStyle::PM_DefaultFrameWidth);
}

/******************************************************************************
*  Return the column sort order for one item in the list.
*/
QString AlarmListViewItem::key(int column, bool) const
{
	AlarmListView* listView = alarmListView();
	if (column == listView->column(AlarmListView::TIME_COLUMN)
	||  column == listView->column(AlarmListView::TIME_TO_COLUMN))
		return mDateTimeOrder;
	if (column == listView->column(AlarmListView::REPEAT_COLUMN))
		return mRepeatOrder;
	if (column == listView->column(AlarmListView::COLOUR_COLUMN))
		return mColourOrder;
	if (column == listView->column(AlarmListView::TYPE_COLUMN))
		return mTypeOrder;
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
	int column = listView->column(AlarmListView::MESSAGE_COLUMN);
	int xOffset = listView->contentsX();
	if (listView->header()->sectionAt(pt.x() + xOffset) == column)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)listView->itemAt(pt);
		if (item)
		{
			int columnX = listView->header()->sectionPos(column) - xOffset;
			int columnWidth = listView->columnWidth(column);
			int widthNeeded = item->messageColWidthNeeded();
			if (!item->messageTruncated()  &&  columnWidth >= widthNeeded)
			{
				if (columnX + widthNeeded <= listView->viewport()->width())
					return;
			}
			QRect rect = listView->itemRect(item);
			rect.setLeft(columnX);
			rect.setWidth(columnWidth);
			kdDebug(5950) << "AlarmListTooltip::maybeTip(): display\n";
			tip(rect, AlarmText::summary(item->event(), 10));    // display up to 10 lines of text
		}
	}
}

