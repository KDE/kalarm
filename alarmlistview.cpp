/*
 *  alarmlistview.cpp  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
#include "preferences.h"
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


/*=============================================================================
=  Class: AlarmListView
=  Displays the list of outstanding alarms.
=============================================================================*/

AlarmListView::AlarmListView(QWidget* parent, const char* name)
	: KListView(parent, name),
	  mTimeColumn(0),
	  mTimeToColumn(1),
	  mRepeatColumn(2),
	  mColourColumn(3),
	  mMessageColumn(4),
	  mDrawMessageInColour(false),
	  mShowExpired(false)
{
	setMultiSelection(true);
	setSelectionMode(QListView::Extended);

	addColumn(i18n("Time"));           // date/time column
	addColumn(i18n("Time To"));        // time-to-alarm column
	addColumn(i18n("Repeat"));         // repeat count column
	addColumn(QString::null);          // colour column
	addColumn(i18n("Message, File or Command"));
	setColumnWidthMode(mMessageColumn, QListView::Maximum);
	setAllColumnsShowFocus(true);
	setSorting(mTimeColumn);           // sort initially by date/time
	setShowSortIndicator(true);
	mTimeColumnHeaderWidth   = columnWidth(mTimeColumn);
	mTimeToColumnHeaderWidth = columnWidth(mTimeToColumn);
	mLastColumnHeaderWidth = columnWidth(mMessageColumn);
	setColumnAlignment(mRepeatColumn, Qt::AlignHCenter);
	setColumnWidthMode(mRepeatColumn, QListView::Maximum);

	// Find the height of the list items, and set the width of the colour column accordingly
	setColumnWidth(mColourColumn, itemHeight() * 3/4);
	setColumnWidthMode(mColourColumn, QListView::Manual);
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
	QDateTime now = QDateTime::currentDateTime();
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
					addEntry(event, now);
				}
			}
		}
	}
	events = theApp()->getCalendar().events();
	for (Event* kcalEvent = events.first();  kcalEvent;  kcalEvent = events.next())
	{
		event.set(*kcalEvent);
		if (mShowExpired  ||  !event.expired())
			addEntry(event, now);
	}
	resizeLastColumn();
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

AlarmListViewItem* AlarmListView::getEntry(const QString& eventID)
{
	for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
		if (item->event().id() == eventID)
			return item;
	return 0;
}

AlarmListViewItem* AlarmListView::addEntry(const KAlarmEvent& event, const QDateTime& now, bool setSize)
{
	if (!mShowExpired  &&  event.expired())
		return 0;
	AlarmListViewItem* item = new AlarmListViewItem(this, event, now);
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
	int messageWidth = mLastColumnHeaderWidth;
	for (AlarmListViewItem* item = firstChild();  item;  item = item->nextSibling())
	{
		int mw = item->messageWidth();
		if (mw > messageWidth)
			messageWidth = mw;
	}
	int x = header()->sectionPos(mMessageColumn);
	int width = visibleWidth();
	width -= x;
	if (width < messageWidth)
		width = messageWidth;
	setColumnWidth(mMessageColumn, width);
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

/******************************************************************************
*  Fetches the single selected item.
*  Reply = null if no items are selected, or if multiple items are selected.
*/
AlarmListViewItem* AlarmListView::singleSelectedItem() const
{
	QListViewItem* item = 0;
	for (QListViewItem* it = firstChild();  it;  it = it->nextSibling())
	{
		if (isSelected(it))
		{
			if (item)
				return 0;
			item = it;
		}
	}
	return (AlarmListViewItem*)item;
}

/******************************************************************************
*  Fetches all selected items.
*/
QPtrList<AlarmListViewItem> AlarmListView::selectedItems() const
{
	QPtrList<AlarmListViewItem> items;
	items.setAutoDelete(false);
	for (QListViewItem* item = firstChild();  item;  item = item->nextSibling())
	{
		if (isSelected(item))
			items.append((AlarmListViewItem*)item);
	}
	return items;
}

/******************************************************************************
*  Returns how many items are selected.
*/
int AlarmListView::selectedCount() const
{
	int count = 0;
	for (QListViewItem* item = firstChild();  item;  item = item->nextSibling())
	{
		if (isSelected(item))
			++count;
	}
	return count;
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

AlarmListViewItem::AlarmListViewItem(AlarmListView* parent, const KAlarmEvent& event, const QDateTime& now)
	: QListViewItem(parent),
	  mEvent(event),
	  mTimeToAlarmShown(false)
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

	setText(parent->messageColumn(), alarmText(event));
	mMessageWidth = width(parent->fontMetrics(), parent, parent->messageColumn());

	if (parent->timeColumn() >= 0)
		setText(parent->timeColumn(), alarmTimeText());
	if (parent->timeToColumn() >= 0)
	{
		QString tta = timeToAlarmText(now);
		setText(parent->timeToColumn(), tta);
		mTimeToAlarmShown = !tta.isNull();
	}
	DateTime dateTime = event.expired() ? event.startDateTime() : event.nextDateTime();
	QTime t = dateTime.time();
	mDateTimeOrder.sprintf("%04d%03d%02d%02d", dateTime.date().year(), dateTime.date().dayOfYear(),
	                                           t.hour(), t.minute());

	int repeatOrder = 0;
	int repeatInterval = 0;
	QString repeatText = event.recurrenceText(true);     // text displayed in Repeat column
	if (event.repeatAtLogin())
		repeatOrder = 1;
	else
	{
		repeatInterval = event.recurInterval();
		switch (event.recurType())
		{
			case KAlarmEvent::MINUTELY:
				repeatOrder = 2;
				break;
			case KAlarmEvent::DAILY:
				repeatOrder = 3;
				break;
			case KAlarmEvent::WEEKLY:
				repeatOrder = 4;
				break;
			case KAlarmEvent::MONTHLY_DAY:
			case KAlarmEvent::MONTHLY_POS:
				repeatOrder = 5;
				break;
			case KAlarmEvent::ANNUAL_DATE:
			case KAlarmEvent::ANNUAL_POS:
			case KAlarmEvent::ANNUAL_DAY:
				repeatOrder = 6;
				break;
			case KAlarmEvent::NO_RECUR:
			default:
				break;
		}
	}
	setText(parent->repeatColumn(), repeatText);
	mRepeatOrder.sprintf("%c%08d", '0' + repeatOrder, repeatInterval);

	bool showColour = (event.action() == KAlarmEvent::MESSAGE || event.action() == KAlarmEvent::FILE);
	mColourOrder.sprintf("%06u", (showColour ? event.bgColour().rgb() : 0));
}

/******************************************************************************
*  Return the alarm time text in the form "date time".
*/
QString AlarmListViewItem::alarmText(const KAlarmEvent& event)
{
	static QString from    = QString::fromLatin1("From:");
	static QString to      = QString::fromLatin1("To:");
	static QString subject = QString::fromLatin1("Subject:");

	QString text = (event.action() == KAlarmEvent::EMAIL) ? event.emailSubject() : event.cleanText();
	int newline = text.find('\n');
	if (newline < 0)
		return text;       // it's a single-line text
	if (event.action() == KAlarmEvent::MESSAGE)
	{
		// If the message is the text of an email, return its subject line
		QStringList lines = QStringList::split('\n', text);
#warning Check the format of an email
		if (lines.count() >= 3
		&&  lines[0].startsWith(from)
		&&  lines[1].startsWith(to)
		&&  lines[2].startsWith(subject))
			return lines[2].mid(subject.length()).stripWhiteSpace();
	}
	return text.left(newline) + QString::fromLatin1("...");
}

/******************************************************************************
*  Return the alarm time text in the form "date time".
*/
QString AlarmListViewItem::alarmTimeText() const
{
	DateTime dateTime = mEvent.expired() ? mEvent.startDateTime() : mEvent.nextDateTime();
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
	if (mEvent.expired())
		return QString::null;
	DateTime dateTime = mEvent.nextDateTime();
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
	int days = mins / 24*60;
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
	if (mEvent.expired())
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
	int                  margin = listView->itemMargin();
	QRect box(margin, margin, width - margin*2, height() - margin*2);
	bool   selected = isSelected();
	QColor bgColour = selected ? cg.highlight() : cg.base();
	QColor fgColour = selected ? cg.highlightedText()
	                   : mEvent.expired() ? theApp()->preferences()->expiredColour() : cg.text();
	painter->setPen(fgColour);
	painter->fillRect(0, 0, width, height(), bgColour);

	if (column == listView->timeColumn())
		painter->drawText(box, AlignVCenter, text(column));
	else if (column == listView->timeToColumn())
		painter->drawText(box, AlignVCenter, text(column));
	else if (column == listView->repeatColumn())
		painter->drawText(box, AlignVCenter | AlignHCenter, text(column));
	else if (column == listView->colourColumn())
	{
		// Paint the cell the colour of the alarm message
		if (mEvent.action() == KAlarmEvent::MESSAGE || mEvent.action() == KAlarmEvent::FILE)
			painter->fillRect(box, mEvent.bgColour());
	}
	else if (column == listView->messageColumn())
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
		textRect.setLeft(box.left() + iconWidth + 3*frameWidth);
		if (!selected  &&  listView->drawMessageInColour())
		{
			painter->fillRect(box, mEvent.bgColour());
			painter->setBackgroundColor(mEvent.bgColour());
//			painter->setPen(mEvent.fgColour());
		}
		painter->drawPixmap(QPoint(iconRect.left() + frameWidth, iconRect.top()), *pixmap, pixmapRect);
		painter->drawText(textRect, AlignVCenter, text(column));
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
=  Class: AlarmListWhatsThis
=  Sets What's This? text depending on where in the list view is clicked.
=============================================================================*/

QString AlarmListWhatsThis::text(const QPoint& pt)
{
	QRect frame = listView->header()->frameGeometry();
	if (frame.contains(pt)
	||  listView->itemAt(QPoint(listView->itemMargin(), pt.y())) && frame.contains(QPoint(pt.x(), frame.y())))
	{
		int column = listView->header()->sectionAt(pt.x());
		if (column == listView->timeColumn())
			return i18n("Next scheduled date and time of the alarm");
		if (column == listView->timeToColumn())
			return i18n("How long until the next scheduled trigger of the alarm");
		if (column == listView->repeatColumn())
			return i18n("How often the alarm recurs");
		if (column == listView->colourColumn())
			return i18n("Background color of alarm message");
		if (column == listView->messageColumn())
			return i18n("Alarm message text, URL of text file to display, command to execute, or email subject line. The alarm type is indicated by the icon at the left.");
	}
	return i18n("List of scheduled alarms");
};

