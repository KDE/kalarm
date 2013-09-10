/*
 *  akonadimodel.cpp  -  KAlarm calendar file access using Akonadi
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

#include "akonadimodel.h"
#include "alarmtime.h"
#include "autoqpointer.h"
#include "calendarmigrator.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"
#include "synchtimer.h"
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"

#include <kalarmcal/alarmtext.h>
#include <kalarmcal/collectionattribute.h>
#include <kalarmcal/compatibilityattribute.h>
#include <kalarmcal/eventattribute.h>

#include <akonadi/agentfilterproxymodel.h>
#include <akonadi/agentinstancecreatejob.h>
#include <akonadi/agentmanager.h>
#include <akonadi/agenttype.h>
#include <akonadi/agenttypedialog.h>
#include <akonadi/attributefactory.h>
#include <akonadi/changerecorder.h>
#include <akonadi/collectiondeletejob.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/item.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemmodifyjob.h>
#include <akonadi/itemdeletejob.h>
#include <akonadi/itemfetchscope.h>

#include <klocale.h>
#include <kcolorutils.h>

#include <QApplication>
#include <QFileInfo>
#include <QTimer>

using namespace Akonadi;
using namespace KAlarmCal;

static Collection::Rights writableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;

//static bool checkItem_true(const Item&) { return true; }

/*=============================================================================
= Class: AkonadiModel
=============================================================================*/

AkonadiModel* AkonadiModel::mInstance = 0;
QPixmap*      AkonadiModel::mTextIcon = 0;
QPixmap*      AkonadiModel::mFileIcon = 0;
QPixmap*      AkonadiModel::mCommandIcon = 0;
QPixmap*      AkonadiModel::mEmailIcon = 0;
QPixmap*      AkonadiModel::mAudioIcon = 0;
QSize         AkonadiModel::mIconSize;
int           AkonadiModel::mTimeHourPos = -2;

/******************************************************************************
* Construct and return the singleton.
*/
AkonadiModel* AkonadiModel::instance()
{
    if (!mInstance)
        mInstance = new AkonadiModel(new ChangeRecorder(qApp), qApp);
    return mInstance;
}

/******************************************************************************
* Constructor.
*/
AkonadiModel::AkonadiModel(ChangeRecorder* monitor, QObject* parent)
    : EntityTreeModel(monitor, parent),
      mMonitor(monitor),
      mResourcesChecked(false)
{
    // Set lazy population to enable the contents of unselected collections to be ignored
    setItemPopulationStrategy(LazyPopulation);

    // Restrict monitoring to collections containing the KAlarm mime types
    monitor->setCollectionMonitored(Collection::root());
    monitor->setResourceMonitored("akonadi_kalarm_resource");
    monitor->setResourceMonitored("akonadi_kalarm_dir_resource");
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_ACTIVE);
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_ARCHIVED);
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_TEMPLATE);
    monitor->itemFetchScope().fetchFullPayload();
    monitor->itemFetchScope().fetchAttribute<EventAttribute>();

    AttributeFactory::registerAttribute<CollectionAttribute>();
    AttributeFactory::registerAttribute<CompatibilityAttribute>();
    AttributeFactory::registerAttribute<EventAttribute>();

    if (!mTextIcon)
    {
        mTextIcon    = new QPixmap(SmallIcon(QLatin1String("dialog-information")));
        mFileIcon    = new QPixmap(SmallIcon(QLatin1String("document-open")));
        mCommandIcon = new QPixmap(SmallIcon(QLatin1String("system-run")));
        mEmailIcon   = new QPixmap(SmallIcon(QLatin1String("mail-message-unread")));
        mAudioIcon   = new QPixmap(SmallIcon(QLatin1String("audio-x-generic")));
        mIconSize = mTextIcon->size().expandedTo(mFileIcon->size()).expandedTo(mCommandIcon->size()).expandedTo(mEmailIcon->size()).expandedTo(mAudioIcon->size());
    }

#ifdef __GNUC__
#warning Only want to monitor collection properties, not content, when this becomes possible
#endif
    connect(monitor, SIGNAL(collectionChanged(Akonadi::Collection,QSet<QByteArray>)), SLOT(slotCollectionChanged(Akonadi::Collection,QSet<QByteArray>)));
    connect(monitor, SIGNAL(collectionRemoved(Akonadi::Collection)), SLOT(slotCollectionRemoved(Akonadi::Collection)));
    connect(CalendarMigrator::instance(), SIGNAL(creating(QString,bool)), SLOT(slotCollectionBeingCreated(QString,bool)));
    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    Preferences::connect(SIGNAL(archivedColourChanged(QColor)), this, SLOT(slotUpdateArchivedColour(QColor)));
    Preferences::connect(SIGNAL(disabledColourChanged(QColor)), this, SLOT(slotUpdateDisabledColour(QColor)));
    Preferences::connect(SIGNAL(holidaysChanged(KHolidays::HolidayRegion)), this, SLOT(slotUpdateHolidays()));
    Preferences::connect(SIGNAL(workTimeChanged(QTime,QTime,QBitArray)), this, SLOT(slotUpdateWorkingHours()));

    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(slotRowsInserted(QModelIndex,int,int)));
    connect(this, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), SLOT(slotRowsAboutToBeRemoved(QModelIndex,int,int)));
    connect(monitor, SIGNAL(itemChanged(Akonadi::Item,QSet<QByteArray>)), SLOT(slotMonitoredItemChanged(Akonadi::Item,QSet<QByteArray>)));

    connect(ServerManager::self(), SIGNAL(stateChanged(Akonadi::ServerManager::State)),
                                   SLOT(checkResources(Akonadi::ServerManager::State)));
    checkResources(ServerManager::state());
}

/******************************************************************************
* Called when the server manager is running, i.e. the agent manager knows about
* all existing resources.
* Once it is running, if necessary migrate any KResources alarm calendars from
* pre-Akonadi versions of KAlarm, or create default Akonadi calendar resources
* if any are missing.
*/
void AkonadiModel::checkResources(ServerManager::State state)
{
    if (!mResourcesChecked  &&  state == ServerManager::Running)
    {
        mResourcesChecked = true;
        CalendarMigrator::execute();
    }
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant AkonadiModel::data(const QModelIndex& index, int role) const
{
    // First check that it's a role we're interested in - if not, use the base method
    switch (role)
    {
        case Qt::BackgroundRole:
        case Qt::ForegroundRole:
        case Qt::DisplayRole:
        case Qt::TextAlignmentRole:
        case Qt::DecorationRole:
        case Qt::SizeHintRole:
        case Qt::AccessibleTextRole:
        case Qt::ToolTipRole:
        case Qt::CheckStateRole:
        case SortRole:
        case ValueRole:
        case StatusRole:
        case AlarmActionsRole:
        case AlarmSubActionRole:
        case EnabledRole:
        case EnabledTypesRole:
        case CommandErrorRole:
        case BaseColourRole:
        case AlarmTypeRole:
        case IsStandardRole:
            break;
        default:
            return EntityTreeModel::data(index, role);
    }

    const Collection collection = index.data(CollectionRole).value<Collection>();
    if (collection.isValid())
    {
        // This is a Collection row
        switch (role)
        {
            case Qt::DisplayRole:
                return collection.displayName();
            case EnabledTypesRole:
                if (!collection.hasAttribute<CollectionAttribute>())
                    return 0;
                return static_cast<int>(collection.attribute<CollectionAttribute>()->enabled());
            case BaseColourRole:
                role = Qt::BackgroundRole;
                break;
            case Qt::BackgroundRole:
            {
                QColor colour = backgroundColor_p(collection);
                if (colour.isValid())
                    return colour;
                break;
            }
            case Qt::ForegroundRole:
                return foregroundColor(collection, collection.contentMimeTypes());
            case Qt::ToolTipRole:
                return tooltip(collection, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
            case AlarmTypeRole:
                return static_cast<int>(types(collection));
            case IsStandardRole:
                if (!collection.hasAttribute<CollectionAttribute>()
                ||  !isCompatible(collection))
                    return 0;
                return static_cast<int>(collection.attribute<CollectionAttribute>()->standard());
            case KeepFormatRole:
                if (!collection.hasAttribute<CollectionAttribute>())
                    return false;
                return collection.attribute<CollectionAttribute>()->keepFormat();
            default:
                break;
        }
    }
    else
    {
        const Item item = index.data(ItemRole).value<Item>();
        if (item.isValid())
        {
            // This is an Item row
            QString mime = item.mimeType();
            if ((mime != KAlarmCal::MIME_ACTIVE  &&  mime != KAlarmCal::MIME_ARCHIVED  &&  mime != KAlarmCal::MIME_TEMPLATE)
            ||  !item.hasPayload<KAEvent>())
                return QVariant();
            switch (role)
            {
                case StatusRole:
                    // Mime type has a one-to-one relationship to event's category()
                    if (mime == KAlarmCal::MIME_ACTIVE)
                        return CalEvent::ACTIVE;
                    if (mime == KAlarmCal::MIME_ARCHIVED)
                        return CalEvent::ARCHIVED;
                    if (mime == KAlarmCal::MIME_TEMPLATE)
                        return CalEvent::TEMPLATE;
                    return QVariant();
                case CommandErrorRole:
                    if (!item.hasAttribute<EventAttribute>())
                        return KAEvent::CMD_NO_ERROR;
                    return item.attribute<EventAttribute>()->commandError();
                default:
                    break;
            }
            int column = index.column();
            if (role == Qt::WhatsThisRole)
                return whatsThisText(column);
            const KAEvent event(this->event(item));
            if (!event.isValid())
                return QVariant();
            if (role == AlarmActionsRole)
                return event.actionTypes();
            if (role == AlarmSubActionRole)
                return event.actionSubType();
            bool calendarColour = false;
            switch (column)
            {
                case TimeColumn:
                    switch (role)
                    {
                        case Qt::BackgroundRole:
                            calendarColour = true;
                            break;
                        case Qt::DisplayRole:
                            if (event.expired())
                                return AlarmTime::alarmTimeText(event.startDateTime());
                            return AlarmTime::alarmTimeText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER));
                        case SortRole:
                        {
                            DateTime due;
                            if (event.expired())
                                due = event.startDateTime();
                            else
                                due = event.nextTrigger(KAEvent::DISPLAY_TRIGGER);
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
                            calendarColour = true;
                            break;
                        case Qt::DisplayRole:
                            if (event.expired())
                                return QString();
                            return AlarmTime::timeToAlarmText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER));
                        case SortRole:
                        {
                            if (event.expired())
                                return -1;
                            DateTime due = event.nextTrigger(KAEvent::DISPLAY_TRIGGER);
                            KDateTime now = KDateTime::currentUtcDateTime();
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
                            calendarColour = true;
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
                        {
                            KAEvent::Actions type = event.actionTypes();
                            if (type & KAEvent::ACT_DISPLAY)
                                return event.bgColour();
                            if (type == KAEvent::ACT_COMMAND)
                            {
                                if (event.commandError() != KAEvent::CMD_NO_ERROR)
                                    return Qt::red;
                            }
                            break;
                        }
                        case Qt::ForegroundRole:
                            if (event.commandError() != KAEvent::CMD_NO_ERROR)
                            {
                                if (event.actionTypes() == KAEvent::ACT_COMMAND)
                                    return Qt::white;
                                QColor colour = Qt::red;
                                int r, g, b;
                                event.bgColour().getRgb(&r, &g, &b);
                                if (r > 128  &&  g <= 128  &&  b <= 128)
                                    colour = Qt::white;
                                return colour;
                            }
                            break;
                        case Qt::DisplayRole:
                            if (event.commandError() != KAEvent::CMD_NO_ERROR)
                                return QString::fromLatin1("!");
                            break;
                        case SortRole:
                        {
                            unsigned i = (event.actionTypes() == KAEvent::ACT_DISPLAY)
                                         ? event.bgColour().rgb() : 0;
                            return QString::fromLatin1("%1").arg(i, 6, 10, QLatin1Char('0'));
                        }
                        default:
                            break;
                    }
                    break;
                case TypeColumn:
                    switch (role)
                    {
                        case Qt::BackgroundRole:
                            calendarColour = true;
                            break;
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
                            return static_cast<int>(event.actionSubType());
                        case SortRole:
                            return QString::fromLatin1("%1").arg(event.actionSubType(), 2, 10, QLatin1Char('0'));
                    }
                    break;
                case TextColumn:
                    switch (role)
                    {
                        case Qt::BackgroundRole:
                            calendarColour = true;
                            break;
                        case Qt::DisplayRole:
                        case SortRole:
                            return AlarmText::summary(event, 1);
                        case Qt::ToolTipRole:
                            return AlarmText::summary(event, 10);
                        default:
                            break;
                    }
                    break;
                case TemplateNameColumn:
                    switch (role)
                    {
                        case Qt::BackgroundRole:
                            calendarColour = true;
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

            switch (role)
            {
                case Qt::ForegroundRole:
                    if (!event.enabled())
                           return Preferences::disabledColour();
                    if (event.expired())
                           return Preferences::archivedColour();
                    break;   // use the default for normal active alarms
                case Qt::ToolTipRole:
                    // Show the last command execution error message
                    switch (event.commandError())
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
                case EnabledRole:
                    return event.enabled();
                default:
                    break;
            }

            if (calendarColour)
            {
                Collection parent = item.parentCollection();
                QColor colour = backgroundColor(parent);
                if (colour.isValid())
                    return colour;
            }
        }
    }
    return EntityTreeModel::data(index, role);
}

/******************************************************************************
* Set the font to use for all items, or the checked state of one item.
* The font must always be set at initialisation.
*/
bool AkonadiModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;
    // NOTE: need to emit dataChanged() whenever something is updated (except via a job).
    Collection collection = index.data(CollectionRole).value<Collection>();
    if (collection.isValid())
    {
        // This is a Collection row
        bool updateCollection = false;
        CollectionAttribute* attr = 0;
        switch (role)
        {
            case Qt::BackgroundRole:
            {
                QColor colour = value.value<QColor>();
                attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
                if (attr->backgroundColor() == colour)
                    return true;   // no change
                attr->setBackgroundColor(colour);
                updateCollection = true;
                break;
            }
            case EnabledTypesRole:
            {
                CalEvent::Types types = static_cast<CalEvent::Types>(value.value<int>());
                attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
                if (attr->enabled() == types)
                    return true;   // no change
                kDebug() << "Set enabled:" << types << ", was=" << attr->enabled();
                attr->setEnabled(types);
                updateCollection = true;
                break;
            }
            case IsStandardRole:
                if (collection.hasAttribute<CollectionAttribute>()
                &&  isCompatible(collection))
                {
                    CalEvent::Types types = static_cast<CalEvent::Types>(value.value<int>());
                    attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
kDebug()<<"Set standard:"<<types<<", was="<<attr->standard();
                    attr->setStandard(types);
                    updateCollection = true;
                }
                break;
            case KeepFormatRole:
            {
                bool keepFormat = value.value<bool>();
                attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
                if (attr->keepFormat() == keepFormat)
                    return true;   // no change
                attr->setKeepFormat(keepFormat);
                updateCollection = true;
                break;
            }
            default:
                break;
        }
        if (updateCollection)
        {
            // Update the CollectionAttribute value.
            // Note that we can't supply 'collection' to CollectionModifyJob since
            // that also contains the CompatibilityAttribute value, which is read-only
            // for applications. So create a new Collection instance and only set a
            // value for CollectionAttribute.
            Collection c(collection.id());
            CollectionAttribute* att = c.attribute<CollectionAttribute>(Entity::AddIfMissing);
            *att = *attr;
            CollectionModifyJob* job = new CollectionModifyJob(c, this);
            connect(job, SIGNAL(result(KJob*)), this, SLOT(modifyCollectionJobDone(KJob*)));
            return true;
        }
    }
    else
    {
        Item item = index.data(ItemRole).value<Item>();
        if (item.isValid())
        {
            bool updateItem = false;
            switch (role)
            {
                case CommandErrorRole:
                {
                    KAEvent::CmdErrType err = static_cast<KAEvent::CmdErrType>(value.toInt());
                    switch (err)
                    {
                        case KAEvent::CMD_NO_ERROR:
                        case KAEvent::CMD_ERROR:
                        case KAEvent::CMD_ERROR_PRE:
                        case KAEvent::CMD_ERROR_POST:
                        case KAEvent::CMD_ERROR_PRE_POST:
                        {
                            if (err == KAEvent::CMD_NO_ERROR  &&  !item.hasAttribute<EventAttribute>())
                                return true;   // no change
                            EventAttribute* attr = item.attribute<EventAttribute>(Entity::AddIfMissing);
                            if (attr->commandError() == err)
                                return true;   // no change
                            attr->setCommandError(err);
                            updateItem = true;
kDebug()<<"Item:"<<item.id()<<"  CommandErrorRole ->"<<err;
                            break;
                        }
                        default:
                            return false;
                    }
                    break;
                }
                default:
kDebug()<<"Item: passing to EntityTreeModel::setData("<<role<<")";
                    break;
            }
            if (updateItem)
            {
                queueItemModifyJob(item);
                return true;
            }
        }
    }

    return EntityTreeModel::setData(index, value, role);
}

/******************************************************************************
* Return the number of columns for either a collection or an item.
*/
int AkonadiModel::entityColumnCount(HeaderGroup group) const
{
    switch (group)
    {
        case CollectionTreeHeaders:
            return 1;
        case ItemListHeaders:
            return ColumnCount;
        default:
            return EntityTreeModel::entityColumnCount(group);
    }
}

/******************************************************************************
* Return data for a column heading.
*/
QVariant AkonadiModel::entityHeaderData(int section, Qt::Orientation orientation, int role, HeaderGroup group) const
{
    if (orientation == Qt::Horizontal)
    {
        switch (group)
        {
            case CollectionTreeHeaders:
                if (section != 0)
                    return QVariant();
                if (role == Qt::DisplayRole)
                    return i18nc("@title:column", "Calendars");
                break;

            case ItemListHeaders:
                if (section < 0  ||  section >= ColumnCount)
                    return QVariant();
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
                break;

            default:
                break;
        }
    }
    return EntityTreeModel::entityHeaderData(section, orientation, role, group);
}

/******************************************************************************
* Recursive function to emit the dataChanged() signal for all items in a
* specified column range.
*/
void AkonadiModel::signalDataChanged(bool (*checkFunc)(const Item&), int startColumn, int endColumn, const QModelIndex& parent)
{
    int start = -1;
    int end   = -1;
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Item item = data(ix, ItemRole).value<Item>();
        bool isItem = item.isValid();
        if (isItem)
        {
            if ((*checkFunc)(item))
            {
                // For efficiency, emit a single signal for each group of
                // consecutive items, rather than a separate signal for each item.
                if (start < 0)
                    start = row;
                end = row;
                continue;
            }
        }
        if (start >= 0)
            emit dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
        start = -1;
        if (!isItem)
            signalDataChanged(checkFunc, startColumn, endColumn, ix);
    }

    if (start >= 0)
        emit dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
}

/******************************************************************************
* Signal every minute that the time-to-alarm values have changed.
*/
static bool checkItem_isActive(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ACTIVE; }

void AkonadiModel::slotUpdateTimeTo()
{
    signalDataChanged(&checkItem_isActive, TimeToColumn, TimeToColumn, QModelIndex());
}


/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
static bool checkItem_isArchived(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ARCHIVED; }

void AkonadiModel::slotUpdateArchivedColour(const QColor&)
{
    kDebug();
    signalDataChanged(&checkItem_isArchived, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the colour used to display disabled alarms has changed.
*/
static bool checkItem_isDisabled(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid())
            return !event.enabled();
    }
    return false;
}

void AkonadiModel::slotUpdateDisabledColour(const QColor&)
{
    kDebug();
    signalDataChanged(&checkItem_isDisabled, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the definition of holidays has changed.
*/
static bool checkItem_excludesHolidays(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.holidaysExcluded())
            return true;
    }
    return false;
}

void AkonadiModel::slotUpdateHolidays()
{
    kDebug();
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_excludesHolidays, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the definition of working hours has changed.
*/
static bool checkItem_workTimeOnly(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.workTimeOnly())
            return true;
    }
    return false;
}

void AkonadiModel::slotUpdateWorkingHours()
{
    kDebug();
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_workTimeOnly, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the command error status of an alarm has changed, to save the new
* status and update the visual command error indication.
*/
void AkonadiModel::updateCommandError(const KAEvent& event)
{
    QModelIndex ix = itemIndex(event.itemId());
    if (ix.isValid())
        setData(ix, QVariant(static_cast<int>(event.commandError())), CommandErrorRole);
}

/******************************************************************************
* Return the foreground color for displaying a collection, based on the
* supplied mime types which it contains, and on whether it is fully writable.
*/
QColor AkonadiModel::foregroundColor(const Akonadi::Collection& collection, const QStringList& mimeTypes)
{
    QColor colour;
    if (mimeTypes.contains(KAlarmCal::MIME_ACTIVE))
        colour = KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText).color();
    else if (mimeTypes.contains(KAlarmCal::MIME_ARCHIVED))
        colour = Preferences::archivedColour();
    else if (mimeTypes.contains(KAlarmCal::MIME_TEMPLATE))
        colour = KColorScheme(QPalette::Active).foreground(KColorScheme::LinkText).color();
    if (colour.isValid()  &&  isWritable(collection) <= 0)
        return KColorUtils::lighten(colour, 0.2);
    return colour;
}

/******************************************************************************
* Set the background color for displaying the collection and its alarms.
*/
void AkonadiModel::setBackgroundColor(Collection& collection, const QColor& colour)
{
    QModelIndex ix = modelIndexForCollection(this, collection);
    if (ix.isValid())
        setData(ix, QVariant(colour), Qt::BackgroundRole);
}

/******************************************************************************
* Return the background color for displaying the collection and its alarms,
* after updating the collection from the Akonadi database.
*/
QColor AkonadiModel::backgroundColor(Akonadi::Collection& collection) const
{
    if (!collection.isValid())
        return QColor();
    refresh(collection);
    return backgroundColor_p(collection);
}

/******************************************************************************
* Return the background color for displaying the collection and its alarms.
*/
QColor AkonadiModel::backgroundColor_p(const Akonadi::Collection& collection) const
{
    if (!collection.isValid()  ||  !collection.hasAttribute<CollectionAttribute>())
        return QColor();
    return collection.attribute<CollectionAttribute>()->backgroundColor();
}

/******************************************************************************
* Return the display name for the collection, after updating the collection
* from the Akonadi database.
*/
QString AkonadiModel::displayName(Akonadi::Collection& collection) const
{
    if (!collection.isValid())
        return QString();
    refresh(collection);
    return collection.displayName();
}

/******************************************************************************
* Return the storage type (file, directory, etc.) for the collection.
*/
QString AkonadiModel::storageType(const Akonadi::Collection& collection) const
{
    KUrl url = collection.remoteId();
    return !url.isLocalFile()                     ? i18nc("@info/plain", "URL")
           : QFileInfo(url.toLocalFile()).isDir() ? i18nc("@info/plain Directory in filesystem", "Directory")
           :                                        i18nc("@info/plain", "File");
}

/******************************************************************************
* Return a collection's tooltip text. The collection's enabled status is
* evaluated for specified alarm types.
*/
QString AkonadiModel::tooltip(const Collection& collection, CalEvent::Types types) const
{
    QString name = QLatin1Char('@') + collection.displayName();   // insert markers for stripping out name
    KUrl url = collection.remoteId();
    QString type = QLatin1Char('@') + storageType(collection);   // file/directory/URL etc.
    QString locn = url.pathOrUrl();
    bool inactive = !collection.hasAttribute<CollectionAttribute>()
                 || !(collection.attribute<CollectionAttribute>()->enabled() & types);
    QString disabled = i18nc("@info/plain", "Disabled");
    QString readonly = readOnlyTooltip(collection);
    bool writable = readonly.isEmpty();
    if (inactive  &&  !writable)
        return i18nc("@info:tooltip",
                     "%1"
                     "<nl/>%2: <filename>%3</filename>"
                     "<nl/>%4, %5",
                     name, type, locn, disabled, readonly);
    if (inactive  ||  !writable)
        return i18nc("@info:tooltip",
                     "%1"
                     "<nl/>%2: <filename>%3</filename>"
                     "<nl/>%4",
                     name, type, locn, (inactive ? disabled : readonly));
    return i18nc("@info:tooltip",
                 "%1"
                 "<nl/>%2: <filename>%3</filename>",
                 name, type, locn);
}

/******************************************************************************
* Return the read-only status tooltip for a collection.
* A null string is returned if the collection is fully writable.
*/
QString AkonadiModel::readOnlyTooltip(const Collection& collection)
{
    KACalendar::Compat compat;
    switch (AkonadiModel::isWritable(collection, compat))
    {
        case 1:
            return QString();
        case 0:
            return i18nc("@info/plain", "Read-only (old format)");
        default:
            if (compat == KACalendar::Current)
                return i18nc("@info/plain", "Read-only");
            return i18nc("@info/plain", "Read-only (other format)");
    }
}

/******************************************************************************
* Return the repetition text.
*/
QString AkonadiModel::repeatText(const KAEvent& event) const
{
    QString repeatText = event.recurrenceText(true);
    if (repeatText.isEmpty())
        repeatText = event.repetitionText(true);
    return repeatText;
}

/******************************************************************************
* Return a string for sorting the repetition column.
*/
QString AkonadiModel::repeatOrder(const KAEvent& event) const
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
    return QString::fromLatin1("%1%2").arg(static_cast<char>('0' + repeatOrder)).arg(repeatInterval, 8, 10, QLatin1Char('0'));
}

/******************************************************************************
* Return the icon associated with the event's action.
*/
QPixmap* AkonadiModel::eventIcon(const KAEvent& event) const
{
    switch (event.actionTypes())
    {
        case KAEvent::ACT_EMAIL:
            return mEmailIcon;
        case KAEvent::ACT_AUDIO:
            return mAudioIcon;
        case KAEvent::ACT_COMMAND:
            return mCommandIcon;
        case KAEvent::ACT_DISPLAY:
            if (event.actionSubType() == KAEvent::FILE)
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
QString AkonadiModel::whatsThisText(int column) const
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

/******************************************************************************
* Remove a collection from Akonadi. The calendar file is not removed.
*/
bool AkonadiModel::removeCollection(const Akonadi::Collection& collection)
{
    if (!collection.isValid())
        return false;
    kDebug() << collection.id();
    Collection col = collection;
    mCollectionsDeleting << collection.id();
    // Note: CollectionDeleteJob deletes the backend storage also.
    AgentManager* agentManager = AgentManager::self();
    const AgentInstance instance = agentManager->instance(collection.resource());
    if (instance.isValid())
        agentManager->removeInstance(instance);
#if 0
    CollectionDeleteJob* job = new CollectionDeleteJob(col);
    connect(job, SIGNAL(result(KJob*)), SLOT(deleteCollectionJobDone(KJob*)));
    mPendingCollectionJobs[job] = CollJobData(col.id(), displayName(col));
    job->start();
#endif
    return true;
}

/******************************************************************************
* Return whether a collection is currently being deleted.
*/
bool AkonadiModel::isCollectionBeingDeleted(Collection::Id id) const
{
    return mCollectionsDeleting.contains(id);
}

#if 0
/******************************************************************************
* Called when a collection deletion job has completed.
* Checks for any error.
*/
void AkonadiModel::deleteCollectionJobDone(KJob* j)
{
    QMap<KJob*, CollJobData>::iterator it = mPendingCollectionJobs.find(j);
    CollJobData jobData;
    if (it != mPendingCollectionJobs.end())
    {
        jobData = it.value();
        mPendingCollectionJobs.erase(it);
    }
    if (j->error())
    {
        emit collectionDeleted(jobData.id, false);
        QString errMsg = i18nc("@info", "Failed to remove calendar <resource>%1</resource>.", jobData.displayName);
        kError() << errMsg << ":" << j->errorString();
        KAMessageBox::error(MainWindow::mainMainWindow(), i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
    }
    else
        emit collectionDeleted(jobData.id, true);
}
#endif

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
bool AkonadiModel::reloadCollection(const Akonadi::Collection& collection)
{
    if (!collection.isValid())
        return false;
    kDebug() << collection.id();
    mMonitor->setCollectionMonitored(collection, false);
    mMonitor->setCollectionMonitored(collection, true);
    return true;
}

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
void AkonadiModel::reload()
{
    kDebug();
    const Collection::List collections = mMonitor->collectionsMonitored();
    foreach (const Collection& collection, collections)
    {
        mMonitor->setCollectionMonitored(collection, false);
        mMonitor->setCollectionMonitored(collection, true);
    }
}

/******************************************************************************
* Called when a collection modification job has completed.
* Checks for any error.
*/
void AkonadiModel::modifyCollectionJobDone(KJob* j)
{
    Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
    Collection::Id id = collection.id();
    if (j->error())
    {
        emit collectionModified(id, false);
        if (mCollectionsDeleted.contains(id))
            mCollectionsDeleted.removeAll(id);
        else
        {
            QString errMsg = i18nc("@info", "Failed to update calendar <resource>%1</resource>.", displayName(collection));
            kError() << "Id:" << collection.id() << errMsg << ":" << j->errorString();
            KAMessageBox::error(MainWindow::mainMainWindow(), i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
        }
    }
    else
        emit collectionModified(id, true);
}

/******************************************************************************
* Returns the index to a specified event.
*/
QModelIndex AkonadiModel::eventIndex(const KAEvent& event)
{
    return itemIndex(event.itemId());
}

#if 0
/******************************************************************************
* Return all events of a given type belonging to a collection.
*/
KAEvent::List AkonadiModel::events(Akonadi::Collection& collection, CalEvent::Type type) const
{
    KAEvent::List list;
    QModelIndex ix = modelIndexForCollection(this, collection);
    if (ix.isValid())
        getChildEvents(ix, type, list);
    return list;
}

/******************************************************************************
* Recursive function to append all child Events with a given mime type.
*/
void AkonadiModel::getChildEvents(const QModelIndex& parent, CalEvent::Type type, KAEvent::List& events) const
{
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Item item = data(ix, ItemRole).value<Item>();
        if (item.isValid())
        {
            if (item.hasPayload<KAEvent>())
            {
                KAEvent event = item.payload<KAEvent>();
                if (event.isValid()  &&  event.category() == type)
                    events += event;
            }
        }
        else
        {
            Collection c = ix.data(CollectionRole).value<Collection>();
            if (c.isValid())
                getChildEvents(ix, type, events);
        }
    }
}
#endif

KAEvent AkonadiModel::event(Item::Id itemId) const
{
    QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return KAEvent();
    return event(ix.data(ItemRole).value<Item>(), ix, 0);
}

KAEvent AkonadiModel::event(const QModelIndex& index) const
{
    return event(index.data(ItemRole).value<Item>(), index, 0);
}

KAEvent AkonadiModel::event(const Item& item, const QModelIndex& index, Collection* collection) const
{
    if (!item.isValid()  ||  !item.hasPayload<KAEvent>())
        return KAEvent();
    const QModelIndex ix = index.isValid() ? index : itemIndex(item.id());
    if (!ix.isValid())
        return KAEvent();
    KAEvent e = item.payload<KAEvent>();
    if (e.isValid())
    {

        Collection c = data(ix, ParentCollectionRole).value<Collection>();
        // Set collection ID using a const method, to avoid unnecessary copying of KAEvent
        e.setCollectionId_const(c.id());
        if (collection)
            *collection = c;
    }
    return e;
}

#if 0
/******************************************************************************
* Add an event to the default or a user-selected Collection.
*/
AkonadiModel::Result AkonadiModel::addEvent(KAEvent* event, CalEvent::Type type, QWidget* promptParent, bool noPrompt)
{
    kDebug() << event->id();

    // Determine parent collection - prompt or use default
    bool cancelled;
    Collection collection = destination(type, Collection::CanCreateItem, promptParent, noPrompt, &cancelled);
    if (!collection.isValid())
    {
        delete event;
        if (cancelled)
            return Cancelled;
        kDebug() << "No collection";
        return Failed;
    }
    if (!addEvent(event, collection))
    {
        kDebug() << "Failed";
        return Failed;    // event was deleted by addEvent()
    }
    return Success;
}
#endif

/******************************************************************************
* Add events to a specified Collection.
* Events which are scheduled to be added to the collection are updated with
* their Akonadi item ID.
* The caller must connect to the itemDone() signal to check whether events
* have been added successfully. Note that the first signal may be emitted
* before this function returns.
* Reply = true if item creation has been scheduled for all events,
*       = false if at least one item creation failed to be scheduled.
*/
bool AkonadiModel::addEvents(const KAEvent::List& events, Collection& collection)
{
    bool ok = true;
    for (int i = 0, count = events.count();  i < count;  ++i)
        ok = ok && addEvent(*events[i], collection);
    return ok;
}

/******************************************************************************
* Add an event to a specified Collection.
* If the event is scheduled to be added to the collection, it is updated with
* its Akonadi item ID.
* The event's 'updated' flag is cleared.
* The caller must connect to the itemDone() signal to check whether events
* have been added successfully.
* Reply = true if item creation has been scheduled.
*/
bool AkonadiModel::addEvent(KAEvent& event, Collection& collection)
{
    kDebug() << "ID:" << event.id();
    Item item;
    if (!event.setItemPayload(item, collection.contentMimeTypes()))
    {
        kWarning() << "Invalid mime type for collection";
        return false;
    }
    event.setItemId(item.id());
kDebug()<<"-> item id="<<item.id();
    ItemCreateJob* job = new ItemCreateJob(item, collection);
    connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
    mPendingItemJobs[job] = item.id();
    job->start();
kDebug()<<"...exiting";
    return true;
}

/******************************************************************************
* Update an event in its collection.
* The event retains its existing Akonadi item ID.
* The event's 'updated' flag is cleared.
* The caller must connect to the itemDone() signal to check whether the event
* has been updated successfully.
* Reply = true if item update has been scheduled.
*/
bool AkonadiModel::updateEvent(KAEvent& event)
{
    kDebug() << "ID:" << event.id();
    return updateEvent(event.itemId(), event);
}
bool AkonadiModel::updateEvent(Akonadi::Entity::Id itemId, KAEvent& newEvent)
{
kDebug()<<"item id="<<itemId;
    const QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    const Collection collection = ix.data(ParentCollectionRole).value<Collection>();
    Item item = ix.data(ItemRole).value<Item>();
kDebug()<<"item id="<<item.id()<<", revision="<<item.revision();
    if (!newEvent.setItemPayload(item, collection.contentMimeTypes()))
    {
        kWarning() << "Invalid mime type for collection";
        return false;
    }
//    setData(ix, QVariant::fromValue(item), ItemRole);
    queueItemModifyJob(item);
    return true;
}

/******************************************************************************
* Delete an event from its collection.
*/
bool AkonadiModel::deleteEvent(const KAEvent& event)
{
    return deleteEvent(event.itemId());
}
bool AkonadiModel::deleteEvent(Akonadi::Entity::Id itemId)
{
    kDebug() << itemId;
    QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    if (mCollectionsDeleting.contains(ix.data(ParentCollectionRole).value<Collection>().id()))
    {
        kDebug() << "Collection being deleted";
        return true;    // the event's collection is being deleted
    }
    Item item = ix.data(ItemRole).value<Item>();
    ItemDeleteJob* job = new ItemDeleteJob(item);
    connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
    mPendingItemJobs[job] = itemId;
    job->start();
    return true;
}

/******************************************************************************
* Queue an ItemModifyJob for execution. Ensure that only one job is
* simultaneously active for any one Item.
*
* This is necessary because we can't call two ItemModifyJobs for the same Item
* at the same time; otherwise Akonadi will detect a conflict and require manual
* intervention to resolve it.
*/
void AkonadiModel::queueItemModifyJob(const Item& item)
{
    kDebug() << item.id();
    QMap<Item::Id, Item>::Iterator it = mItemModifyJobQueue.find(item.id());
    if (it != mItemModifyJobQueue.end())
    {
        // A job is already queued for this item. Replace the queued item value with the new one.
        kDebug() << "Replacing previously queued job";
        it.value() = item;
    }
    else
    {
        // There is no job already queued for this item
        if (mItemsBeingCreated.contains(item.id()))
        {
            kDebug() << "Waiting for item initialisation";
            mItemModifyJobQueue[item.id()] = item;   // wait for item initialisation to complete
        }
        else
        {
            Item newItem = item;
            Item current = itemById(item.id());    // fetch the up-to-date item
            if (current.isValid())
                newItem.setRevision(current.revision());
            mItemModifyJobQueue[item.id()] = Item();   // mark the queued item as now executing
            ItemModifyJob* job = new ItemModifyJob(newItem);
            job->disableRevisionCheck();
            connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
            mPendingItemJobs[job] = item.id();
            kDebug() << "Executing Modify job for item" << item.id() << ", revision=" << newItem.revision();
        }
    }
}

/******************************************************************************
* Called when an item job has completed.
* Checks for any error.
* Note that for an ItemModifyJob, the item revision number may not be updated
* to the post-modification value. The next queued ItemModifyJob is therefore
* not kicked off from here, but instead from the slot attached to the
* itemChanged() signal, which has the revision updated.
*/
void AkonadiModel::itemJobDone(KJob* j)
{
    QMap<KJob*, Entity::Id>::iterator it = mPendingItemJobs.find(j);
    Entity::Id itemId = -1;
    if (it != mPendingItemJobs.end())
    {
        itemId = it.value();
        mPendingItemJobs.erase(it);
    }
    QByteArray jobClass = j->metaObject()->className();
    kDebug() << jobClass;
    if (j->error())
    {
        QString errMsg;
        if (jobClass == "Akonadi::ItemCreateJob")
            errMsg = i18nc("@info/plain", "Failed to create alarm.");
        else if (jobClass == "Akonadi::ItemModifyJob")
            errMsg = i18nc("@info/plain", "Failed to update alarm.");
        else if (jobClass == "Akonadi::ItemDeleteJob")
            errMsg = i18nc("@info/plain", "Failed to delete alarm.");
        else
            Q_ASSERT(0);
        kError() << errMsg << itemId << ":" << j->errorString();
        emit itemDone(itemId, false);

        if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
        {
            // Execute the next queued job for this item
            Item current = itemById(itemId);    // fetch the up-to-date item
            checkQueuedItemModifyJob(current);
        }
        KAMessageBox::error(MainWindow::mainMainWindow(), i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
    }
    else
    {
        if (jobClass == "Akonadi::ItemCreateJob")
        {
            // Prevent modification of the item until it is fully initialised.
            // Either slotMonitoredItemChanged() or slotRowsInserted(), or both,
            // will be called when the item is done.
            kDebug() << "item id=" << static_cast<ItemCreateJob*>(j)->item().id();
            mItemsBeingCreated << static_cast<ItemCreateJob*>(j)->item().id();
        }
        emit itemDone(itemId);
    }

/*    if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
    {
        QMap<Item::Id, Item>::iterator it = mItemModifyJobQueue.find(itemId);
        if (it != mItemModifyJobQueue.end())
        {
            if (!it.value().isValid())
                mItemModifyJobQueue.erase(it);   // there are no more jobs queued for the item
        }
    }*/
}

/******************************************************************************
* Check whether there are any ItemModifyJobs waiting for a specified item, and
* if so execute the first one provided its creation has completed. This
* prevents clashes in Akonadi conflicts between simultaneous ItemModifyJobs for
* the same item.
*
* Note that when an item is newly created (e.g. via addEvent()), the KAlarm
* resource itemAdded() function creates an ItemModifyJob to give it a remote
* ID. Until that job is complete, any other ItemModifyJob for the item will
* cause a conflict.
*/
void AkonadiModel::checkQueuedItemModifyJob(const Item& item)
{
    if (mItemsBeingCreated.contains(item.id()))
{kDebug()<<"Still being created";
        return;    // the item hasn't been fully initialised yet
}
    QMap<Item::Id, Item>::iterator it = mItemModifyJobQueue.find(item.id());
    if (it == mItemModifyJobQueue.end())
{kDebug()<<"No jobs queued";
        return;    // there are no jobs queued for the item
}
    Item qitem = it.value();
    if (!qitem.isValid())
    {
        // There is no further job queued for the item, so remove the item from the list
kDebug()<<"No more jobs queued";
        mItemModifyJobQueue.erase(it);
    }
    else
    {
        // Queue the next job for the Item, after updating the Item's
        // revision number to match that set by the job just completed.
        qitem.setRevision(item.revision());
        mItemModifyJobQueue[item.id()] = Item();   // mark the queued item as now executing
        ItemModifyJob* job = new ItemModifyJob(qitem);
        job->disableRevisionCheck();
        connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
        mPendingItemJobs[job] = qitem.id();
        kDebug() << "Executing queued Modify job for item" << qitem.id() << ", revision=" << qitem.revision();
    }
}

/******************************************************************************
* Called when rows have been inserted into the model.
*/
void AkonadiModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    kDebug() << start << "-" << end << "(parent =" << parent << ")";
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Collection collection = ix.data(CollectionRole).value<Collection>();
        if (collection.isValid())
        {
            // A collection has been inserted.
            // Ignore it if it isn't owned by a valid resource.
            kDebug() << "Collection" << collection.id() << collection.name();
            if (AgentManager::self()->instance(collection.resource()).isValid())
            {
                QSet<QByteArray> attrs;
                attrs += CollectionAttribute::name();
                setCollectionChanged(collection, attrs, true);
                emit collectionAdded(collection);

                if (!mCollectionsBeingCreated.contains(collection.remoteId())
                &&  (collection.rights() & writableRights) == writableRights)
                {
                    // Update to current KAlarm format if necessary, and if the user agrees
                    CalendarMigrator::updateToCurrentFormat(collection, false, MainWindow::mainMainWindow());
                }
            }
        }
        else
        {
            // An item has been inserted
            const Item item = ix.data(ItemRole).value<Item>();
            if (item.isValid())
            {
                kDebug() << "item id=" << item.id() << ", revision=" << item.revision();
                if (mItemsBeingCreated.removeAll(item.id()))   // the new item has now been initialised
                    checkQueuedItemModifyJob(item);    // execute the next job queued for the item
            }
        }
    }
    EventList events = eventList(parent, start, end);
    if (!events.isEmpty())
        emit eventsAdded(events);
}

/******************************************************************************
* Called when rows are about to be removed from the model.
*/
void AkonadiModel::slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    kDebug() << start << "-" << end << "(parent =" << parent << ")";
    EventList events = eventList(parent, start, end);
    if (!events.isEmpty())
    {
        foreach (const Event& event, events)
            kDebug() << "Collection:" << event.collection.id() << ", Event ID:" << event.event.id();
        emit eventsToBeRemoved(events);
    }
}

/******************************************************************************
* Return a list of KAEvent/Collection pairs for a given range of rows.
*/
AkonadiModel::EventList AkonadiModel::eventList(const QModelIndex& parent, int start, int end)
{
    EventList events;
    for (int row = start;  row <= end;  ++row)
    {
        Collection c;
        QModelIndex ix = index(row, 0, parent);
        KAEvent evnt = event(ix.data(ItemRole).value<Item>(), ix, &c);
        if (evnt.isValid())
            events += Event(evnt, c);
    }
    return events;
}

/******************************************************************************
* Called when a monitored collection's properties or content have changed.
* Optionally emits a signal if properties of interest have changed.
*/
void AkonadiModel::setCollectionChanged(const Collection& collection, const QSet<QByteArray>& attributeNames, bool rowInserted)
{
    // Check for a read/write permission change
    Collection::Rights oldRights = mCollectionRights.value(collection.id(), Collection::AllRights);
    Collection::Rights newRights = collection.rights() & writableRights;
    if (newRights != oldRights)
    {
        kDebug() << "Collection" << collection.id() << ": rights ->" << newRights;
        mCollectionRights[collection.id()] = newRights;
        emit collectionStatusChanged(collection, ReadOnly, (newRights != writableRights), rowInserted);
    }

    // Check for a change in content mime types
    // (e.g. when a collection is first created at startup).
    CalEvent::Types oldAlarmTypes = mCollectionAlarmTypes.value(collection.id(), CalEvent::EMPTY);
    CalEvent::Types newAlarmTypes = CalEvent::types(collection.contentMimeTypes());
    if (newAlarmTypes != oldAlarmTypes)
    {
        kDebug() << "Collection" << collection.id() << ": alarm types ->" << newAlarmTypes;
        mCollectionAlarmTypes[collection.id()] = newAlarmTypes;
        emit collectionStatusChanged(collection, AlarmTypes, static_cast<int>(newAlarmTypes), rowInserted);
    }

    // Check for the collection being enabled/disabled
    if (attributeNames.contains(CollectionAttribute::name()))
    {
        static bool first = true;
        CalEvent::Types oldEnabled = mCollectionEnabled.value(collection.id(), CalEvent::EMPTY);
        CalEvent::Types newEnabled = collection.hasAttribute<CollectionAttribute>() ? collection.attribute<CollectionAttribute>()->enabled() : CalEvent::EMPTY;
        if (first  ||  newEnabled != oldEnabled)
        {
            kDebug() << "Collection" << collection.id() << ": enabled ->" << newEnabled;
            first = false;
            mCollectionEnabled[collection.id()] = newEnabled;
            emit collectionStatusChanged(collection, Enabled, static_cast<int>(newEnabled), rowInserted);
        }
    }

    // Check for the backend calendar format changing
    if (attributeNames.contains(CompatibilityAttribute::name()))
    {
        // Update to current KAlarm format if necessary, and if the user agrees
        kDebug() << "CompatibilityAttribute";
        Collection col(collection);
        refresh(col);
        CalendarMigrator::updateToCurrentFormat(col, false, MainWindow::mainMainWindow());
    }
}

/******************************************************************************
* Called when a monitored collection is removed.
*/
void AkonadiModel::slotCollectionRemoved(const Collection& collection)
{
    Collection::Id id = collection.id();
    kDebug() << id;
    mCollectionRights.remove(id);
    mCollectionsDeleting.removeAll(id);
    while (mCollectionsDeleted.count() > 20)   // don't let list grow indefinitely
        mCollectionsDeleted.removeFirst();
    mCollectionsDeleted << id;
}

/******************************************************************************
* Called when a collection creation is about to start, or has completed.
*/
void AkonadiModel::slotCollectionBeingCreated(const QString& path, bool finished)
{
    if (finished)
        mCollectionsBeingCreated.removeAll(path);
    else
        mCollectionsBeingCreated << path;
}

/******************************************************************************
* Called when an item in the monitored collections has changed.
*/
void AkonadiModel::slotMonitoredItemChanged(const Akonadi::Item& item, const QSet<QByteArray>&)
{
    kDebug() << "item id=" << item.id() << ", revision=" << item.revision();
    mItemsBeingCreated.removeAll(item.id());   // the new item has now been initialised
    checkQueuedItemModifyJob(item);    // execute the next job queued for the item

    KAEvent evnt = event(item);
    if (!evnt.isValid())
        return;
    const QModelIndexList indexes = modelIndexesForItem(this, item);
    foreach (const QModelIndex& index, indexes)
    {
        if (index.isValid())
        {
            // Wait to ensure that the base EntityTreeModel has processed the
            // itemChanged() signal first, before we emit eventChanged().
            Collection c = data(index, ParentCollectionRole).value<Collection>();
            evnt.setCollectionId(c.id());
            mPendingEventChanges.enqueue(Event(evnt, c));
            QTimer::singleShot(0, this, SLOT(slotEmitEventChanged()));
            break;
        }
    }
}

/******************************************************************************
* Called to emit a signal when an event in the monitored collections has
* changed.
*/
void AkonadiModel::slotEmitEventChanged()
{
    while (!mPendingEventChanges.isEmpty())
    {
        emit eventChanged(mPendingEventChanges.dequeue());
    }
}

/******************************************************************************
* Refresh the specified Collection with up to date data.
* Return: true if successful, false if collection not found.
*/
bool AkonadiModel::refresh(Akonadi::Collection& collection) const
{
    QModelIndex ix = modelIndexForCollection(this, collection);
    if (!ix.isValid())
        return false;
    collection = ix.data(CollectionRole).value<Collection>();
    return true;
}

/******************************************************************************
* Refresh the specified Item with up to date data.
* Return: true if successful, false if item not found.
*/
bool AkonadiModel::refresh(Akonadi::Item& item) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, item);
    if (ixs.isEmpty()  ||  !ixs[0].isValid())
        return false;
    item = ixs[0].data(ItemRole).value<Item>();
    return true;
}

/******************************************************************************
* Find the QModelIndex of a collection.
*/
QModelIndex AkonadiModel::collectionIndex(const Collection& collection) const
{
    QModelIndex ix = modelIndexForCollection(this, collection);
    if (!ix.isValid())
        return QModelIndex();
    return ix;
}

/******************************************************************************
* Return the up to date collection with the specified Akonadi ID.
*/
Collection AkonadiModel::collectionById(Collection::Id id) const
{
    QModelIndex ix = modelIndexForCollection(this, Collection(id));
    if (!ix.isValid())
        return Collection();
    return ix.data(CollectionRole).value<Collection>();
}

/******************************************************************************
* Find the QModelIndex of an item.
*/
QModelIndex AkonadiModel::itemIndex(const Item& item) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, item);
    if (ixs.isEmpty()  ||  !ixs[0].isValid())
        return QModelIndex();
    return ixs[0];
}

/******************************************************************************
* Return the up to date item with the specified Akonadi ID.
*/
Item AkonadiModel::itemById(Item::Id id) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, Item(id));
    if (ixs.isEmpty()  ||  !ixs[0].isValid())
        return Item();
    return ixs[0].data(ItemRole).value<Item>();
}

/******************************************************************************
* Find the collection containing the specified Akonadi item ID.
*/
Collection AkonadiModel::collectionForItem(Item::Id id) const
{
    QModelIndex ix = itemIndex(id);
    if (!ix.isValid())
        return Collection();
    return ix.data(ParentCollectionRole).value<Collection>();
}

bool AkonadiModel::isCompatible(const Collection& collection)
{
    return collection.hasAttribute<CompatibilityAttribute>()
       &&  collection.attribute<CompatibilityAttribute>()->compatibility() == KACalendar::Current;
}

/******************************************************************************
* Return whether a collection is fully writable.
*/
int AkonadiModel::isWritable(const Akonadi::Collection& collection)
{
    KACalendar::Compat format;
    return isWritable(collection, format);
}

int AkonadiModel::isWritable(const Akonadi::Collection& collection, KACalendar::Compat& format)
{
    format = KACalendar::Incompatible;
    if (!collection.isValid())
        return -1;
    Collection col = collection;
    instance()->refresh(col);    // update with latest data
    if ((col.rights() & writableRights) != writableRights)
    {
        format = KACalendar::Current;
        return -1;
    }
    if (!col.hasAttribute<CompatibilityAttribute>())
        return -1;
    format = col.attribute<CompatibilityAttribute>()->compatibility();
    switch (format)
    {
        case KACalendar::Current:
            return 1;
        case KACalendar::Converted:
        case KACalendar::Convertible:
            return 0;
        default:
            return -1;
    }
}

CalEvent::Types AkonadiModel::types(const Collection& collection)
{
    return CalEvent::types(collection.contentMimeTypes());
}

// vim: et sw=4:
