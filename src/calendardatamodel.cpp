/*
 *  calendardatamodel.cpp  -  base for models containing calendars and events
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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

#include "calendardatamodel.h"
#include "alarmtime.h"
#include "preferences.h"

#include <kalarmcal/alarmtext.h>
#include <kalarmcal/kaevent.h>

#include <KLocalizedString>
#include <kcolorutils.h>

#include <QModelIndex>
#include <QUrl>
#include <QFileInfo>
#include <QIcon>
#include "kalarm_debug.h"

/*=============================================================================
= Class: CalendarDataModel
=============================================================================*/

QPixmap* CalendarDataModel::mTextIcon    = nullptr;
QPixmap* CalendarDataModel::mFileIcon    = nullptr;
QPixmap* CalendarDataModel::mCommandIcon = nullptr;
QPixmap* CalendarDataModel::mEmailIcon   = nullptr;
QPixmap* CalendarDataModel::mAudioIcon   = nullptr;
QSize    CalendarDataModel::mIconSize;

/******************************************************************************
* Constructor.
*/
CalendarDataModel::CalendarDataModel()
{
    if (!mTextIcon)
    {
        mTextIcon    = new QPixmap(QIcon::fromTheme(QStringLiteral("dialog-information")).pixmap(16, 16));
        mFileIcon    = new QPixmap(QIcon::fromTheme(QStringLiteral("document-open")).pixmap(16, 16));
        mCommandIcon = new QPixmap(QIcon::fromTheme(QStringLiteral("system-run")).pixmap(16, 16));
        mEmailIcon   = new QPixmap(QIcon::fromTheme(QStringLiteral("mail-unread")).pixmap(16, 16));
        mAudioIcon   = new QPixmap(QIcon::fromTheme(QStringLiteral("audio-x-generic")).pixmap(16, 16));
        mIconSize = mTextIcon->size().expandedTo(mFileIcon->size()).expandedTo(mCommandIcon->size()).expandedTo(mEmailIcon->size()).expandedTo(mAudioIcon->size());
    }
}

CalendarDataModel::~CalendarDataModel()
{
}

/******************************************************************************
* Return data for a column heading.
*/
QVariant CalendarDataModel::headerData(int section, Qt::Orientation orientation, int role, bool eventHeaders, bool& handled)
{
    if (orientation == Qt::Horizontal)
    {
        handled = true;
        if (eventHeaders)
        {
            // Event column headers
            if (section < 0  ||  section >= ColumnCount)
                return QVariant();
            if (role == Qt::DisplayRole  ||  role == ColumnTitleRole)
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
                        return (role == Qt::DisplayRole) ? QString() : i18nc("@title:column", "Color");
                    case TypeColumn:
                        return (role == Qt::DisplayRole) ? QString() : i18nc("@title:column", "Type");
                    case TextColumn:
                        return i18nc("@title:column", "Message, File or Command");
                    case TemplateNameColumn:
                        return i18nc("@title:column Template name", "Name");
                }
            }
            else if (role == Qt::WhatsThisRole)
                return whatsThisText(section);
        }
        else
        {
            // Calendar column headers
            if (section != 0)
                return QVariant();
            if (role == Qt::DisplayRole)
                return i18nc("@title:column", "Calendars");
        }
    }

    handled = false;
    return QVariant();
}

/******************************************************************************
* Return the data for a given role, for a specified event.
* @param calendarColour  updated to true if the calendar's background colour
*                        should be returned by the caller, else set to false.
* @param handled         updated to true if the reply is valid, else set to false.
*/
QVariant CalendarDataModel::eventData(const QModelIndex& ix, int role, const KAEvent& event, bool& calendarColour, bool& handled) const
{
    handled = true;
    calendarColour = false;

    const int column = ix.column();
    if (role == Qt::WhatsThisRole)
        return whatsThisText(column);
    if (!event.isValid())
        return QVariant();
    if (role == AlarmActionsRole)
        return event.actionTypes();
    if (role == AlarmSubActionRole)
        return event.actionSubType();
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
                        return AlarmTime::alarmTimeText(event.startDateTime(), '0');
                    return AlarmTime::alarmTimeText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER), '0');
                case TimeDisplayRole:
                    if (event.expired())
                        return AlarmTime::alarmTimeText(event.startDateTime(), '~');
                    return AlarmTime::alarmTimeText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER), '~');
                case Qt::TextAlignmentRole:
                    return Qt::AlignRight;
                case SortRole:
                {
                    DateTime due;
                    if (event.expired())
                        due = event.startDateTime();
                    else
                        due = event.nextTrigger(KAEvent::DISPLAY_TRIGGER);
                    return due.isValid() ? due.effectiveKDateTime().toUtc().qDateTime()
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
                case Qt::TextAlignmentRole:
                    return Qt::AlignRight;
                case SortRole:
                {
                    if (event.expired())
                        return -1;
                    const DateTime due = event.nextTrigger(KAEvent::DISPLAY_TRIGGER);
                    const KADateTime now = KADateTime::currentUtcDateTime();
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
                    const KAEvent::Actions type = event.actionTypes();
                    if (type & KAEvent::ACT_DISPLAY)
                        return event.bgColour();
                    if (type == KAEvent::ACT_COMMAND)
                    {
                        if (event.commandError() != KAEvent::CMD_NO_ERROR)
                            return QColor(Qt::red);
                    }
                    break;
                }
                case Qt::ForegroundRole:
                    if (event.commandError() != KAEvent::CMD_NO_ERROR)
                    {
                        if (event.actionTypes() == KAEvent::ACT_COMMAND)
                            return QColor(Qt::white);
                        QColor colour = Qt::red;
                        int r, g, b;
                        event.bgColour().getRgb(&r, &g, &b);
                        if (r > 128  &&  g <= 128  &&  b <= 128)
                            colour = QColor(Qt::white);
                        return colour;
                    }
                    break;
                case Qt::DisplayRole:
                    if (event.commandError() != KAEvent::CMD_NO_ERROR)
                        return QLatin1String("!");
                    break;
                case SortRole:
                {
                    const unsigned i = (event.actionTypes() == KAEvent::ACT_DISPLAY)
                                       ? event.bgColour().rgb() : 0;
                    return QStringLiteral("%1").arg(i, 6, 10, QLatin1Char('0'));
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
//TODO: Implement accessibility
                    return QString();
                case ValueRole:
                    return static_cast<int>(event.actionSubType());
                case SortRole:
                    return QStringLiteral("%1").arg(event.actionSubType(), 2, 10, QLatin1Char('0'));
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

    // Use the base class's data value, unless calendar colour is to be used.
    handled = calendarColour;
    return QVariant();
}

/******************************************************************************
* Return the foreground color for displaying a collection, based on the
* supplied mime types which it contains, and on whether it is fully writable.
*/
QColor CalendarDataModel::foregroundColor(CalEvent::Type alarmType, bool readOnly)
{
    QColor colour;
    switch (alarmType)
    {
        case CalEvent::ACTIVE:
            colour = KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText).color();
            break;
        case CalEvent::ARCHIVED:
            colour = Preferences::archivedColour();
            break;
        case CalEvent::TEMPLATE:
            colour = KColorScheme(QPalette::Active).foreground(KColorScheme::LinkText).color();
            break;
        default:
            break;
    }
    if (colour.isValid()  &&  readOnly)
        return KColorUtils::lighten(colour, 0.2);
    return colour;
}

/******************************************************************************
* Return the storage type (file, directory, etc.) for the collection.
*/
QString CalendarDataModel::storageTypeForLocation(const QString& location) const
{
    const QUrl url = QUrl::fromUserInput(location, QString(), QUrl::AssumeLocalFile);
    return !url.isLocalFile()                     ? i18nc("@info", "URL")
           : QFileInfo(url.toLocalFile()).isDir() ? i18nc("@info Directory in filesystem", "Directory")
           :                                        i18nc("@info", "File");
}

/******************************************************************************
* Return a collection's tooltip text. The collection's enabled status is
* evaluated for specified alarm types.
*/
QString CalendarDataModel::tooltip(bool writable, bool inactive, const QString& name, const QString& type,
                                   const QString& locn, const QString& disabled, const QString& readonly)
{
    if (inactive  &&  !writable)
        return xi18nc("@info:tooltip",
                     "%1"
                     "<nl/>%2: <filename>%3</filename>"
                     "<nl/>%4, %5",
                     name, type, locn, disabled, readonly);
    if (inactive  ||  !writable)
        return xi18nc("@info:tooltip",
                     "%1"
                     "<nl/>%2: <filename>%3</filename>"
                     "<nl/>%4",
                     name, type, locn, (inactive ? disabled : readonly));
    return xi18nc("@info:tooltip",
                 "%1"
                 "<nl/>%2: <filename>%3</filename>",
                 name, type, locn);
}

/******************************************************************************
* Return the read-only status tooltip for a collection.
* A null string is returned if the collection is fully writable.
*/
QString CalendarDataModel::readOnlyTooltip(KACalendar::Compat compat, int writable)
{
    switch (writable)
    {
        case 1:
            return QString();
        case 0:
            return i18nc("@info", "Read-only (old format)");
        default:
            if (compat == KACalendar::Current)
                return i18nc("@info", "Read-only");
            return i18nc("@info", "Read-only (other format)");
    }
}

/******************************************************************************
* Return the repetition text.
*/
QString CalendarDataModel::repeatText(const KAEvent& event)
{
    const QString repText = event.recurrenceText(true);
    return repText.isEmpty() ? event.repetitionText(true) : repText;
}

/******************************************************************************
* Return a string for sorting the repetition column.
*/
QString CalendarDataModel::repeatOrder(const KAEvent& event)
{
    int repOrder = 0;
    int repInterval = 0;
    if (event.repeatAtLogin())
        repOrder = 1;
    else
    {
        repInterval = event.recurInterval();
        switch (event.recurType())
        {
            case KARecurrence::MINUTELY:
                repOrder = 2;
                break;
            case KARecurrence::DAILY:
                repOrder = 3;
                break;
            case KARecurrence::WEEKLY:
                repOrder = 4;
                break;
            case KARecurrence::MONTHLY_DAY:
            case KARecurrence::MONTHLY_POS:
                repOrder = 5;
                break;
            case KARecurrence::ANNUAL_DATE:
            case KARecurrence::ANNUAL_POS:
                repOrder = 6;
                break;
            case KARecurrence::NO_RECUR:
            default:
                break;
        }
    }
    return QStringLiteral("%1%2").arg(static_cast<char>('0' + repOrder)).arg(repInterval, 8, 10, QLatin1Char('0'));
}

/******************************************************************************
* Returns the QWhatsThis text for a specified column.
*/
QString CalendarDataModel::whatsThisText(int column)
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
* Return the icon associated with an event's action.
*/
QPixmap* CalendarDataModel::eventIcon(const KAEvent& event)
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
            Q_FALLTHROUGH();    // fall through to ACT_DISPLAY_COMMAND
        case KAEvent::ACT_DISPLAY_COMMAND:
        default:
            return mTextIcon;
    }
}

// vim: et sw=4:
