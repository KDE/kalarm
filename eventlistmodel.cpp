/*
 *  eventlistmodel.cpp  -  model class for lists of alarms or templates
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QApplication>
#include <QPixmap>

#include <klocale.h>
#include <kiconloader.h>
#include <kdebug.h>

#include "resources/alarmresource.h"
#include "resources/alarmresources.h"
#include "resources/kcalendar.h"
#include "alarmcalendar.h"
#include "alarmtext.h"
#include "preferences.h"
#include "synchtimer.h"
#include "eventlistmodel.moc"


/*=============================================================================
= Class: EventListModel
= Contains all active/archived alarms, or all alarm templates, unsorted.
=============================================================================*/

EventListModel* EventListModel::mAlarmInstance = 0;
EventListModel* EventListModel::mTemplateInstance = 0;
QPixmap* EventListModel::mTextIcon = 0;
QPixmap* EventListModel::mFileIcon = 0;
QPixmap* EventListModel::mCommandIcon = 0;
QPixmap* EventListModel::mEmailIcon = 0;
QSize    EventListModel::mIconSize;
int      EventListModel::mTimeHourPos = -2;


EventListModel* EventListModel::alarms()
{
	if (!mAlarmInstance)
	{
		mAlarmInstance = new EventListModel(static_cast<KCalEvent::Status>(KCalEvent::ACTIVE | KCalEvent::ARCHIVED));
		Preferences::connect(SIGNAL(archivedColourChanged(const QColor&)), mAlarmInstance, SLOT(slotUpdateArchivedColour(const QColor&)));
		Preferences::connect(SIGNAL(disabledColourChanged(const QColor&)), mAlarmInstance, SLOT(slotUpdateDisabledColour(const QColor&)));
		Preferences::connect(SIGNAL(workTimeChanged(const QTime&, const QTime&, const QBitArray&)), mAlarmInstance, SLOT(slotUpdateWorkingHours()));
	}
	return mAlarmInstance;
}

EventListModel* EventListModel::templates()
{
	if (!mTemplateInstance)
		mTemplateInstance = new EventListModel(KCalEvent::TEMPLATE);
	return mTemplateInstance;
}

EventListModel::EventListModel(KCalEvent::Status status, QObject* parent)
	: QAbstractTableModel(parent),
	  mStatus(status)
{
	// Load the current list of alarms.
	// The list will be updated whenever a signal is received notifying changes.
	// We need to store the list so that when deletions occur, the deleted alarm's
	// position in the list can be determined.
	mEvents = AlarmCalendar::resources()->events(mStatus);
for(int x=0; x<mEvents.count(); ++x)kDebug()<<"Event"<<(void*)mEvents[x];
	if (!mTextIcon)
	{
		mTextIcon    = new QPixmap(SmallIcon("text"));
		mFileIcon    = new QPixmap(SmallIcon("document-open"));
		mCommandIcon = new QPixmap(SmallIcon("exec"));
		mEmailIcon   = new QPixmap(SmallIcon("mail"));
		mIconSize = mTextIcon->size().expandedTo(mFileIcon->size()).expandedTo(mCommandIcon->size()).expandedTo(mEmailIcon->size());
	}
	MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
#ifdef __GNUC__
#warning Need to update when a resource is reloaded, to prevent crash
#endif
	connect(AlarmResources::instance(), SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotResourceStatusChanged(AlarmResource*, AlarmResources::Change)));
}

int EventListModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;
	return mEvents.count();
}

int EventListModel::columnCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;
	return ColumnCount;
}

QModelIndex EventListModel::index(int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid()  ||  row >= mEvents.count())
		return QModelIndex();
	return createIndex(row, column, mEvents[row]);
}

QVariant EventListModel::data(const QModelIndex& index, int role) const
{
	int column = index.column();
	if (role == Qt::WhatsThisRole)
		return whatsThisText(column);
	KCal::Event* kcalEvent = static_cast<KCal::Event*>(index.internalPointer());
	if (!kcalEvent)
		return QVariant();
	KAEvent event(static_cast<KCal::Event*>(index.internalPointer()));
	switch (role)
	{
		case Qt::ForegroundRole:
			if (!event.enabled())
			       return Preferences::disabledColour();
			if (event.expired())
			       return Preferences::archivedColour();
			break;   // use the default for normal active alarms
		case StatusRole:
			return event.category();
		default:
			break;
	}
	bool resourceColour = false;
	switch (column)
	{
		case TimeColumn:
			switch (role)
			{
				case Qt::BackgroundRole:
					resourceColour = true;
					break;
				case Qt::DisplayRole:
				{
					DateTime due = event.expired() ? event.startDateTime() : event.displayDateTime();
					return alarmTimeText(due);
				}
				case SortRole:
				{
					DateTime due = event.expired() ? event.startDateTime() : event.displayDateTime();
					return due.isValid() ? due.effectiveKDateTime().toUtc().dateTime()
					                     : QDateTime(QDate(9999,12,31), QTime(0,0,0));
				}
				default:
					break;
			}
			break;
		case TimeToColumn:
			switch (role)
			{
				case Qt::BackgroundRole:
					resourceColour = true;
					break;
				case Qt::DisplayRole:
					if (event.expired())
						return QString();
					return timeToAlarmText(event.displayDateTime());
				case SortRole:
				{
					if (event.expired())
						return -1;
					KDateTime now = KDateTime::currentUtcDateTime();
					DateTime due = event.displayDateTime();
					if (due.isDateOnly())
						return now.date().daysTo(due.date()) * 1440;
					return (now.secsTo(due.effectiveKDateTime()) + 59) / 60;
				}
			}
			break;
		case RepeatColumn:
			switch (role)
			{
				case Qt::BackgroundRole:
					resourceColour = true;
					break;
				case Qt::DisplayRole:
					return repeatText(event);
				case Qt::TextAlignmentRole:
					return Qt::AlignHCenter;
				case SortRole:
					return repeatOrder(event);
			}
			break;
		case ColourColumn:
			switch (role)
			{
				case Qt::BackgroundRole:
					if (event.action() == KAEvent::MESSAGE
					||  event.action() == KAEvent::FILE)
						return event.bgColour();
				case SortRole:
				{
					unsigned i = (event.action() == KAEvent::MESSAGE || event.action() == KAEvent::FILE)
				           	? event.bgColour().rgb() : 0;
					return QString("%1").arg(i, 6, 10, QLatin1Char('0'));
				}
			}
			break;
		case TypeColumn:
			switch (role)
			{
				case Qt::DecorationRole:
				{
					QVariant v;
					v.setValue(*eventIcon(event));
					return v;
				}
				case Qt::TextAlignmentRole:
					return Qt::AlignHCenter;
				case Qt::SizeHintRole:
					return mIconSize;
				case Qt::AccessibleTextRole:
#ifdef __GNUC__
#warning Implement this
#endif
					return QString();
				case ValueRole:
					return static_cast<int>(event.action());
				case SortRole:
					return QString("%1").arg(event.action(), 2, 10, QLatin1Char('0'));
			}
			break;
		case TextColumn:
			switch (role)
			{
				case Qt::BackgroundRole:
					resourceColour = true;
					break;
				case Qt::DisplayRole:
				case SortRole:
					return AlarmText::summary(event, 1);
				case Qt::ToolTipRole:
					return AlarmText::summary(event);
				default:
					break;
			}
			break;
		case TemplateNameColumn:
			switch (role)
			{
				case Qt::BackgroundRole:
					resourceColour = true;
					break;
				case Qt::DisplayRole:
					return event.templateName();
				case SortRole:
					return event.templateName().toUpper();
			}
			break;
		default:
			break;
	}

	if (resourceColour)
	{
		AlarmResource* resource = AlarmResources::instance()->resourceForIncidence(event.id());
		if (resource  &&  resource->colour().isValid())
			return resource->colour();
	}
	return QVariant();
}

bool EventListModel::setData(const QModelIndex& ix, const QVariant&, int role)
{
	if (ix.isValid()  &&  role == Qt::EditRole)
	{
//??? update event
		int row = ix.row();
		emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
		return true;
	}
	return false;
}

QVariant EventListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (section)
			{
				case TimeColumn:
					return i18nc("@title:column", "Time");
				case TimeToColumn:
					return i18nc("@title:column", "Time To");
				case RepeatColumn:
					return i18nc("@title:column", "Repeat");
				case ColourColumn:
					return QString();
				case TypeColumn:
					return QString();
				case TextColumn:
					return i18nc("@title:column", "Message, File or Command");
				case TemplateNameColumn:
					return i18nc("@title:column Template name", "Name");
			}
		}
		else if (role == Qt::WhatsThisRole)
			return whatsThisText(section);
	}
	return QVariant();
}

Qt::ItemFlags EventListModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::ItemIsEnabled;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

/******************************************************************************
* Signal every minute that the time-to-alarm values have changed.
*/
void EventListModel::slotUpdateTimeTo()
{
	int n = mEvents.count();
	if (n > 0)
		emit dataChanged(index(0, TimeToColumn), index(n - 1, TimeToColumn));
}

/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
void EventListModel::slotUpdateArchivedColour(const QColor&)
{
	kDebug(5950) << "EventListModel::slotUpdateArchivedColour()";
	int firstRow = -1;
	for (int row = 0, end = mEvents.count();  row < end;  ++row)
	{
		if (KCalEvent::status(mEvents[row]) == KCalEvent::ARCHIVED)
		{
			// For efficiency, emit a single signal for each group
			// of consecutive archived alarms, rather than a separate
			// signal for each alarm.
			if (firstRow < 0)
				firstRow = row;
		}
		else if (firstRow >= 0)
		{
			emit dataChanged(index(firstRow, 0), index(row - 1, ColumnCount - 1));
			firstRow = -1;
		}
	}
	if (firstRow >= 0)
		emit dataChanged(index(firstRow, 0), index(mEvents.count() - 1, ColumnCount - 1));
}

/******************************************************************************
* Called when the colour used to display disabled alarms has changed.
*/
void EventListModel::slotUpdateDisabledColour(const QColor&)
{
	kDebug(5950) << "EventListModel::slotUpdateDisabledColour()";
	int firstRow = -1;
	for (int row = 0, end = mEvents.count();  row < end;  ++row)
	{
		if (mEvents[row]->statusStr() == QLatin1String("DISABLED"))
		{
			// For efficiency, emit a single signal for each group
			// of consecutive disabled alarms, rather than a separate
			// signal for each alarm.
			if (firstRow < 0)
				firstRow = row;
		}
		else if (firstRow >= 0)
		{
			emit dataChanged(index(firstRow, 0), index(row - 1, ColumnCount - 1));
			firstRow = -1;
		}
	}
	if (firstRow >= 0)
		emit dataChanged(index(firstRow, 0), index(mEvents.count() - 1, ColumnCount - 1));
}

/******************************************************************************
* Called when the definition of working hours has changed.
* Update the next trigger time for all alarms which are set to recur only
* during working hours.
*/
void EventListModel::slotUpdateWorkingHours()
{
	kDebug(5950) << "EventListModel::slotUpdateWorkingHours()";
	int firstRow = -1;
	for (int row = 0, end = mEvents.count();  row < end;  ++row)
	{
		if (KAEvent(mEvents[row]).workTimeOnly())
		{
			// For efficiency, emit a single signal for each group
			// of consecutive alarms to update, rather than a separate
			// signal for each alarm.
			if (firstRow < 0)
				firstRow = row;
		}
		else if (firstRow >= 0)
		{
			emit dataChanged(index(firstRow, TimeColumn), index(row - 1, TimeColumn));
			emit dataChanged(index(firstRow, TimeToColumn), index(row - 1, TimeToColumn));
			firstRow = -1;
		}
	}
	if (firstRow >= 0)
	{
		emit dataChanged(index(firstRow, TimeColumn), index(mEvents.count() - 1, TimeColumn));
		emit dataChanged(index(firstRow, TimeToColumn), index(mEvents.count() - 1, TimeToColumn));
	}
}

/******************************************************************************
* Called when a resource status has changed.
*/
void EventListModel::slotResourceStatusChanged(AlarmResource* resource, AlarmResources::Change change)
{
	bool added = false;
	switch (change)
	{
		case AlarmResources::Added:
			kDebug(5950) << "EventListModel::slotResourceStatusChanged(Added)";
			added = true;
			break;
		case AlarmResources::Deleted:
			kDebug(5950) << "EventListModel::slotResourceStatusChanged(Deleted)";
			removeResource(resource);
			return;
		case AlarmResources::Invalidated:
			kDebug(5950) << "EventListModel::slotResourceStatusChanged(Invalidated)";
			removeResource(resource);
			return;
		case AlarmResources::Location:
			kDebug(5950) << "EventListModel::slotResourceStatusChanged(Location)";
			removeResource(resource);
			added = true;
			break;
		case AlarmResources::Enabled:
			if (resource->isActive())
				added = true;
			else
				removeResource(resource);
			break;
		case AlarmResources::Colour:
		{
			kDebug(5950) << "EventListModel::slotResourceStatusChanged(Colour)";
			AlarmResources* resources = AlarmResources::instance();
			int firstRow = -1;
			for (int row = 0, end = mEvents.count();  row < end;  ++row)
			{
				if (resources->resource(mEvents[row]) == resource)
				{
					// For efficiency, emit a single signal for each group
					// of consecutive alarms for the resource, rather than a separate
					// signal for each alarm.
					if (firstRow < 0)
						firstRow = row;
				}
				else if (firstRow >= 0)
				{
					emit dataChanged(index(firstRow, 0), index(row - 1, ColumnCount - 1));
					firstRow = -1;
				}
			}
			if (firstRow >= 0)
				emit dataChanged(index(firstRow, 0), index(mEvents.count() - 1, ColumnCount - 1));
			return;
		}
		case AlarmResources::ReadOnly:
			return;
	}

	if (added)
	{
		KCal::Event::List list = AlarmCalendar::resources()->events(resource, mStatus);
		if (!list.isEmpty())
		{
			int row = mEvents.count();
			beginInsertRows(QModelIndex(), row, row + list.count() - 1);
			mEvents += list;
			endInsertRows();
		}
	}
}

/******************************************************************************
* Remove a resource's events from the list.
* This has to be called before the resource is actually deleted or reloaded. If
* not, timer based updates can occur between the resource being deleted and
* slotResourceStatusChanged(Deleted) being triggered, leading to crashes when
* data from the resource's events is fetched.
*/
void EventListModel::removeResource(AlarmResource* resource)
{
	kDebug(5950) << "EventListModel::removeResource()";
	AlarmResources* resources = AlarmResources::instance();
	int lastRow = -1;
	for (int row = mEvents.count();  --row >= 0; )
	{
		AlarmResource* r = resources->resource(mEvents[row]);
		if (!r  ||  r == resource)
		{
			// For efficiency, delete each group of consecutive
			// alarms for the resource, rather than deleting each
			// alarm separately.
			if (lastRow < 0)
				lastRow = row;
		}
		else if (lastRow >= 0)
		{
			beginRemoveRows(QModelIndex(), row + 1, lastRow);
			while (lastRow > row)
				mEvents.removeAt(lastRow--);
			endRemoveRows();
			lastRow = -1;
		}
	}
	if (lastRow >= 0)
	{
		beginRemoveRows(QModelIndex(), 0, lastRow);
		while (lastRow >= 0)
			mEvents.removeAt(lastRow--);
		endRemoveRows();
	}
}

/******************************************************************************
* Reload the event list.
*/
void EventListModel::reload()
{
	// This would be better done by a reset(), but the signals are private to QAbstractItemModel
	if (!mEvents.isEmpty())
	{
		beginRemoveRows(QModelIndex(), 0, mEvents.count() - 1);
		mEvents.clear();
		endRemoveRows();
	}
	KCal::Event::List list = AlarmCalendar::resources()->events(mStatus);
	if (!list.isEmpty())
	{
		beginInsertRows(QModelIndex(), 0, list.count() - 1);
		mEvents = list;
		endInsertRows();
	}
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListModel::eventIndex(const QString& eventId) const
{
	for (int row = 0, end = mEvents.count();  row < end;  ++row)
	{
		if (mEvents[row]->uid() == eventId)
			return createIndex(row, 0, mEvents[row]);
	}
	return QModelIndex();
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListModel::eventIndex(const KCal::Event* event) const
{
	int row = mEvents.indexOf(const_cast<KCal::Event*>(event));
	if (row < 0)
		return QModelIndex();
	return createIndex(row, 0, mEvents[row]);
}

/******************************************************************************
* Add an event to the list.
*/
void EventListModel::addEvent(KCal::Event* event)
{
	if (!(KAEvent(event).category() & mStatus))
		return;
	int row = mEvents.count();
	beginInsertRows(QModelIndex(), row, row);
	mEvents += event;
	endInsertRows();
}

/******************************************************************************
* Add an event to the list.
*/
void EventListModel::addEvents(const KCal::Event::List& events)
{
	KCal::Event::List evs;
	for (int i = 0, count = events.count();  i < count;  ++i)
		if (KAEvent(events[i]).category() & mStatus)
			evs += events[i];
	int row = mEvents.count();
	beginInsertRows(QModelIndex(), row, row + evs.count() - 1);
	mEvents += evs;
	endInsertRows();
}

/******************************************************************************
* Remove an event from the list.
*/
void EventListModel::removeEvent(int row)
{
	if (row < 0)
		return;
	beginRemoveRows(QModelIndex(), row, row);
	mEvents.removeAt(row);
	endRemoveRows();
}

/******************************************************************************
* Notify that an event in the list has been updated.
*/
void EventListModel::updateEvent(int row)
{
	if (row < 0)
		return;
	emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

void EventListModel::updateEvent(KCal::Event* oldEvent, KCal::Event* newEvent)
{
	int row = mEvents.indexOf(oldEvent);
	if (row < 0)
		return;
	mEvents[row] = newEvent;
	emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

/******************************************************************************
* Find the row of an event in the list, given its unique ID.
*/
int EventListModel::findEvent(const QString& eventId) const
{
	for (int row = 0, end = mEvents.count();  row < end;  ++row)
	{
		if (mEvents[row]->uid() == eventId)
			return row;
	}
	return -1;
}

/******************************************************************************
* Return the event referred to by an index.
*/
KCal::Event* EventListModel::event(const QModelIndex& index)
{
	if (!index.isValid())
		return 0;
	return static_cast<KCal::Event*>(index.internalPointer());
}

/******************************************************************************
* Return the alarm time text in the form "date time".
*/
QString EventListModel::alarmTimeText(const DateTime& dateTime) const
{
	if (!dateTime.isValid())
		return i18nc("@info/plain Alarm never occurs", "Never");
	KLocale* locale = KGlobal::locale();
	KDateTime kdt = dateTime.effectiveKDateTime().toTimeSpec(Preferences::timeZone());
	QString dateTimeText = locale->formatDate(kdt.date(), KLocale::ShortDate);
	if (!dateTime.isDateOnly()  ||  kdt.utcOffset() != dateTime.utcOffset())
	{
		// Display the time of day if it's a date/time value, or if it's
		// a date-only value but it's in a different time zone
		dateTimeText += QLatin1Char(' ');
		QString time = locale->formatTime(kdt.time());
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
* Return the time-to-alarm text.
*/
QString EventListModel::timeToAlarmText(const DateTime& dateTime) const
{
	if (!dateTime.isValid())
		return i18nc("@info/plain Alarm never occurs", "Never");
	KDateTime now = KDateTime::currentUtcDateTime();
	if (dateTime.isDateOnly())
	{
		int days = now.date().daysTo(dateTime.date());
		// xgettext: no-c-format
		return i18nc("@info/plain n days", "%1d", days);
	}
	int mins = (now.secsTo(dateTime.effectiveKDateTime()) + 59) / 60;
	if (mins < 0)
		return QString();
	char minutes[3] = "00";
	minutes[0] = (mins%60) / 10 + '0';
	minutes[1] = (mins%60) % 10 + '0';
	if (mins < 24*60)
		return i18nc("@info/plain hours:minutes", "%1:%2", mins/60, minutes);
	int days = mins / (24*60);
	mins = mins % (24*60);
	return i18nc("@info/plain days hours:minutes", "%1d %2:%3", days, mins/60, minutes);
}

/******************************************************************************
* Return the repetition text.
*/
QString EventListModel::repeatText(const KAEvent& event) const
{
	QString repeatText = event.recurrenceText(true);
	if (repeatText.isEmpty())
		repeatText = event.repetitionText(true);
	return repeatText;
}

/******************************************************************************
* Return a string for sorting the repetition column.
*/
QString EventListModel::repeatOrder(const KAEvent& event) const
{
	int repeatOrder = 0;
	int repeatInterval = 0;
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
	return QString("%1%2").arg(static_cast<char>('0' + repeatOrder)).arg(repeatInterval, 8, 10, QLatin1Char('0'));
}

/******************************************************************************
*  Return the icon associated with the event's action.
*/
QPixmap* EventListModel::eventIcon(const KAEvent& event) const
{
	switch (event.action())
	{
		case KAAlarm::FILE:     return mFileIcon;
		case KAAlarm::COMMAND:  return mCommandIcon;
		case KAAlarm::EMAIL:    return mEmailIcon;
		case KAAlarm::MESSAGE:
		default:                return mTextIcon;
	}
}

/******************************************************************************
*  Returns the QWhatsThis text for a specified column.
*/
QString EventListModel::whatsThisText(int column) const
{
	switch (column)
	{
		case TimeColumn:
			return i18nc("@info:whatsthis", "Next scheduled date and time of the alarm");
		case TimeToColumn:
			return i18nc("@info:whatsthis", "How long until the next scheduled trigger of the alarm");
		case RepeatColumn:
			return i18nc("@info:whatsthis", "How often the alarm recurs");
		case ColourColumn:
			return i18nc("@info:whatsthis", "Background color of alarm message");
		case TypeColumn:
			return i18nc("@info:whatsthis", "Alarm type (message, file, command or email)");
		case TextColumn:
			return i18nc("@info:whatsthis", "Alarm message text, URL of text file to display, command to execute, or email subject line");
		case TemplateNameColumn:
			return i18nc("@info:whatsthis", "Name of the alarm template");
		default:
			return i18nc("@info:whatsthis", "List of scheduled alarms");
	}
}


/*=============================================================================
= Class: EventListFilterModel
= Base class for all filters on EventListModel.
=============================================================================*/

EventListFilterModel::EventListFilterModel(EventListModel* baseModel, QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setSourceModel(baseModel);
	setSortRole(EventListModel::SortRole);
}

/******************************************************************************
* Return the event referred to by an index.
*/
KCal::Event* EventListFilterModel::event(const QModelIndex& index) const
{
	return static_cast<EventListModel*>(sourceModel())->event(mapToSource(index));
}

KCal::Event* EventListFilterModel::event(int row) const
{
	return static_cast<EventListModel*>(sourceModel())->event(mapToSource(index(row, 0)));
}

void EventListFilterModel::slotDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
	emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight));
}
