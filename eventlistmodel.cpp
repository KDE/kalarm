/*
 *  eventlistmodel.cpp  -  model class for lists of alarms or templates
 *  Program:  kalarm
 *  Copyright © 2007-2012 by David Jarvie <djarvie@kde.org>
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

#include "resources/alarmresource.h"
#include "resources/alarmresources.h"
#include "alarmcalendar.h"
#include "alarmtime.h"
#include "preferences.h"
#include "synchtimer.h"
#include "eventlistmodel.moc"

#include <kalarmcal/alarmtext.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kdebug.h>

#include <QPixmap>

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
QPixmap* EventListModel::mAudioIcon = 0;
QSize    EventListModel::mIconSize;
int      EventListModel::mTimeHourPos = -2;


EventListModel* EventListModel::alarms()
{
    if (!mAlarmInstance)
    {
        mAlarmInstance = new EventListModel(CalEvent::ACTIVE | CalEvent::ARCHIVED);
        Preferences::connect(SIGNAL(archivedColourChanged(QColor)), mAlarmInstance, SLOT(slotUpdateArchivedColour(QColor)));
        Preferences::connect(SIGNAL(disabledColourChanged(QColor)), mAlarmInstance, SLOT(slotUpdateDisabledColour(QColor)));
        Preferences::connect(SIGNAL(holidaysChanged(KHolidays::HolidayRegion)), mAlarmInstance, SLOT(slotUpdateHolidays()));
        Preferences::connect(SIGNAL(workTimeChanged(QTime,QTime,QBitArray)), mAlarmInstance, SLOT(slotUpdateWorkingHours()));
    }
    return mAlarmInstance;
}

EventListModel* EventListModel::templates()
{
    if (!mTemplateInstance)
        mTemplateInstance = new EventListModel(CalEvent::TEMPLATE);
    return mTemplateInstance;
}

EventListModel::~EventListModel()
{
    if (this == mAlarmInstance)
        mAlarmInstance = 0;
    else if (this == mTemplateInstance)
        mTemplateInstance = 0;
}

EventListModel::EventListModel(CalEvent::Types status, QObject* parent)
    : QAbstractTableModel(parent),
      mStatus(status)
{
    // Load the current list of alarms.
    // The list will be updated whenever a signal is received notifying changes.
    // We need to store the list so that when deletions occur, the deleted alarm's
    // position in the list can be determined.
    mEvents = AlarmCalendar::resources()->events(mStatus);
    mHaveEvents = !mEvents.isEmpty();
//for(int x=0; x<mEvents.count(); ++x)kDebug(0)<<"Resource"<<(void*)mEvents[x]->resource()<<"Event"<<(void*)mEvents[x];
    if (!mTextIcon)
    {
        mTextIcon    = new QPixmap(SmallIcon("dialog-information"));
        mFileIcon    = new QPixmap(SmallIcon("document-open"));
        mCommandIcon = new QPixmap(SmallIcon("system-run"));
        mEmailIcon   = new QPixmap(SmallIcon("mail-message-unread"));
        mAudioIcon   = new QPixmap(SmallIcon("audio-x-generic"));
        mIconSize = mTextIcon->size().expandedTo(mFileIcon->size()).expandedTo(mCommandIcon->size()).expandedTo(mEmailIcon->size()).expandedTo(mAudioIcon->size());
    }
    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    AlarmResources* resources = AlarmResources::instance();
    connect(resources, SIGNAL(resourceStatusChanged(AlarmResource*,AlarmResources::Change)),
                       SLOT(slotResourceStatusChanged(AlarmResource*,AlarmResources::Change)));
    connect(resources, SIGNAL(resourceLoaded(AlarmResource*,bool)),
                       SLOT(slotResourceLoaded(AlarmResource*,bool)));
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
    KAEvent* event = static_cast<KAEvent*>(index.internalPointer());
    if (!event)
        return QVariant();
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
                    DateTime due = event->expired() ? event->startDateTime() : event->nextTrigger(KAEvent::DISPLAY_TRIGGER);
                    return AlarmTime::alarmTimeText(due);
                }
                case SortRole:
                {
                    DateTime due = event->expired() ? event->startDateTime() : event->nextTrigger(KAEvent::DISPLAY_TRIGGER);
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
                    if (event->expired())
                        return QString();
                    return AlarmTime::timeToAlarmText(event->nextTrigger(KAEvent::DISPLAY_TRIGGER));
                case SortRole:
                {
                    if (event->expired())
                        return -1;
                    KDateTime now = KDateTime::currentUtcDateTime();
                    DateTime due = event->nextTrigger(KAEvent::DISPLAY_TRIGGER);
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
                    switch (event->actionTypes())
                    {
                        case KAEvent::ACT_DISPLAY_COMMAND:
                        case KAEvent::ACT_DISPLAY:
                            return event->bgColour();
                        case KAEvent::ACT_COMMAND:
                            if (event->commandError() != KAEvent::CMD_NO_ERROR)
                                return Qt::red;
                            break;
                        default:
                            break;
                    }
                    break;
                case Qt::ForegroundRole:
                    if (event->commandError() != KAEvent::CMD_NO_ERROR)
                    {
                        if (event->actionTypes() == KAEvent::ACT_COMMAND)
                            return Qt::white;
                        QColor colour = Qt::red;
                        int r, g, b;
                        event->bgColour().getRgb(&r, &g, &b);
                        if (r > 128  &&  g <= 128  &&  b <= 128)
                            colour = Qt::white;
                        return colour;
                    }
                    break;
                case Qt::DisplayRole:
                    if (event->commandError() != KAEvent::CMD_NO_ERROR)
                        return QString::fromLatin1("!");
                    break;
                case SortRole:
                {
                    unsigned i = (event->actionTypes() == KAEvent::ACT_DISPLAY)
                               ? event->bgColour().rgb() : 0;
                    return QString("%1").arg(i, 6, 10, QLatin1Char('0'));
                }
                default:
                    break;
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
                    return static_cast<int>(event->actionSubType());
                case SortRole:
                    return QString("%1").arg(event->actionSubType(), 2, 10, QLatin1Char('0'));
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
                    return AlarmText::summary(*event, 1);
                case Qt::ToolTipRole:
                    return AlarmText::summary(*event, 10);
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
                    return event->templateName();
                case SortRole:
                    return event->templateName().toUpper();
            }
            break;
        default:
            break;
    }

    switch (role)
    {
        case Qt::ForegroundRole:
            if (!event->enabled())
                   return Preferences::disabledColour();
            if (event->expired())
                   return Preferences::archivedColour();
            break;   // use the default for normal active alarms
        case Qt::ToolTipRole:
            // Show the last command execution error message
            switch (event->commandError())
            {
                case KAEvent::CMD_ERROR:
                    return i18nc("@info:tooltip", "Command execution failed");
                case KAEvent::CMD_ERROR_PRE:
                    return i18nc("@info:tooltip", "Pre-alarm action execution failed");
                case KAEvent::CMD_ERROR_POST:
                    return i18nc("@info:tooltip", "Post-alarm action execution failed");
                case KAEvent::CMD_ERROR_PRE_POST:
                    return i18nc("@info:tooltip", "Pre- and post-alarm action execution failed");
                default:
                case KAEvent::CMD_NO_ERROR:
                    break;
            }
            break;
        case StatusRole:
            return event->category();
        case EnabledRole:
            return event->enabled();
        default:
            break;
    }

    if (resourceColour)
    {
        AlarmResource* resource = AlarmResources::instance()->resourceForIncidence(event->id());
        if (resource  &&  resource->colour().isValid())
            return resource->colour();
    }
    return QVariant();
}

bool EventListModel::setData(const QModelIndex& ix, const QVariant&, int role)
{
    if (ix.isValid()  &&  role == Qt::EditRole)
    {
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
    kDebug();
    int firstRow = -1;
    for (int row = 0, end = mEvents.count();  row < end;  ++row)
    {
        if (mEvents[row]->category() == CalEvent::ARCHIVED)
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
    kDebug();
    int firstRow = -1;
    for (int row = 0, end = mEvents.count();  row < end;  ++row)
    {
        if (!mEvents[row]->enabled())
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
* Called when the definition of holidays has changed.
* Update the next trigger time for all alarms which are set to recur only on
* non-holidays.
*/
void EventListModel::slotUpdateHolidays()
{
    kDebug();
    int firstRow = -1;
    for (int row = 0, end = mEvents.count();  row < end;  ++row)
    {
        if (mEvents[row]->holidaysExcluded())
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
* Called when the definition of working hours has changed.
* Update the next trigger time for all alarms which are set to recur only
* during working hours.
*/
void EventListModel::slotUpdateWorkingHours()
{
    kDebug();
    int firstRow = -1;
    for (int row = 0, end = mEvents.count();  row < end;  ++row)
    {
        if (mEvents[row]->workTimeOnly())
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
* Called when the command error status of an alarm has changed.
* Update the visual command error indication.
*/
void EventListModel::updateCommandError(const QString& eventId)
{
    int row = findEvent(eventId);
    if (row >= 0)
    {
        QModelIndex ix = index(row, ColourColumn);
        emit dataChanged(ix, ix);
    }
}

/******************************************************************************
* Called when loading of a resource is complete.
*/
void EventListModel::slotResourceLoaded(AlarmResource* resource, bool active)
{
    if (active)
        slotResourceStatusChanged(resource, AlarmResources::Added);
}

/******************************************************************************
* Static method called when a resource status has changed.
*/
void EventListModel::resourceStatusChanged(AlarmResource* resource, AlarmResources::Change change)
{
    if (mAlarmInstance)
        mAlarmInstance->slotResourceStatusChanged(resource, change);
    if (mTemplateInstance)
        mTemplateInstance->slotResourceStatusChanged(resource, change);
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
            kDebug() << resource->resourceName() << "Added";
            added = true;
            break;
        case AlarmResources::Deleted:
            kDebug() << resource->resourceName() << "Deleted";
            removeResource(resource);
            return;
        case AlarmResources::Invalidated:
            kDebug() << resource->resourceName() << "Invalidated";
            removeResource(resource);
            return;
        case AlarmResources::Location:
            kDebug() << resource->resourceName() << "Location";
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
            kDebug() << "Colour";
            int firstRow = -1;
            for (int row = 0, end = mEvents.count();  row < end;  ++row)
            {
                if (mEvents[row]->resource() == resource)
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
        case AlarmResources::WrongType:
            return;
    }

    if (added)
    {
        KAEvent::List list = AlarmCalendar::resources()->events(resource, mStatus);
        for (int i = list.count();  --i >= 0;  )
        {
            if (mEvents.indexOf(list[i]) >= 0)
                list.remove(i);    // avoid creating duplicate entries
        }
        if (!list.isEmpty())
        {
            int row = mEvents.count();
            beginInsertRows(QModelIndex(), row, row + list.count() - 1);
            mEvents += list;
            endInsertRows();
            if (!mHaveEvents)
                updateHaveEvents(true);
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
    kDebug();
    int lastRow = -1;
    for (int row = mEvents.count();  --row >= 0; )
    {
        AlarmResource* r = mEvents[row]->resource();
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
                mEvents.remove(lastRow--);
            endRemoveRows();
            lastRow = -1;
        }
    }
    if (lastRow >= 0)
    {
        beginRemoveRows(QModelIndex(), 0, lastRow);
        while (lastRow >= 0)
            mEvents.remove(lastRow--);
        endRemoveRows();
    }
    if (mHaveEvents  &&  mEvents.isEmpty())
        updateHaveEvents(false);
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
    KAEvent::List list = AlarmCalendar::resources()->events(mStatus);
    if (!list.isEmpty())
    {
        beginInsertRows(QModelIndex(), 0, list.count() - 1);
        mEvents = list;
        endInsertRows();
    }
    if (mHaveEvents == mEvents.isEmpty())
        updateHaveEvents(!mHaveEvents);
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListModel::eventIndex(const QString& eventId) const
{
    for (int row = 0, end = mEvents.count();  row < end;  ++row)
    {
        if (mEvents[row]->id() == eventId)
            return createIndex(row, 0, mEvents[row]);
    }
    return QModelIndex();
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListModel::eventIndex(const KAEvent* event) const
{
    int row = mEvents.indexOf(const_cast<KAEvent*>(event));
    if (row < 0)
        return QModelIndex();
    return createIndex(row, 0, mEvents[row]);
}

/******************************************************************************
* Add an event to the list.
*/
void EventListModel::addEvent(KAEvent* event)
{
    if (!(event->category() & mStatus)  ||  mEvents.contains(event))
        return;
    int row = mEvents.count();
    beginInsertRows(QModelIndex(), row, row);
    mEvents += event;
    endInsertRows();
    if (!mHaveEvents)
        updateHaveEvents(true);
}

/******************************************************************************
* Add an event to the list.
*/
void EventListModel::addEvents(const KAEvent::List& events)
{
    KAEvent::List evs;
    for (int i = 0, count = events.count();  i < count;  ++i)
        if (events[i]->category() & mStatus)
            evs += events[i];
    int row = mEvents.count();
    beginInsertRows(QModelIndex(), row, row + evs.count() - 1);
    mEvents += evs;
    endInsertRows();
    if (!mHaveEvents)
        updateHaveEvents(true);
}

/******************************************************************************
* Remove an event from the list.
*/
void EventListModel::removeEvent(int row)
{
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    mEvents.remove(row);
    endRemoveRows();
    if (mHaveEvents  &&  mEvents.isEmpty())
        updateHaveEvents(false);
}

/******************************************************************************
* Notify that an event in the list has been updated.
*/
bool EventListModel::updateEvent(int row)
{
    if (row < 0)
        return false;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    return true;
}

bool EventListModel::updateEvent(const QString& oldId, KAEvent* newEvent)
{
    int row = findEvent(oldId);
    if (row < 0)
        return false;
    mEvents[row] = newEvent;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    return true;
}

/******************************************************************************
* Find the row of an event in the list, given its unique ID.
*/
int EventListModel::findEvent(const QString& eventId) const
{
    for (int row = 0, end = mEvents.count();  row < end;  ++row)
    {
        if (mEvents[row]->id() == eventId)
            return row;
    }
    return -1;
}

/******************************************************************************
* Return the event for a given row.
*/
KAEvent* EventListModel::event(int row) const
{
    if (row < 0  ||  row >= mEvents.count())
        return 0;
    return mEvents[row];
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent* EventListModel::event(const QModelIndex& index)
{
    if (!index.isValid())
        return 0;
    return static_cast<KAEvent*>(index.internalPointer());
}

/******************************************************************************
* Return the repetition text.
*/
QString EventListModel::repeatText(const KAEvent* event) const
{
    QString repeatText = event->recurrenceText(true);
    if (repeatText.isEmpty())
        repeatText = event->repetitionText(true);
    return repeatText;
}

/******************************************************************************
* Return a string for sorting the repetition column.
*/
QString EventListModel::repeatOrder(const KAEvent* event) const
{
    int repeatOrder = 0;
    int repeatInterval = 0;
    if (event->repeatAtLogin())
        repeatOrder = 1;
    else
    {
        repeatInterval = event->recurInterval();
        switch (event->recurType())
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
* Return the icon associated with the event's action.
*/
QPixmap* EventListModel::eventIcon(const KAEvent* event) const
{
    switch (event->actionTypes())
    {
        case KAEvent::ACT_EMAIL:
            return mEmailIcon;
        case KAEvent::ACT_AUDIO:
            return mAudioIcon;
        case KAEvent::ACT_COMMAND:
            return mCommandIcon;
        case KAEvent::ACT_DISPLAY:
            if (event->actionSubType() == KAEvent::FILE)
                return mFileIcon;
            // fall through to ACT_DISPLAY_COMMAND
        case KAEvent::ACT_DISPLAY_COMMAND:
        default:
            return mTextIcon;
    }
}

/******************************************************************************
* Returns the QWhatsThis text for a specified column.
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
            return QString();
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
    setDynamicSortFilter(true);
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent* EventListFilterModel::event(const QModelIndex& index) const
{
    return static_cast<EventListModel*>(sourceModel())->event(mapToSource(index));
}

KAEvent* EventListFilterModel::event(int row) const
{
    return static_cast<EventListModel*>(sourceModel())->event(mapToSource(index(row, 0)));
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListFilterModel::eventIndex(const QString& eventId) const
{
    return mapFromSource(static_cast<EventListModel*>(sourceModel())->eventIndex(eventId));
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListFilterModel::eventIndex(const KAEvent* event) const
{
    return mapFromSource(static_cast<EventListModel*>(sourceModel())->eventIndex(event));
}

void EventListFilterModel::slotDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight));
}

// vim: et sw=4:
