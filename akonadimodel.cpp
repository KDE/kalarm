/*
 *  akonadimodel.cpp  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2007-2011 by David Jarvie <djarvie@kde.org>
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
#include "alarmtext.h"
#include "autoqpointer.h"
#include "collectionattribute.h"
#include "eventattribute.h"
#include "preferences.h"
#include "synchtimer.h"

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
#include <kmessagebox.h>

#include <QApplication>
#include <QFileInfo>
#include <QTimer>

using namespace Akonadi;
using KAlarm::CollectionAttribute;
using KAlarm::EventAttribute;

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
      mMonitor(monitor)
{
    // Set lazy population to enable the contents of unselected collections to be ignored
    setItemPopulationStrategy(LazyPopulation);

    // Restrict monitoring to collections containing the KAlarm mime types
    monitor->setCollectionMonitored(Collection::root());
    monitor->setResourceMonitored("akonadi_kalarm_resource");
    monitor->setResourceMonitored("akonadi_kalarm_dir_resource");
    monitor->setMimeTypeMonitored(KAlarm::MIME_ACTIVE);
    monitor->setMimeTypeMonitored(KAlarm::MIME_ARCHIVED);
    monitor->setMimeTypeMonitored(KAlarm::MIME_TEMPLATE);
    monitor->itemFetchScope().fetchFullPayload();
    monitor->itemFetchScope().fetchAttribute<EventAttribute>();

    AttributeFactory::registerAttribute<CollectionAttribute>();
    AttributeFactory::registerAttribute<EventAttribute>();

    if (!mTextIcon)
    {
        mTextIcon    = new QPixmap(SmallIcon("dialog-information"));
        mFileIcon    = new QPixmap(SmallIcon("document-open"));
        mCommandIcon = new QPixmap(SmallIcon("system-run"));
        mEmailIcon   = new QPixmap(SmallIcon("mail-message-unread"));
        mAudioIcon   = new QPixmap(SmallIcon("audio-x-generic"));
        mIconSize = mTextIcon->size().expandedTo(mFileIcon->size()).expandedTo(mCommandIcon->size()).expandedTo(mEmailIcon->size()).expandedTo(mAudioIcon->size());
    }

#ifdef __GNUC__
#warning Only want to monitor collection properties, not content, when this becomes possible
#endif
    connect(monitor, SIGNAL(collectionChanged(const Akonadi::Collection&, const QSet<QByteArray>&)), SLOT(slotCollectionChanged(const Akonadi::Collection&, const QSet<QByteArray>&)));
    connect(monitor, SIGNAL(collectionRemoved(const Akonadi::Collection&)), SLOT(slotCollectionRemoved(const Akonadi::Collection&)));
    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    Preferences::connect(SIGNAL(archivedColourChanged(const QColor&)), this, SLOT(slotUpdateArchivedColour(const QColor&)));
    Preferences::connect(SIGNAL(disabledColourChanged(const QColor&)), this, SLOT(slotUpdateDisabledColour(const QColor&)));
    Preferences::connect(SIGNAL(holidaysChanged(const KHolidays::HolidayRegion&)), this, SLOT(slotUpdateHolidays()));
    Preferences::connect(SIGNAL(workTimeChanged(const QTime&, const QTime&, const QBitArray&)), this, SLOT(slotUpdateWorkingHours()));

    connect(this, SIGNAL(rowsInserted(const QModelIndex&, int, int)), SLOT(slotRowsInserted(const QModelIndex&, int, int)));
    connect(this, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)), SLOT(slotRowsAboutToBeRemoved(const QModelIndex&, int, int)));
    connect(monitor, SIGNAL(itemChanged(const Akonadi::Item&, const QSet<QByteArray>&)), SLOT(slotMonitoredItemChanged(const Akonadi::Item&, const QSet<QByteArray>&)));
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
        case Qt::FontRole:
        case SortRole:
        case ValueRole:
        case StatusRole:
        case AlarmActionsRole:
        case AlarmActionRole:
        case EnabledRole:
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
                return displayName_p(collection);
            case EnabledRole:
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
            {
                QStringList mimeTypes = collection.contentMimeTypes();
                if (mimeTypes.contains(KAlarm::MIME_ACTIVE))
                    return (collection.rights() & writableRights) == writableRights ? Qt::black : Qt::darkGray;
                if (mimeTypes.contains(KAlarm::MIME_ARCHIVED))
                    return (collection.rights() & writableRights) == writableRights ? Qt::darkGreen : Qt::green;
                if (mimeTypes.contains(KAlarm::MIME_TEMPLATE))
                    return (collection.rights() & writableRights) == writableRights ? Qt::darkBlue : Qt::blue;
                break;
            }
            case Qt::FontRole:
            {
                if (!collection.hasAttribute<CollectionAttribute>())
                    break;
                CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
                if (!attr->enabled())
                    break;
                QStringList mimeTypes = collection.contentMimeTypes();
                if ((mimeTypes.contains(KAlarm::MIME_ACTIVE)  &&  attr->isStandard(KAlarm::CalEvent::ACTIVE))
                ||  (mimeTypes.contains(KAlarm::MIME_ARCHIVED)  &&  attr->isStandard(KAlarm::CalEvent::ARCHIVED))
                ||  (mimeTypes.contains(KAlarm::MIME_TEMPLATE)  &&  attr->isStandard(KAlarm::CalEvent::TEMPLATE)))
                {
                    // It's the standard collection for a mime type
                    QFont font = mFont;
                    font.setBold(true);
                    return font;
                }
                break;
            }
            case Qt::ToolTipRole:
                return tooltip(collection, KAlarm::CalEvent::ALL);
            case AlarmTypeRole:
                return static_cast<int>(types(collection));
            case IsStandardRole:
                if (!collection.hasAttribute<CollectionAttribute>())
                    return 0;
                return static_cast<int>(collection.attribute<CollectionAttribute>()->standard());
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
            if ((mime != KAlarm::MIME_ACTIVE  &&  mime != KAlarm::MIME_ARCHIVED  &&  mime != KAlarm::MIME_TEMPLATE)
            ||  !item.hasPayload<KAEvent>())
                return QVariant();
            switch (role)
            {
                case StatusRole:
                    // Mime type has a one-to-one relationship to event's category()
                    if (mime == KAlarm::MIME_ACTIVE)
                        return KAlarm::CalEvent::ACTIVE;
                    if (mime == KAlarm::MIME_ARCHIVED)
                        return KAlarm::CalEvent::ARCHIVED;
                    if (mime == KAlarm::MIME_TEMPLATE)
                        return KAlarm::CalEvent::TEMPLATE;
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
                return event.actions();
            if (role == AlarmActionRole)
                return event.action();
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
                                return alarmTimeText(event.startDateTime());
                            return alarmTimeText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER));
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
                            return timeToAlarmText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER));
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
                            if (event.action() == KAEvent::MESSAGE
                            ||  event.action() == KAEvent::FILE
                            ||  (event.action() == KAEvent::COMMAND && event.commandDisplay()))
                                return event.bgColour();
                            if (event.action() == KAEvent::COMMAND)
                            {
                                if (event.commandError() != KAEvent::CMD_NO_ERROR)
                                    return Qt::red;
                            }
                            break;
                        case Qt::ForegroundRole:
                            if (event.commandError() != KAEvent::CMD_NO_ERROR)
                            {
                                if (event.action() == KAEvent::COMMAND && !event.commandDisplay())
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
                            unsigned i = (event.action() == KAEvent::MESSAGE || event.action() == KAEvent::FILE)
                                       ? event.bgColour().rgb() : 0;
                            return QString("%1").arg(i, 6, 10, QLatin1Char('0'));
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
                            return static_cast<int>(event.action());
                        case SortRole:
                            return QString("%1").arg(event.action(), 2, 10, QLatin1Char('0'));
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
                            return AlarmText::summary(event);
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
        switch (role)
        {
            case Qt::BackgroundRole:
            {
                QColor colour = value.value<QColor>();
                CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
                if (attr->backgroundColor() == colour)
                    return true;   // no change
                attr->setBackgroundColor(colour);
                updateCollection = true;
                break;
            }
            case Qt::FontRole:
                // Set the font used in all views.
                // This enables data(index, Qt::FontRole) to return bold when appropriate.
                mFont = value.value<QFont>();
                return true;
            case EnabledRole:
            {
                KAlarm::CalEvent::Types types = static_cast<KAlarm::CalEvent::Types>(value.value<int>());
                CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
if (attr) { kDebug()<<"Set enabled:"<<types<<", was="<<attr->enabled(); } else { kDebug()<<"Set enabled:"<<types<<", no attribute"; }
                if (attr->enabled() == types)
                    return true;   // no change
                attr->setEnabled(types);
                updateCollection = true;
                break;
            }
            case IsStandardRole:
                if (collection.hasAttribute<CollectionAttribute>())
                {
                    KAlarm::CalEvent::Types types = static_cast<KAlarm::CalEvent::Types>(value.value<int>());
CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
kDebug()<<"Set standard:"<<types<<", was="<<attr->standard();
                    collection.attribute<CollectionAttribute>()->setStandard(types);
                    updateCollection = true;
                }
                break;
            default:
                break;
        }
        if (updateCollection)
        {
            CollectionModifyJob* job = new CollectionModifyJob(collection, this);
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
* Return the alarm time text in the form "date time".
*/
QString AkonadiModel::alarmTimeText(const DateTime& dateTime) const
{
    if (!dateTime.isValid())
        return i18nc("@info/plain Alarm never occurs", "Never");
    KLocale* locale = KGlobal::locale();
    KDateTime kdt = dateTime.effectiveKDateTime().toTimeSpec(Preferences::timeZone());
    QString dateTimeText = locale->formatDate(kdt.date(), KLocale::ShortDate);
    if (!dateTime.isDateOnly()
    ||  (!dateTime.isClockTime()  &&  kdt.utcOffset() != dateTime.utcOffset()))
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
QString AkonadiModel::timeToAlarmText(const DateTime& dateTime) const
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
* Recursive function to emit the dataChanged() signal for all items in a
* specified column range.
*/
void AkonadiModel::signalDataChanged(bool (*checkFunc)(const Item&), int startColumn, int endColumn, const QModelIndex& parent)
{
    int start = -1;
    int end;
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
{ return item.mimeType() == KAlarm::MIME_ACTIVE; }

void AkonadiModel::slotUpdateTimeTo()
{
    signalDataChanged(&checkItem_isActive, TimeToColumn, TimeToColumn, QModelIndex());
}


/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
static bool checkItem_isArchived(const Item& item)
{ return item.mimeType() == KAlarm::MIME_ARCHIVED; }

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
    return displayName_p(collection);
}

/******************************************************************************
* Return the display name for the collection.
*/
QString AkonadiModel::displayName_p(const Akonadi::Collection& collection) const
{
    QString name;
    if (collection.isValid()  &&  collection.hasAttribute<EntityDisplayAttribute>())
        name = collection.attribute<EntityDisplayAttribute>()->displayName();
    return name.isEmpty() ? collection.name() : name;
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
QString AkonadiModel::tooltip(const Collection& collection, KAlarm::CalEvent::Types types) const
{
    QString name = '@' + displayName_p(collection);   // insert markers for stripping out name
    KUrl url = collection.remoteId();
    QString type = '@' + storageType(collection);   // file/directory/URL etc.
    QString locn = url.pathOrUrl();
    bool inactive = !collection.hasAttribute<CollectionAttribute>()
                 || !(collection.attribute<CollectionAttribute>()->enabled() & types);
    bool writable = (collection.rights() & writableRights) == writableRights;
    QString disabled = i18nc("@info/plain", "Disabled");
    QString readonly = i18nc("@info/plain", "Read-only");
//if (!collection.hasAttribute<CollectionAttribute>()) { kDebug()<<"Tooltip: no collection attribute"; } else { kDebug()<<"Tooltip: enabled="<<collection.attribute<CollectionAttribute>()->enabled(); } //disabled="<<inactive;
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
    return QString("%1%2").arg(static_cast<char>('0' + repeatOrder)).arg(repeatInterval, 8, 10, QLatin1Char('0'));
}

/******************************************************************************
*  Return the icon associated with the event's action.
*/
QPixmap* AkonadiModel::eventIcon(const KAEvent& event) const
{
    switch (event.action())
    {
        case KAAlarm::FILE:
            return mFileIcon;
        case KAAlarm::EMAIL:
            return mEmailIcon;
        case KAAlarm::AUDIO:
            return mAudioIcon;
        case KAAlarm::COMMAND:
            if (!event.commandDisplay())
                return mCommandIcon;
            // fall through to MESSAGE
        case KAAlarm::MESSAGE:
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
* Add a new collection. The user will be prompted to enter its configuration.
*/
AgentInstanceCreateJob* AkonadiModel::addCollection(KAlarm::CalEvent::Type type, QWidget* parent)
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<AgentTypeDialog> dlg = new AgentTypeDialog(parent);
    QString mimeType;
    switch (type)
    {
        case KAlarm::CalEvent::ACTIVE:
            mimeType = KAlarm::MIME_ACTIVE;
            break;
        case KAlarm::CalEvent::ARCHIVED:
            mimeType = KAlarm::MIME_ARCHIVED;
            break;
        case KAlarm::CalEvent::TEMPLATE:
            mimeType = KAlarm::MIME_TEMPLATE;
            break;
        default:
            return 0;
    }
    dlg->agentFilterProxyModel()->addMimeTypeFilter(mimeType);
    dlg->agentFilterProxyModel()->addCapabilityFilter(QLatin1String("Resource"));
    if (dlg->exec() != QDialog::Accepted)
        return 0;
    const AgentType agentType = dlg->agentType();
    if (!agentType.isValid())
        return 0;
    AgentInstanceCreateJob* job = new AgentInstanceCreateJob(agentType, parent);
    job->configure(parent);    // cause the user to be prompted for configuration
    connect(job, SIGNAL(result(KJob*)), SLOT(addCollectionJobDone(KJob*)));
    job->start();
    return job;
}

/******************************************************************************
* Called when an agent creation job has completed.
* Checks for any error.
*/
void AkonadiModel::addCollectionJobDone(KJob* j)
{
    AgentInstanceCreateJob* job = static_cast<AgentInstanceCreateJob*>(j);
    if (j->error())
    {
        kError() << "Failed to create new calendar resource:" << j->errorString();
        KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)", i18nc("@info/plain", "Failed to create new calendar resource"), j->errorString()));
        emit collectionAdded(job, false);
    }
    else
        emit collectionAdded(job, true);
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
        KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
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
    if (j->error())
    {
        emit collectionModified(collection.id(), false);
        QString errMsg = i18nc("@info", "Failed to update calendar <resource>%1</resource>.", displayName(collection));
        kError() << errMsg << ":" << j->errorString();
        KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
    }
    else
        emit collectionModified(collection.id(), true);
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
KAEvent::List AkonadiModel::events(Akonadi::Collection& collection, KAlarm::CalEvent::Type type) const
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
void AkonadiModel::getChildEvents(const QModelIndex& parent, KAlarm::CalEvent::Type type, KAEvent::List& events) const
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

KAEvent AkonadiModel::event(const QModelIndex& index) const
{
    return event(index.data(ItemRole).value<Item>());
}

KAEvent AkonadiModel::event(Item::Id itemId) const
{
    QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return KAEvent();
    return event(ix);
}

KAEvent AkonadiModel::event(const Item& item) const
{
    if (!item.isValid()  ||  !item.hasPayload<KAEvent>())
        return KAEvent();
    return item.payload<KAEvent>();
}

#if 0
/******************************************************************************
* Add an event to the default or a user-selected Collection.
*/
AkonadiModel::Result AkonadiModel::addEvent(KAEvent* event, KAlarm::CalEvent::Type type, QWidget* promptParent, bool noPrompt)
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
*         false if at least one item creation failed to be scheduled.
*/
bool AkonadiModel::addEvents(const QList<KAEvent*>& events, Collection& collection)
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
    if (!setItemPayload(item, event, collection))
        return false;
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
    QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    Collection collection = ix.data(ParentCollectionRole).value<Collection>();
    Item item = ix.data(ItemRole).value<Item>();
kDebug()<<"item id="<<item.id()<<", revision="<<item.revision();
    if (!setItemPayload(item, newEvent, collection))
        return false;
//    setData(ix, QVariant::fromValue(item), ItemRole);
    queueItemModifyJob(item);
    return true;
}

/******************************************************************************
* Initialise an Item with an event.
* Note that the event is not updated with the Item ID.
* The event's 'updated' flag is cleared.
*/
bool AkonadiModel::setItemPayload(Item& item, KAEvent& event, const Collection& collection)
{
    QString mimetype;
    switch (event.category())
    {
        case KAlarm::CalEvent::ACTIVE:      mimetype = KAlarm::MIME_ACTIVE;    break;
        case KAlarm::CalEvent::ARCHIVED:    mimetype = KAlarm::MIME_ARCHIVED;  break;
        case KAlarm::CalEvent::TEMPLATE:    mimetype = KAlarm::MIME_TEMPLATE;  break;
        default:                            Q_ASSERT(0);  return false;
    }
    if (!collection.contentMimeTypes().contains(mimetype))
    {
        kWarning() << "Invalid mime type for Collection";
        return false;
    }
    item.setMimeType(mimetype);
    item.setPayload<KAEvent>(event);
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
        KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
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
            QSet<QByteArray> attrs;
            attrs += CollectionAttribute::name();
            slotCollectionChanged(collection, attrs);
        }
        else
        {
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
        emit eventsToBeRemoved(events);
}

/******************************************************************************
* Return a list of KAEvent/Collection pairs for a given range of rows.
*/
AkonadiModel::EventList AkonadiModel::eventList(const QModelIndex& parent, int start, int end)
{
    EventList events;
    for (int row = start;  row <= end;  ++row)
    {
        QModelIndex ix = index(row, 0, parent);
        KAEvent evnt = event(ix.data(ItemRole).value<Item>());
        if (evnt.isValid())
            events += Event(evnt, data(ix, ParentCollectionRole).value<Collection>());
    }
    return events;
}

/******************************************************************************
* Called when a monitored collection's properties or content have changed.
* Emits a signal if the writable property has changed.
*/
void AkonadiModel::slotCollectionChanged(const Collection& collection, const QSet<QByteArray>& attributeNames)
{
    Collection::Rights oldRights = mCollectionRights.value(collection.id(), Collection::AllRights);
    Collection::Rights newRights = collection.rights() & writableRights;
    if (newRights != oldRights)
    {
        mCollectionRights[collection.id()] = newRights;
        emit collectionStatusChanged(collection, ReadOnly, (newRights != writableRights));
    }

    if (attributeNames.contains(CollectionAttribute::name()))
    {
        static bool first = true;
kDebug()<<"COLLECTION ATTRIBUTE changed";
        KAlarm::CalEvent::Types oldEnabled = mCollectionEnabled.value(collection.id(), KAlarm::CalEvent::EMPTY);
        KAlarm::CalEvent::Types newEnabled = collection.hasAttribute<CollectionAttribute>() ? collection.attribute<CollectionAttribute>()->enabled() : KAlarm::CalEvent::EMPTY;
        if (first  ||  newEnabled != oldEnabled)
        {
            kDebug() << "enabled ->" << newEnabled;
            first = false;
            mCollectionEnabled[collection.id()] = newEnabled;
            emit collectionStatusChanged(collection, Enabled, static_cast<int>(newEnabled));
        }
    }
}

/******************************************************************************
* Called when a monitored collection is removed.
*/
void AkonadiModel::slotCollectionRemoved(const Collection& collection)
{
    kDebug() << collection.id();
    mCollectionRights.remove(collection.id());
    mCollectionsDeleting.removeAll(collection.id());
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
            mPendingEventChanges.enqueue(Event(evnt, data(index, ParentCollectionRole).value<Collection>()));
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

KAlarm::CalEvent::Types AkonadiModel::types(const Collection& collection)
{
    KAlarm::CalEvent::Types types = 0;
    QStringList mimeTypes = collection.contentMimeTypes();
    if (mimeTypes.contains(KAlarm::MIME_ACTIVE))
        types |= KAlarm::CalEvent::ACTIVE;
    if (mimeTypes.contains(KAlarm::MIME_ARCHIVED))
        types |= KAlarm::CalEvent::ARCHIVED;
    if (mimeTypes.contains(KAlarm::MIME_TEMPLATE))
        types |= KAlarm::CalEvent::TEMPLATE;
    return types;
}

/******************************************************************************
* Check whether the alarm types in a calendar correspond with a collection's
* mime types.
* Reply = true if at least 1 alarm is the right type.
*/
bool AkonadiModel::checkAlarmTypes(const Akonadi::Collection& collection, KCalCore::Calendar::Ptr& calendar)
{
    KAlarm::CalEvent::Types etypes = types(collection);
    if (etypes)
    {
        bool have = false;
        bool other = false;
        const KCalCore::Event::List events = calendar->rawEvents();
        for (int i = 0, iend = events.count();  i < iend;  ++i)
        {
            KAlarm::CalEvent::Type s = KAlarm::CalEvent::status(events[i]);
            if (etypes & s)
                have = true;
            else
                other = true;
            if (have && other)
                break;
        }
        if (!have  &&  other)
            return false;   // contains only wrong alarm types
    }
    return true;
}

// vim: et sw=4:
