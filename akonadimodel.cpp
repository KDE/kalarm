/*
 *  akonadimodel.cpp  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2007-2010 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "alarmtext.h"
#include "autoqpointer.h"
#include "collectionattribute.h"
#include "eventattribute.h"
#include "kaevent.h"
#include "preferences.h"
#include "synchtimer.h"

#include <akonadi/agentinstancecreatejob.h>
#include <akonadi/agentmanager.h>
#include <akonadi/agenttype.h>
#include <akonadi/attributefactory.h>
#include <akonadi/changerecorder.h>
#include <akonadi/collectiondialog.h>
#include <akonadi/collectiondeletejob.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/item.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemmodifyjob.h>
#include <akonadi/itemdeletejob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/session.h>
#include <kcalcore/memorycalendar.h>

#include <klocale.h>
#include <kmessagebox.h>

#include <QApplication>
#include <QFileInfo>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>
#include <QObject>

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
    : EntityTreeModel(monitor, parent)
{
    // Set lazy population to enable the contents of unselected collections to be ignored
    setItemPopulationStrategy(LazyPopulation);

    // Restrict monitoring to collections containing the KAlarm mime types
    monitor->setCollectionMonitored(Collection::root());
    monitor->setResourceMonitored("akonadi_kalarm_resource");
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
#ifdef __GNUC__
#warning When a calendar is disabled, its rows are not removed from the model, so no alarms deleted signal is emitted
#endif
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
                if (collection.hasAttribute<CollectionAttribute>())
                    return collection.attribute<CollectionAttribute>()->isEnabled();
                return false;
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
                if (!attr->isEnabled())
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
            {
                QString name = '@' + displayName_p(collection);   // insert markers for stripping out name
                KUrl url = collection.remoteId();
                QString type = '@' + storageType(collection);   // file/directory/URL etc.
                QString locn = url.pathOrUrl();
                bool inactive = !collection.hasAttribute<CollectionAttribute>()
                             || !collection.attribute<CollectionAttribute>()->isEnabled();
                bool writable = (collection.rights() & writableRights) == writableRights;
                QString disabled = i18nc("@info/plain", "Disabled");
                QString readonly = i18nc("@info/plain", "Read-only");
//if (!collection.hasAttribute<CollectionAttribute>()) { kDebug()<<"Tooltip: no collection attribute"; } else { kDebug()<<"Tooltip: enabled="<<collection.attribute<CollectionAttribute>()->isEnabled(); } //disabled="<<inactive;
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
            KAEvent event = this->event(item);
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
    Collection collection = index.data(CollectionRole).value<Collection>();
    if (collection.isValid())
    {
        // This is a Collection row
        bool updateCollection = false;
        switch (role)
        {
#ifdef __GNUC__
#warning Emit dataChanged() whenever any item is changed
#endif
            case Qt::BackgroundRole:
            {
                QColor colour = value.value<QColor>();
                CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
                if (attr->backgroundColor() == colour)
                    return true;
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
                bool enabled = value.toBool();
                CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
if (attr) { kDebug()<<"Set enabled:"<<enabled<<", was="<<attr->isEnabled(); } else { kDebug()<<"Set enabled:"<<enabled<<", no attribute"; }
                if (attr->isEnabled() == enabled)
                    return true;
                attr->setEnabled(enabled);
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
                case Qt::EditRole:
                {
#ifdef __GNUC__
#warning  ??? update event
#endif
                    int row = index.row();
                    emit dataChanged(this->index(row, 0, index.parent()), this->index(row, ColumnCount - 1, index.parent()));
                    return true;
                }
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
                                return true;
                            EventAttribute* attr = item.attribute<EventAttribute>(Entity::AddIfMissing);
                            if (attr->commandError() == err)
                                return true;
                            attr->setCommandError(err);
                            //updateItem = true;
//                            int row = index.row();
//                            emit dataChanged(this->index(row, 0, index.parent()), this->index(row, ColumnCount - 1, index.parent()));
                            break;
                        }
                        default:
                            return false;
                    }
                    break;
                }
                default:
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
* Recursive function to emit the dataChanged() signal for all items with a
* given mime type and in a specified column range.
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
        KAEvent event = item.payload<KAEvent>();
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
* Update the next trigger time for all alarms which are set to recur only on
* non-holidays.
*/
static bool checkItem_excludesHolidays(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.holidaysExcluded())
        {
            event.updateHolidays();
            return true;
        }
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
* Update the next trigger time for all alarms which are set to recur only
* during working hours.
*/
static bool checkItem_workTimeOnly(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.workTimeOnly())
        {
            event.updateWorkHours();
            return true;
        }
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
    QString resourceType;
    switch (type)
    {
        case KAlarm::CalEvent::ACTIVE:    resourceType = "akonadi_kalarm_active_resource";  break;
        case KAlarm::CalEvent::ARCHIVED:  resourceType = "akonadi_kalarm_archived_resource";  break;
        case KAlarm::CalEvent::TEMPLATE:  resourceType = "akonadi_kalarm_template_resource";  break;
        default:
            return 0;
    }
    AgentType agentType = AgentManager::self()->type(resourceType);
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
    {
#ifdef __GNUC__
#warning Can the collection be retrieved?
#endif
        //job->instance().identifier();
        emit collectionAdded(job, true);
    }
}

/******************************************************************************
* Remove a collection from Akonadi. The calendar file is not removed.
*/
bool AkonadiModel::removeCollection(const Akonadi::Collection& collection)
{
    if (!collection.isValid())
        return false;
    Collection col = collection;
    CollectionDeleteJob* job = new CollectionDeleteJob(col);
    connect(job, SIGNAL(result(KJob*)), SLOT(deleteCollectionJobDone(KJob*)));
    mPendingCollections[job] = CollJobData(col.id(), displayName(col));
    job->start();
    return true;
}

/******************************************************************************
* Called when a collection deletion job has completed.
* Checks for any error.
*/
void AkonadiModel::deleteCollectionJobDone(KJob* j)
{
    QMap<KJob*, CollJobData>::iterator it = mPendingCollections.find(j);
    CollJobData jobData;
    if (it != mPendingCollections.end())
    {
        jobData = it.value();
        mPendingCollections.erase(it);
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
                {
                    event.setItemId(item.id());
                    if (item.hasAttribute<EventAttribute>())
                    {
                        KAEvent::CmdErrType err = item.attribute<EventAttribute>()->commandError();
                        event.setCommandError(err, false);
                    }
                    events += event;
                }
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
    if (item.isValid()  &&  item.hasPayload<KAEvent>())
    {
        KAEvent event = item.payload<KAEvent>();
        if (event.isValid())
        {
            event.setItemId(item.id());
            if (item.hasAttribute<EventAttribute>())
            {
                KAEvent::CmdErrType err = item.attribute<EventAttribute>()->commandError();
                event.setCommandError(err);
            }
        }
        return event;
    }
    return KAEvent();
}

#if 0
/******************************************************************************
* Return all events in all calendars.
*/
QList<KAEvent*> AkonadiModel::events() const
{
    QList<KAEvent*> list;
    Collection::List collections = mMonitor->collectionsMonitored();
    foreach (const Collection& c, collections)
    {
        ???
    }
}

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
    Item item;
    if (!setItemPayload(item, event, collection))
        return false;
    event.setItemId(item.id());
    ItemCreateJob* job = new ItemCreateJob(item, collection);
    connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
    mPendingItems[job] = item.id();
    job->start();
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
    return updateEvent(event.itemId(), event);
}
bool AkonadiModel::updateEvent(Akonadi::Entity::Id itemId, KAEvent& newEvent)
{
    QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    Collection collection = ix.data(ParentCollectionRole).value<Collection>();
    Item item = ix.data(ItemRole).value<Item>();
    if (!setItemPayload(item, newEvent, collection))
        return false;
    setData(ix, QVariant::fromValue(item), ItemRole);
//    queueItemModifyJob(item);
    return true;
#ifdef __GNUC__
#warning Ensure KAlarm event list is updated correctly before and after Akonadi update
#endif
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
    event.clearUpdated();
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
    QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    Item item = ix.data(ItemRole).value<Item>();
    ItemDeleteJob* job = new ItemDeleteJob(item);
    connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
    mPendingItems[job] = itemId;
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
    QMap<Item::Id, Item::List>::Iterator it = mItemModifyJobQueue.find(item.id());
    if (it != mItemModifyJobQueue.end())
        it.value() << item;
    else
    {
        Item::List items;
        items << item;
        Item current = itemById(item.id());    // fetch the up-to-date item
        if (current.isValid())
            items[0].setRevision(current.revision());
        mItemModifyJobQueue[item.id()] = items;
        ItemModifyJob* job = new ItemModifyJob(items[0]);
        connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
        mPendingItems[job] = item.id();
//kDebug()<<"Modify job queued for item"<<item.id()<<", revision="<<items[0].revision();
    }
}

/******************************************************************************
* Called when an item job has completed.
* Checks for any error.
*/
void AkonadiModel::itemJobDone(KJob* j)
{
    QMap<KJob*, Entity::Id>::iterator it = mPendingItems.find(j);
    Entity::Id itemId = -1;
    if (it != mPendingItems.end())
    {
        itemId = it.value();
        mPendingItems.erase(it);
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
        KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)", errMsg, j->errorString()));
    }
    else
        emit itemDone(itemId);

    if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
    {
        // It was an ItemModifyJob: execute the next job in the queue if one is waiting
        QMap<Item::Id, Item::List>::iterator it = mItemModifyJobQueue.find(itemId);
        if (it != mItemModifyJobQueue.end())
        {
            Item::List& items = it.value();
            items.erase(items.begin());   // remove this job from the queue
            if (!items.isEmpty())
            {
                // Queue the next job for the Item, after updating the Item's
                // revision number to match that set by the job just completed.
                ItemModifyJob* job = static_cast<ItemModifyJob*>(j);
                items[0].setRevision(job->item().revision());
                job = new ItemModifyJob(items[0]);
                connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
                mPendingItems[job] = items[0].id();
//kDebug()<<"Modify job queued for item"<<items[0].id()<<", revision="<<items[0].revision();
            }
        }
    }
}

/******************************************************************************
* Called when rows have been inserted into the model.
*/
void AkonadiModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
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
#ifdef __GNUC__
#warning Ensure collection rights is initialised at startup
#endif
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
        bool oldEnabled = mCollectionEnabled.value(collection.id(), false);
        bool newEnabled = collection.hasAttribute<CollectionAttribute>() ? collection.attribute<CollectionAttribute>()->isEnabled() : false;
        if (first  ||  newEnabled != oldEnabled)
        {
            first = false;
            mCollectionEnabled[collection.id()] = newEnabled;
            emit collectionStatusChanged(collection, Enabled, newEnabled);
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
}

/******************************************************************************
* Called when an item in the monitored collections has changed.
*/
void AkonadiModel::slotMonitoredItemChanged(const Akonadi::Item& item, const QSet<QByteArray>&)
{
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


/*=============================================================================
= Class: CollectionMimeTypeFilterModel
= Proxy model to restrict its contents to Collections, not Items, containing
= specified content mime types.
=============================================================================*/
class CollectionMimeTypeFilterModel : public Akonadi::EntityMimeTypeFilterModel
{
        Q_OBJECT
    public:
        explicit CollectionMimeTypeFilterModel(QObject* parent = 0);
        void setEventTypeFilter(KAlarm::CalEvent::Type);
        void setFilterWritable(bool writable);
        void setFilterEnabled(bool enabled);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private:
        QString mMimeType;     // collection content type contained in this model
        bool    mWritableOnly; // only include writable collections in this model
        bool    mEnabledOnly;  // only include enabled collections in this model
};

CollectionMimeTypeFilterModel::CollectionMimeTypeFilterModel(QObject* parent)
    : EntityMimeTypeFilterModel(parent),
      mMimeType(),
      mWritableOnly(false)
{
    addMimeTypeInclusionFilter(Collection::mimeType());
    setHeaderGroup(EntityTreeModel::CollectionTreeHeaders);
    setSourceModel(AkonadiModel::instance());
}

void CollectionMimeTypeFilterModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    if (mimeType != mMimeType)
    {
        mMimeType = mimeType;
        invalidateFilter();
    }
}

void CollectionMimeTypeFilterModel::setFilterWritable(bool writable)
{
    if (writable != mWritableOnly)
    {
        mWritableOnly = writable;
        invalidateFilter();
    }
}

void CollectionMimeTypeFilterModel::setFilterEnabled(bool enabled)
{
    if (enabled != mEnabledOnly)
    {
        emit layoutAboutToBeChanged();
        mEnabledOnly = enabled;
        invalidateFilter();
        emit layoutChanged();
    }
}

bool CollectionMimeTypeFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EntityMimeTypeFilterModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (!mWritableOnly  &&  mMimeType.isEmpty())
        return true;
    AkonadiModel* model = AkonadiModel::instance();
    QModelIndex ix = model->index(sourceRow, 0, sourceParent);
    Collection collection = model->data(ix, AkonadiModel::CollectionRole).value<Collection>();
    if (mWritableOnly  &&  collection.rights() == Collection::ReadOnly)
        return false;
    if (!mMimeType.isEmpty()  &&  !collection.contentMimeTypes().contains(mMimeType))
        return false;
    if (mEnabledOnly)
    {
        if (!collection.hasAttribute<CollectionAttribute>()
        ||  !collection.attribute<CollectionAttribute>()->isEnabled())
            return false;
    }
    return true;
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionMimeTypeFilterModel::collection(int row) const
{
    return static_cast<AkonadiModel*>(sourceModel())->data(mapToSource(index(row, 0)), EntityTreeModel::CollectionRole).value<Collection>();
}

Collection CollectionMimeTypeFilterModel::collection(const QModelIndex& index) const
{
    return static_cast<AkonadiModel*>(sourceModel())->data(mapToSource(index), EntityTreeModel::CollectionRole).value<Collection>();
}


/*=============================================================================
= Class: CollectionListModel
= Proxy model converting the collection tree into a flat list.
= The model may be restricted to specified content mime types.
=============================================================================*/

CollectionListModel::CollectionListModel(QObject* parent)
    : KDescendantsProxyModel(parent)
{
    setSourceModel(new CollectionMimeTypeFilterModel(this));
    setDisplayAncestorData(false);
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionListModel::collection(int row) const
{
//    return AkonadiModel::instance()->data(mapToSource(index(row, 0)), EntityTreeModel::CollectionRole).value<Collection>();
    return data(index(row, 0), EntityTreeModel::CollectionRole).value<Collection>();
}

Collection CollectionListModel::collection(const QModelIndex& index) const
{
//    return AkonadiModel::instance()->data(mapToSource(index), EntityTreeModel::CollectionRole).value<Collection>();
    return data(index, EntityTreeModel::CollectionRole).value<Collection>();
}

void CollectionListModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
{
    static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->setEventTypeFilter(type);
}

void CollectionListModel::setFilterWritable(bool writable)
{
    static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->setFilterWritable(writable);
}

void CollectionListModel::setFilterEnabled(bool enabled)
{
    static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->setFilterEnabled(enabled);
}

bool CollectionListModel::isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const
{
    Q_UNUSED(descendant);
    return !ancestor.isValid();
}


/*=============================================================================
= Class: CollectionCheckListModel
= Proxy model providing a checkable collection list.
=============================================================================*/

CollectionCheckListModel* CollectionCheckListModel::mInstance = 0;

CollectionCheckListModel* CollectionCheckListModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionCheckListModel(qApp);
    return mInstance;
}

CollectionCheckListModel::CollectionCheckListModel(QObject* parent)
    : Future::KCheckableProxyModel(parent)
{
    CollectionListModel* model = new CollectionListModel(this);
    setSourceModel(model);
    mSelectionModel = new QItemSelectionModel(model);
    setSelectionModel(mSelectionModel);
    connect(mSelectionModel, SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                             SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));
    connect(model, SIGNAL(rowsInserted(const QModelIndex&, int, int)), SLOT(slotRowsInserted(const QModelIndex&, int, int)));
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionCheckListModel::collection(int row) const
{
    return static_cast<CollectionListModel*>(sourceModel())->collection(mapToSource(index(row, 0)));
}

Collection CollectionCheckListModel::collection(const QModelIndex& index) const
{
    return static_cast<CollectionListModel*>(sourceModel())->collection(mapToSource(index));
}

/******************************************************************************
* Set model data for one index.
* If the change is to disable a collection, check for eligibility and prevent
* the change if necessary.
*/
bool CollectionCheckListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole  &&  static_cast<Qt::CheckState>(value.toInt()) != Qt::Checked)
    {
        // A collection is to be disabled.
        const Collection collection = static_cast<CollectionListModel*>(sourceModel())->collection(index);
        if (collection.isValid()  &&  collection.hasAttribute<CollectionAttribute>())
        {
            const CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
            if (attr->isEnabled())
            {
                QString errmsg;
                QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
                if (attr->standard() != KAlarm::CalEvent::EMPTY)
                {
                    // It's the standard collection for some alarm type.
                    if (attr->isStandard(KAlarm::CalEvent::ACTIVE))
                    {
                        errmsg = i18nc("@info", "You cannot disable your default active alarm calendar.");
                    }
                    else if (attr->isStandard(KAlarm::CalEvent::ARCHIVED)  &&  Preferences::archivedKeepDays())
                    {
                        // Only allow the archived alarms standard collection to be disabled if
                        // we're not saving expired alarms.
                        errmsg = i18nc("@info", "You cannot disable your default archived alarm calendar "
                                                "while expired alarms are configured to be kept.");
                    }
                    else if (KMessageBox::warningContinueCancel(messageParent,
                                                           i18nc("@info", "Do you really want to disable your default calendar?"))
                               == KMessageBox::Cancel)
                        return false;
                }
                if (!errmsg.isEmpty())
                {
                    KMessageBox::sorry(messageParent, errmsg);
                    return false;
                }
            }
        }
    }
    return Future::KCheckableProxyModel::setData(index, value, role);
}

/******************************************************************************
* Called when rows have been inserted into the model.
* Select or deselect them according to their enabled status.
*/
void CollectionCheckListModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    CollectionListModel* model = static_cast<CollectionListModel*>(sourceModel());
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = mapToSource(index(row, 0, parent));
        const Collection collection = model->collection(ix);
        if (collection.isValid())
        {
            QItemSelectionModel::SelectionFlags sel = (collection.hasAttribute<CollectionAttribute>()
                                                       &&  collection.attribute<CollectionAttribute>()->isEnabled())
                                                    ? QItemSelectionModel::Select : QItemSelectionModel::Deselect;
            mSelectionModel->select(ix, sel);
        }
    }
}

/******************************************************************************
* Called when the user has ticked/unticked a collection to enable/disable it.
*/
void CollectionCheckListModel::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const QModelIndexList sel = selected.indexes();
    foreach (const QModelIndex& ix, sel)
        CollectionControlModel::setEnabled(static_cast<CollectionListModel*>(sourceModel())->collection(ix), true);
    const QModelIndexList desel = deselected.indexes();
    foreach (const QModelIndex& ix, desel)
        CollectionControlModel::setEnabled(static_cast<CollectionListModel*>(sourceModel())->collection(ix), false);
}


/*=============================================================================
= Class: CollectionFilterCheckListModel
= Proxy model providing a checkable collection list, filtered by mime type.
=============================================================================*/
CollectionFilterCheckListModel::CollectionFilterCheckListModel(QObject* parent)
    : QSortFilterProxyModel(parent),
      mMimeType()
{
    setSourceModel(CollectionCheckListModel::instance());
}

void CollectionFilterCheckListModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    if (mimeType != mMimeType)
    {
        mMimeType = mimeType;
        invalidateFilter();
    }
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionFilterCheckListModel::collection(int row) const
{
    return CollectionCheckListModel::instance()->collection(mapToSource(index(row, 0)));
}

Collection CollectionFilterCheckListModel::collection(const QModelIndex& index) const
{
    return CollectionCheckListModel::instance()->collection(mapToSource(index));
}

bool CollectionFilterCheckListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (mMimeType.isEmpty())
        return true;
    CollectionCheckListModel* model = CollectionCheckListModel::instance();
    const Collection collection = model->collection(model->index(sourceRow, 0, sourceParent));
    return collection.contentMimeTypes().contains(mMimeType);
}


/*=============================================================================
= Class: CollectionView
= View displaying a list of collections.
=============================================================================*/
CollectionView::CollectionView(CollectionFilterCheckListModel* model, QWidget* parent)
    : QListView(parent)
{
    setModel(model);
}

void CollectionView::setModel(QAbstractItemModel* model)
{
    model->setData(QModelIndex(), viewOptions().font, Qt::FontRole);
    QListView::setModel(model);
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionView::collection(int row) const
{
    return static_cast<CollectionFilterCheckListModel*>(model())->collection(row);
}

Collection CollectionView::collection(const QModelIndex& index) const
{
    return static_cast<CollectionFilterCheckListModel*>(model())->collection(index);
}

/******************************************************************************
* Called when a mouse button is released.
* Any currently selected collection is deselected.
*/
void CollectionView::mouseReleaseEvent(QMouseEvent* e)
{
    if (!indexAt(e->pos()).isValid())
        clearSelection();
    QListView::mouseReleaseEvent(e);
}

/******************************************************************************
* Called when a ToolTip or WhatsThis event occurs.
*/
bool CollectionView::viewportEvent(QEvent* e)
{
    if (e->type() == QEvent::ToolTip  &&  isActiveWindow())
    {
        QHelpEvent* he = static_cast<QHelpEvent*>(e);
        QModelIndex index = indexAt(he->pos());
        QVariant value = model()->data(index, Qt::ToolTipRole);
        if (qVariantCanConvert<QString>(value))
        {
            QString toolTip = value.toString();
            int i = toolTip.indexOf('@');
            if (i > 0)
            {
                int j = toolTip.indexOf(QRegExp("<(nl|br)", Qt::CaseInsensitive), i + 1);
                int k = toolTip.indexOf('@', j);
                QString name = toolTip.mid(i + 1, j - i - 1);
                value = model()->data(index, Qt::FontRole);
                QFontMetrics fm(qvariant_cast<QFont>(value).resolve(viewOptions().font));
                int textWidth = fm.boundingRect(name).width() + 1;
                const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
                QStyleOptionButton opt;
                opt.QStyleOption::operator=(viewOptions());
                opt.rect = rectForIndex(index);
                int checkWidth = QApplication::style()->subElementRect(QStyle::SE_ViewItemCheckIndicator, &opt).width();
                int left = spacing() + 3*margin + checkWidth + viewOptions().decorationSize.width();   // left offset of text
                int right = left + textWidth;
                if (left >= horizontalOffset() + spacing()
                &&  right <= horizontalOffset() + width() - spacing() - 2*frameWidth())
                {
                    // The whole of the collection name is already displayed,
                    // so omit it from the tooltip.
                    if (k > 0)
                        toolTip.remove(i, k + 1 - i);
                }
                else
                {
                    toolTip.remove(k, 1);
                    toolTip.remove(i, 1);
                }
            }
            QToolTip::showText(he->globalPos(), toolTip, this);
            return true;
        }
    }
    return QListView::viewportEvent(e);
}


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select which Collections will be enabled. Disabled Collections
= are not populated or monitored; their contents are ignored. The set of
= enabled Collections is stored in the config file's "Collections" group.
= Note that this model is not used directly for displaying - its purpose is to
= allow collections to be disabled, which will remove them from the other
= collection models.
=============================================================================*/

CollectionControlModel* CollectionControlModel::mInstance = 0;
bool                    CollectionControlModel::mAskDestination = false;

CollectionControlModel* CollectionControlModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionControlModel(qApp);
    return mInstance;
}

CollectionControlModel::CollectionControlModel(QObject* parent)
    : FavoriteCollectionsModel(AkonadiModel::instance(), KConfigGroup(KGlobal::config(), "Collections"), parent)
{
    // Initialise the list of enabled collections
    EntityMimeTypeFilterModel* filter = new EntityMimeTypeFilterModel(this);
    filter->addMimeTypeInclusionFilter(Collection::mimeType());
    filter->setSourceModel(AkonadiModel::instance());
    Collection::List collections;
    findEnabledCollections(filter, QModelIndex(), collections);
    setCollections(collections);

    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool)),
                                      SLOT(statusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool)));
}

/******************************************************************************
* Recursive function to check all collections' enabled status.
*/
void CollectionControlModel::findEnabledCollections(const EntityMimeTypeFilterModel* filter, const QModelIndex& parent, Collection::List& collections) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = filter->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = filter->index(row, 0, parent);
        const Collection collection = model->data(filter->mapToSource(ix), AkonadiModel::CollectionRole).value<Collection>();
        if (collection.hasAttribute<CollectionAttribute>()
        &&  collection.attribute<CollectionAttribute>()->isEnabled())
            collections += collection;
        if (filter->rowCount(ix) > 0)
            findEnabledCollections(filter, ix, collections);
    }
}

bool CollectionControlModel::isEnabled(const Collection& collection)
{
    return collection.isValid()  &&  instance()->collections().contains(collection);
}

void CollectionControlModel::setEnabled(const Collection& collection, bool enabled)
{
    instance()->statusChanged(collection, AkonadiModel::Enabled, enabled);
}

void CollectionControlModel::statusChanged(const Collection& collection, AkonadiModel::Change change, bool value)
{
    if (change == AkonadiModel::Enabled)
    {
        if (collection.isValid())
        {
            if (value)
            {
                const Collection::List cols = collections();
                foreach (const Collection& c, cols)
                {
                    if (c.id() == collection.id())
                        return;
                }
                addCollection(collection);
            }
            else
                removeCollection(collection);
            AkonadiModel* model = static_cast<AkonadiModel*>(sourceModel());
            model->setData(model->collectionIndex(collection), value, AkonadiModel::EnabledRole);
        }
#if 0
        QModelIndex ix = modelIndexForCollection(this, collection);
        if (ix.isValid())
            selectionModel()->select(mapFromSource(ix), (enabled ? QItemSelectionModel::Select : QItemSelectionModel::Deselect));
#endif
    }
}

/******************************************************************************
* Return whether a collection is both enabled and fully writable.
* Optionally, the enabled status can be ignored.
*/
bool CollectionControlModel::isWritable(const Akonadi::Collection& collection, bool ignoreEnabledStatus)
{
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if (!col.hasAttribute<CollectionAttribute>()
    ||  col.attribute<CollectionAttribute>()->compatibility() != KAlarm::Calendar::Current)
        return false;
    return (ignoreEnabledStatus || isEnabled(col))
       &&  (col.rights() & writableRights) == writableRights;
}

/******************************************************************************
* Return the standard collection for a specified mime type.
*/
Collection CollectionControlModel::getStandard(KAlarm::CalEvent::Type type)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    Collection::List cols = instance()->collections();
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        AkonadiModel::instance()->refresh(cols[i]);    // update with latest data
        if (cols[i].isValid()
        &&  cols[i].contentMimeTypes().contains(mimeType)
        &&  cols[i].hasAttribute<CollectionAttribute>()
        &&  (cols[i].attribute<CollectionAttribute>()->standard() & type))
            return cols[i];
    }
    return Collection();
}

/******************************************************************************
* Return whether a collection is the standard collection for a specified
* mime type.
*/
bool CollectionControlModel::isStandard(Akonadi::Collection& collection, KAlarm::CalEvent::Type type)
{
    if (!instance()->collections().contains(collection))
        return false;
    AkonadiModel::instance()->refresh(collection);    // update with latest data
    if (!collection.hasAttribute<CollectionAttribute>())
        return false;
    return collection.attribute<CollectionAttribute>()->isStandard(type);
}

/******************************************************************************
* Return the alarm type(s) for which a collection is the standard collection.
*/
KAlarm::CalEvent::Types CollectionControlModel::standardTypes(const Collection& collection)
{
    if (!instance()->collections().contains(collection))
        return KAlarm::CalEvent::EMPTY;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if (!col.hasAttribute<CollectionAttribute>())
        return KAlarm::CalEvent::EMPTY;
    return col.attribute<CollectionAttribute>()->standard();
}

/******************************************************************************
* Set or clear a collection as the standard collection for a specified mime
* type. If it is being set as standard, the standard status for the mime type
* is cleared for all other collections.
*/
void CollectionControlModel::setStandard(Akonadi::Collection& collection, KAlarm::CalEvent::Type type, bool standard)
{
    AkonadiModel* model = AkonadiModel::instance();
    model->refresh(collection);    // update with latest data
    if (standard)
    {
        // The collection is being set as standard.
        // Clear the 'standard' status for all other collections.
        Collection::List cols = instance()->collections();
        if (!cols.contains(collection))
            return;
        KAlarm::CalEvent::Types ctypes = collection.hasAttribute<CollectionAttribute>()
                                 ? collection.attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
        if (ctypes & type)
            return;    // it's already the standard collection for this type
        for (int i = 0, count = cols.count();  i < count;  ++i)
        {
            KAlarm::CalEvent::Types types;
            if (cols[i] == collection)
            {
                cols[i] = collection;    // update with latest data
                types = ctypes | type;
            }
            else
            {
                model->refresh(cols[i]);    // update with latest data
                types = cols[i].hasAttribute<CollectionAttribute>()
                      ? cols[i].attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
                if (!(types & type))
                    continue;
                types &= ~type;
            }
            QModelIndex index = model->collectionIndex(cols[i]);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        KAlarm::CalEvent::Types types = collection.hasAttribute<CollectionAttribute>()
                                ? collection.attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
        if (types & type)
        {
            types &= ~type;
            QModelIndex index = model->collectionIndex(collection);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
}

/******************************************************************************
* Set which mime types a collection is the standard collection for.
* If it is being set as standard for any mime types, the standard status for
* those mime types is cleared for all other collections.
*/
void CollectionControlModel::setStandard(Akonadi::Collection& collection, KAlarm::CalEvent::Types types)
{
    AkonadiModel* model = AkonadiModel::instance();
    model->refresh(collection);    // update with latest data
    if (types)
    {
        // The collection is being set as standard for at least one mime type.
        // Clear the 'standard' status for all other collections.
        Collection::List cols = instance()->collections();
        if (!cols.contains(collection))
            return;
        KAlarm::CalEvent::Types t = collection.hasAttribute<CollectionAttribute>()
                            ? collection.attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
        if (t == types)
            return;    // there's no change to the collection's status
        for (int i = 0, count = cols.count();  i < count;  ++i)
        {
            KAlarm::CalEvent::Types t;
            if (cols[i] == collection)
            {
                cols[i] = collection;    // update with latest data
                t = types;
            }
            else
            {
                model->refresh(cols[i]);    // update with latest data
                t = cols[i].hasAttribute<CollectionAttribute>()
                  ? cols[i].attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
                if (!(t & types))
                    continue;
                t &= ~types;
            }
            QModelIndex index = model->collectionIndex(cols[i]);
            model->setData(index, static_cast<int>(t), AkonadiModel::IsStandardRole);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        if (collection.hasAttribute<CollectionAttribute>()
        &&  collection.attribute<CollectionAttribute>()->standard())
        {
            QModelIndex index = model->collectionIndex(collection);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
}

/******************************************************************************
* Get the collection to use for storing an alarm.
* Optionally, the standard collection for the alarm type is returned. If more
* than one collection is a candidate, the user is prompted.
*/
Collection CollectionControlModel::destination(KAlarm::CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Collection standard;
    if (type == KAlarm::CalEvent::EMPTY)
        return standard;
    standard = getStandard(type);
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == KAlarm::CalEvent::ARCHIVED  ||  noPrompt  ||  (!mAskDestination  &&  standard.isValid()))
        return standard;

    // Prompt for which collection to use
    CollectionListModel* model = new CollectionListModel(promptParent);
    model->setFilterWritable(true);
    model->setFilterEnabled(true);
    model->setEventTypeFilter(type);
    Collection col;
    switch (model->rowCount())
    {
        case 0:
            break;
        case 1:
            col = model->collection(0);
            break;
        default:
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the dialogue is still open. It prevents double deletion (both on
            // deletion of 'promptParent', and on return from this function).
            AutoQPointer<CollectionDialog> dlg = new CollectionDialog(model, promptParent);
            dlg->setCaption(i18nc("@title:window", "Choose Calendar"));
            dlg->setDefaultCollection(standard);
            dlg->setMimeTypeFilter(QStringList(KAlarm::CalEvent::mimeType(type)));
            if (dlg->exec())
                col = dlg->selectedCollection();
            if (!col.isValid()  &&  cancelled)
                *cancelled = true;
        }
    }
    return col;
}

/******************************************************************************
* Return the enabled collections which contain a specified mime type.
* If 'writable' is true, only writable collections are included.
*/
Collection::List CollectionControlModel::enabledCollections(KAlarm::CalEvent::Type type, bool writable)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    Collection::List cols = instance()->collections();
    Collection::List result;
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        AkonadiModel::instance()->refresh(cols[i]);    // update with latest data
        if (cols[i].contentMimeTypes().contains(mimeType)
        &&  (!writable || ((cols[i].rights() & writableRights) == writableRights)))
            result += cols[i];
    }
    return result;
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant CollectionControlModel::data(const QModelIndex& index, int role) const
{
    return sourceModel()->data(mapToSource(index), role);
}


/*=============================================================================
= Class: ItemListModel
= Filter proxy model containing all items (alarms/templates) of specified mime
= types in enabled collections.
=============================================================================*/
ItemListModel::ItemListModel(KAlarm::CalEvent::Types allowed, QObject* parent)
    : EntityMimeTypeFilterModel(parent),
      mAllowedTypes(allowed),
      mHaveEvents(false)
{
    KSelectionProxyModel* selectionModel = new KSelectionProxyModel(CollectionControlModel::instance()->selectionModel(), this);
    selectionModel->setSourceModel(AkonadiModel::instance());
    selectionModel->setFilterBehavior(KSelectionProxyModel::ChildrenOfExactSelection);
    setSourceModel(selectionModel);

    addMimeTypeExclusionFilter(Collection::mimeType());
    setHeaderGroup(EntityTreeModel::ItemListHeaders);
    if (allowed)
    {
        QStringList mimeTypes = KAlarm::CalEvent::mimeTypes(allowed);
        foreach (const QString& mime, mimeTypes)
            addMimeTypeInclusionFilter(mime);
    }
    setHeaderGroup(EntityTreeModel::ItemListHeaders);
    setSortRole(AkonadiModel::SortRole);
    setDynamicSortFilter(true);
    connect(this, SIGNAL(rowsInserted(const QModelIndex&, int, int)), SLOT(slotRowsInserted()));
    connect(this, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)), SLOT(slotRowsToBeRemoved()));
}

int ItemListModel::columnCount(const QModelIndex& parent) const
{
#if 0
    if (parent.isValid())
        return 0;
#endif
    return AkonadiModel::ColumnCount;
}

/******************************************************************************
* Called when rows have been inserted into the model.
*/
void ItemListModel::slotRowsInserted()
{
    if (!mHaveEvents  &&  rowCount())
    {
        mHaveEvents = true;
        emit haveEventsStatus(true);
    }
}

/******************************************************************************
* Called when rows have been deleted from the model.
*/
void ItemListModel::slotRowsToBeRemoved()
{
    if (mHaveEvents  &&  !rowCount())
    {
        mHaveEvents = false;
        emit haveEventsStatus(false);
    }
}

#if 0
QModelIndex ItemListModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid())
        return QModelIndex();
    return createIndex(row, column, mEvents[row]);
}

bool ItemListModel::setData(const QModelIndex& ix, const QVariant&, int role)
{
    if (ix.isValid()  &&  role == Qt::EditRole)
    {
//??? update event
        int row = ix.row();
        emit dataChanged(index(row, 0), index(row, AkonadiModel::ColumnCount - 1));
        return true;
    }
    return false;
}
#endif

Qt::ItemFlags ItemListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex ItemListModel::eventIndex(Entity::Id itemId) const
{
    QModelIndexList list = match(QModelIndex(), AkonadiModel::ItemIdRole, itemId, 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (list.isEmpty())
        return QModelIndex();
    return index(list[0].row(), 0, list[0].parent());
}

/******************************************************************************
* Return the event in a specified row.
*/
KAEvent ItemListModel::event(int row) const
{
    return event(index(row, 0));
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent ItemListModel::event(const QModelIndex& index) const
{
    return static_cast<AkonadiModel*>(sourceModel())->event(mapToSource(index));
}

/******************************************************************************
* Check whether the model contains any events.
*/
bool ItemListModel::haveEvents() const
{
    return rowCount();
}


/*=============================================================================
= Class: AlarmListModel
= Filter proxy model containing all alarms of specified mime types in enabled
= collections.
Equivalent to AlarmListFilterModel
=============================================================================*/
AlarmListModel* AlarmListModel::mAllInstance = 0;

AlarmListModel::AlarmListModel(QObject* parent)
    : ItemListModel(KAlarm::CalEvent::ACTIVE | KAlarm::CalEvent::ARCHIVED, parent),
      mFilterTypes(KAlarm::CalEvent::ACTIVE | KAlarm::CalEvent::ARCHIVED)
{
}

AlarmListModel::~AlarmListModel()
{
    if (this == mAllInstance)
        mAllInstance = 0;
}

AlarmListModel* AlarmListModel::all()
{
    if (!mAllInstance)
    {
        mAllInstance = new AlarmListModel(AkonadiModel::instance());
        mAllInstance->sort(TimeColumn, Qt::AscendingOrder);
    }
    return mAllInstance;
}

void AlarmListModel::setEventTypeFilter(KAlarm::CalEvent::Types types)
{
    // Ensure that the filter isn't applied to the 'all' instance, and that
    // 'types' doesn't include any disallowed alarm types
    if (!types)
        types = includedTypes();
    if (this != mAllInstance
    &&  types != mFilterTypes  &&  (types & includedTypes()) == types)
    {
        mFilterTypes = types;
        invalidateFilter();
    }
}

bool AlarmListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!ItemListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mFilterTypes == KAlarm::CalEvent::EMPTY)
        return false;
    int type = sourceModel()->data(sourceModel()->index(sourceRow, 0, sourceParent), AkonadiModel::StatusRole).toInt();
    return static_cast<KAlarm::CalEvent::Type>(type) & mFilterTypes;
}

bool AlarmListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return (sourceCol != AkonadiModel::TemplateNameColumn);
}

QVariant AlarmListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (section < 0  ||  section >= ColumnCount)
            return QVariant();
    }
    return ItemListModel::headerData(section, orientation, role);
}


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates for specified alarm types
= in enabled collections.
Equivalent to TemplateListFilterModel
=============================================================================*/
TemplateListModel* TemplateListModel::mAllInstance = 0;

TemplateListModel::TemplateListModel(QObject* parent)
    : ItemListModel(KAlarm::CalEvent::TEMPLATE, parent),
      mActionsEnabled(KAEvent::ACT_ALL),
      mActionsFilter(KAEvent::ACT_ALL)
{
}

TemplateListModel::~TemplateListModel()
{
    if (this == mAllInstance)
        mAllInstance = 0;
}

TemplateListModel* TemplateListModel::all()
{
    if (!mAllInstance)
    {
        mAllInstance = new TemplateListModel(AkonadiModel::instance());
        mAllInstance->sort(TemplateNameColumn, Qt::AscendingOrder);
    }
    return mAllInstance;
}

void TemplateListModel::setAlarmActionFilter(KAEvent::Actions types)
{
    // Ensure that the filter isn't applied to the 'all' instance.
    if (this != mAllInstance  &&  types != mActionsFilter)
    {
        mActionsFilter = types;
        filterChanged();
    }
}

void TemplateListModel::setAlarmActionsEnabled(KAEvent::Actions types)
{
    // Ensure that the setting isn't applied to the 'all' instance.
    if (this != mAllInstance  &&  types != mActionsEnabled)
    {
        mActionsEnabled = types;
        filterChanged();
    }
}

bool TemplateListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!ItemListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mActionsFilter == KAEvent::ACT_ALL)
        return true;
    QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    KAEvent::Actions actions = static_cast<KAEvent::Actions>(sourceModel()->data(sourceIndex, AkonadiModel::AlarmActionsRole).toInt());
    return actions & mActionsFilter;
}

bool TemplateListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return sourceCol == AkonadiModel::TemplateNameColumn
       ||  sourceCol == AkonadiModel::TypeColumn;
}

QVariant TemplateListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case TypeColumn:
                section = AkonadiModel::TypeColumn;
                break;
            case TemplateNameColumn:
                section = AkonadiModel::TemplateNameColumn;
                break;
            default:
                return QVariant();
        }
    }
    return ItemListModel::headerData(section, orientation, role);
}

Qt::ItemFlags TemplateListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = sourceModel()->flags(mapToSource(index));
    if (mActionsEnabled == KAEvent::ACT_ALL)
        return f;
    KAEvent::Actions actions = static_cast<KAEvent::Actions>(ItemListModel::data(index, AkonadiModel::AlarmActionsRole).toInt());
    if (!(actions & mActionsEnabled))
        f = static_cast<Qt::ItemFlags>(f & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    return f;
}

#include "akonadimodel.moc"
#include "moc_akonadimodel.cpp"

// vim: et sw=4:
