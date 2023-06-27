/*
 *  resourcedatamodelbase.cpp  -  base for models containing calendars and events
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resourcedatamodelbase.h"

#include "resources.h"
#include "lib/desktop.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/alarmtext.h"
#include "kalarmcalendar/kaevent.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QApplication>
#include <QIcon>
#include <QRegularExpression>


/*=============================================================================
= Class: ResourceDataModelBase
=============================================================================*/

ResourceDataModelBase* ResourceDataModelBase::mInstance = nullptr;

QPixmap* ResourceDataModelBase::mTextIcon    = nullptr;
QPixmap* ResourceDataModelBase::mFileIcon    = nullptr;
QPixmap* ResourceDataModelBase::mCommandIcon = nullptr;
QPixmap* ResourceDataModelBase::mEmailIcon   = nullptr;
QPixmap* ResourceDataModelBase::mAudioIcon   = nullptr;
QSize    ResourceDataModelBase::mIconSize;

/******************************************************************************
* Constructor.
*/
ResourceDataModelBase::ResourceDataModelBase()
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

ResourceDataModelBase::~ResourceDataModelBase()
{
}

/******************************************************************************
* Create a bulleted list of alarm types for insertion into <para>...</para>.
*/
QString ResourceDataModelBase::typeListForDisplay(CalEvent::Types alarmTypes)
{
    QString list;
    if (alarmTypes & CalEvent::ACTIVE)
        list += QLatin1String("<item>") + i18nc("@item:intext", "Active Alarms") + QLatin1String("</item>");
    if (alarmTypes & CalEvent::ARCHIVED)
        list += QLatin1String("<item>") + i18nc("@item:intext", "Archived Alarms") + QLatin1String("</item>");
    if (alarmTypes & CalEvent::TEMPLATE)
        list += QLatin1String("<item>") + i18nc("@item:intext", "Alarm Templates") + QLatin1String("</item>");
    if (!list.isEmpty())
        list = QLatin1String("<list>") + list + QLatin1String("</list>");
    return list;
}

/******************************************************************************
* Return the read-only status tooltip for a collection, determined by the
* read-write permissions and the KAlarm calendar format compatibility.
* A null string is returned if the collection is read-write and compatible.
*/
QString ResourceDataModelBase::readOnlyTooltip(const Resource& resource)
{
    switch (resource.compatibility())
    {
        case KACalendar::Current:
            return resource.readOnly() ? i18nc("@item:intext Calendar status", "Read-only") : QString();
        case KACalendar::Converted:
        case KACalendar::Convertible:
            return i18nc("@item:intext Calendar status", "Read-only (old format)");
        case KACalendar::Incompatible:
        default:
            return i18nc("@item:intext Calendar status", "Read-only (other format)");
    }
}

/******************************************************************************
* Return data for a column heading.
*/
QVariant ResourceDataModelBase::headerData(int section, Qt::Orientation orientation, int role, bool eventHeaders, bool& handled)
{
    if (orientation == Qt::Horizontal)
    {
        handled = true;
        if (eventHeaders)
        {
            // Event column headers
            if (section < 0  ||  section >= ColumnCount)
                return {};
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
                    case NameColumn:
                        return i18nc("@title:column", "Name");
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
                return {};
            if (role == Qt::DisplayRole)
                return i18nc("@title:column", "Calendars");
        }
    }

    handled = false;
    return {};
}

/******************************************************************************
* Return whether resourceData() or eventData() handle a role.
*/
bool ResourceDataModelBase::roleHandled(int role) const
{
    switch (role)
    {
        case Qt::WhatsThisRole:
        case Qt::ForegroundRole:
        case Qt::BackgroundRole:
        case Qt::DisplayRole:
        case Qt::TextAlignmentRole:
        case Qt::DecorationRole:
        case Qt::SizeHintRole:
        case Qt::AccessibleTextRole:
        case Qt::ToolTipRole:
        case ItemTypeRole:
        case ResourceIdRole:
        case BaseColourRole:
        case TimeDisplayRole:
        case SortRole:
        case StatusRole:
        case ValueRole:
        case EventIdRole:
        case ParentResourceIdRole:
        case EnabledRole:
        case AlarmActionsRole:
        case AlarmSubActionRole:
        case CommandErrorRole:
            return true;
        default:
            return false;
    }
}

/******************************************************************************
* Return the data for a given role, for a specified resource.
*/
QVariant ResourceDataModelBase::resourceData(int& role, const Resource& resource, bool& handled) const
{
    if (roleHandled(role))   // Ensure that roleHandled() is coded correctly
    {
        handled = true;
        switch (role)
        {
            case Qt::DisplayRole:
                return resource.displayName();
            case BaseColourRole:
                role = Qt::BackgroundRole;   // use base model background colour
                break;
            case Qt::BackgroundRole:
            {
                const QColor colour = resource.backgroundColour();
                if (colour.isValid())
                    return colour;
                break;    // use base model background colour
            }
            case Qt::ForegroundRole:
                return resource.foregroundColour();
            case Qt::ToolTipRole:
                return tooltip(resource, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
            case ItemTypeRole:
                return static_cast<int>(Type::Resource);
            case ResourceIdRole:
                return resource.id();
            default:
                break;
        }
    }

    handled = false;
    return {};
}

/******************************************************************************
* Return the data for a given role, for a specified event.
*/
QVariant ResourceDataModelBase::eventData(int role, int column, const KAEvent& event,
                                          const Resource& resource, bool& handled) const
{
    if (roleHandled(role))   // Ensure that roleHandled() is coded correctly
    {
        handled = true;
        bool calendarColour = false;

        switch (role)
        {
            case Qt::WhatsThisRole:
                return whatsThisText(column);
            case ItemTypeRole:
                return static_cast<int>(Type::Event);
            default:
                break;
        }
        if (!event.isValid())
            return {};
        switch (role)
        {
            case EventIdRole:
                return event.id();
            case StatusRole:
                return event.category();
            case AlarmActionsRole:
                return event.actionTypes();
            case AlarmSubActionRole:
                return event.actionSubType();
            case CommandErrorRole:
                return event.commandError();
            default:
                break;
        }
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
                            return alarmTimeText(event.startDateTime(), '0');
                        return alarmTimeText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER), '0');
                    case TimeDisplayRole:
                        if (event.expired())
                            return alarmTimeText(event.startDateTime(), '~');
                        return alarmTimeText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER), '~');
                    case Qt::TextAlignmentRole:
                        return Qt::AlignLeft;
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
                        return timeToAlarmText(event.nextTrigger(KAEvent::DISPLAY_TRIGGER));
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
                    case Qt::TextAlignmentRole:
                        return Qt::AlignCenter;
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
            case NameColumn:
            case TemplateNameColumn:
                switch (role)
                {
                    case Qt::BackgroundRole:
                        calendarColour = true;
                        break;
                    case Qt::DisplayRole:
                    case Qt::ToolTipRole:
                        return event.name();
                    case SortRole:
                        return event.name().toUpper();
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
            default:
                break;
        }

        if (calendarColour)
        {
            const QColor colour = resource.backgroundColour();
            if (colour.isValid())
                return colour;
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
                        // Return empty string to cancel any previous tooltip -
                        // returning QVariant() leaves tooltip unchanged.
                        return QString();
                }
                break;
            case EnabledRole:
                return event.enabled();
            default:
                break;
        }
    }

    handled = false;
    return {};
}

/******************************************************************************
* Return a resource's tooltip text. The resource's enabled status is
* evaluated for specified alarm types.
*/
QString ResourceDataModelBase::tooltip(const Resource& resource, CalEvent::Types types) const
{
    const QString name     = QLatin1Char('@') + resource.displayName();   // insert markers for stripping out name
    const QString type     = QLatin1Char('@') + resource.storageTypeString(false);   // file/directory/URL etc.
    const QString locn     = resource.displayLocation();
    const bool    inactive = !(resource.enabledTypes() & types);
    const QString readonly = readOnlyTooltip(resource);
    const bool    writable = readonly.isEmpty();
    const QString disabled = i18nc("@item:intext Calendar status", "Disabled");
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
* Return the repetition text.
*/
QString ResourceDataModelBase::repeatText(const KAEvent& event)
{
    const QString repText = event.recurrenceText(true);
    return repText.isEmpty() ? event.repetitionText(true) : repText;
}

/******************************************************************************
* Return a string for sorting the repetition column.
*/
QString ResourceDataModelBase::repeatOrder(const KAEvent& event)
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
QString ResourceDataModelBase::whatsThisText(int column)
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
        case NameColumn:
            return i18nc("@info:whatsthis", "Alarm name, or alarm text if name is blank");
        case TextColumn:
            return i18nc("@info:whatsthis", "Alarm message text, URL of text file to display, command to execute, or email subject line");
        case TemplateNameColumn:
            return i18nc("@info:whatsthis", "Name of the alarm template");
        default:
            return {};
    }
}

/******************************************************************************
* Return the icon associated with an event's action.
*/
QPixmap* ResourceDataModelBase::eventIcon(const KAEvent& event)
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

/******************************************************************************
* Display a message to the user.
*/
void ResourceDataModelBase::handleResourceMessage(ResourceType::MessageType type, const QString& message, const QString& details)
{
    if (type == ResourceType::MessageType::Error)
    {
        qCDebug(KALARM_LOG) << "Resource Error!" << message << details;
        KAMessageBox::detailedError(Desktop::mainWindow(), message, details);
    }
    else if (type == ResourceType::MessageType::Info)
    {
        qCDebug(KALARM_LOG) << "Resource user message:" << message << details;
        // KMessageBox::informationList looks bad, so use our own formatting.
        const QString msg = details.isEmpty() ? message : message + QStringLiteral("\n\n") + details;
        KAMessageBox::information(Desktop::mainWindow(), msg);
    }
}

bool ResourceDataModelBase::isMigrationComplete() const
{
    return mMigrationStatus == 1;
}

bool ResourceDataModelBase::isMigrating() const
{
    return mMigrationStatus == 0;
}

void ResourceDataModelBase::setMigrationInitiated(bool started)
{
    mMigrationStatus = (started ? 0 : -1);
}

void ResourceDataModelBase::setMigrationComplete()
{
    mMigrationStatus = 1;
    if (mCreationStatus)
        Resources::notifyResourcesCreated();
}

void ResourceDataModelBase::setCalendarsCreated()
{
    mCreationStatus = true;
    if (mMigrationStatus == 1)
        Resources::notifyResourcesCreated();
}

/******************************************************************************
* Return the alarm time text in the form "date time".
* Parameters:
*   dateTime    = the date/time to format.
*   leadingZero = the character to represent a leading zero, or '\0' for no leading zeroes.
*/
QString ResourceDataModelBase::alarmTimeText(const DateTime& dateTime, char leadingZero)
{
    // Whether the date and time contain leading zeroes.
    static bool    leadingZeroesChecked = false;
    static QString timeFormat;      // time format for current locale
    static QString timeFullFormat;  // time format with leading zero, if different from 'timeFormat'
    static int     hourOffset = 0;  // offset within time string to the hour

    if (!dateTime.isValid())
        return i18nc("@info Alarm never occurs", "Never");
    QLocale locale;
    if (!leadingZeroesChecked)
    {
        // Check whether the day number and/or hour have no leading zeroes, if
        // they are at the start of the date/time. If no leading zeroes, they
        // will need to be padded when displayed, so that displayed dates/times
        // can be aligned with each other.
        // Note that if leading zeroes are not included in other components, no
        // alignment will be attempted.

        // Check the time format.
        // Remove all but hours, minutes and AM/PM, since alarms are on minute
        // boundaries. Preceding separators are also removed.
        timeFormat = locale.timeFormat(QLocale::ShortFormat);
        for (int del = 0, predel = 0, c = 0;  c < timeFormat.size();  ++c)
        {
            char ch = timeFormat.at(c).toLatin1();
            switch (ch)
            {
                case 'H':
                case 'h':
                case 'm':
                case 'a':
                case 'A':
                    if (predel == 1)
                    {
                        timeFormat.remove(del, c - del);
                        c = del;
                    }
                    del = c + 1;   // start deleting from the next character
                    if ((ch == 'A'  &&  del < timeFormat.size()  &&  timeFormat.at(del).toLatin1() == 'P')
                    ||  (ch == 'a'  &&  del < timeFormat.size()  &&  timeFormat.at(del).toLatin1() == 'p'))
                        ++c, ++del;
                    predel = -1;
                    break;

                case 's':
                case 'z':
                case 't':
                    timeFormat.remove(del, c + 1 - del);
                    c = del - 1;
                    if (!predel)
                        predel = 1;
                    break;

                default:
                    break;
            }
        }

        // 'HH' and 'hh' provide leading zeroes; single 'H' or 'h' provide no
        // leading zeroes.
        static const QRegularExpression hourReg(QStringLiteral("[hH]"));
        int i = timeFormat.indexOf(hourReg);
        static const QRegularExpression hourMinAmPmReg(QStringLiteral("[hHmaA]"));
        int first = timeFormat.indexOf(hourMinAmPmReg);
        if (i >= 0  &&  i == first  &&  (i == timeFormat.size() - 1  ||  timeFormat.at(i) != timeFormat.at(i + 1)))
        {
            timeFullFormat = timeFormat;
            timeFullFormat.insert(i, timeFormat.at(i));
            // Find index to hour in formatted times
            const QTime t(1,30,30);
            const QString nozero = locale.toString(t, timeFormat);
            const QString zero   = locale.toString(t, timeFullFormat);
            for (int j = 0; j < nozero.size(); ++j)
                if (nozero[j] != zero[j])
                {
                    hourOffset = j;
                    break;
                }
        }

        leadingZeroesChecked = true;
    }

    const KADateTime kdt = dateTime.effectiveKDateTime().toTimeSpec(Preferences::timeSpec());
    QString dateTimeText = locale.toString(kdt.date(), QLocale::ShortFormat);

    if (!dateTime.isDateOnly()  ||  kdt.utcOffset() != dateTime.utcOffset())
    {
        // Display the time of day if it's a date/time value, or if it's
        // a date-only value but it's in a different time zone
        dateTimeText += QLatin1Char(' ');
        // Don't try to align right-to-left languages...
        const bool useFullFormat = QApplication::isLeftToRight() && leadingZero && !timeFullFormat.isEmpty();
        QString timeText = locale.toString(kdt.time(), (useFullFormat ? timeFullFormat : timeFormat));
        if (useFullFormat  &&  leadingZero != '0'  &&  timeText.at(hourOffset) == QLatin1Char('0'))
            timeText[hourOffset] = QChar::fromLatin1(leadingZero);
        dateTimeText += timeText;
    }
    return dateTimeText + QLatin1Char(' ');
}

/******************************************************************************
* Return the time-to-alarm text.
*/
QString ResourceDataModelBase::timeToAlarmText(const DateTime& dateTime)
{
    if (!dateTime.isValid())
        return i18nc("@info Alarm never occurs", "Never");
    const KADateTime now = KADateTime::currentUtcDateTime();
    if (dateTime.isDateOnly())
    {
        const int days = now.date().daysTo(dateTime.date());
        // xgettext: no-c-format
        return i18nc("@info n days", "%1d", days);
    }
    const int mins = (now.secsTo(dateTime.effectiveKDateTime()) + 59) / 60;
    if (mins <= 0)
        return {};
    QLocale locale;
    QString minutes = locale.toString(mins % 60);
    if (minutes.size() == 1)
        minutes.prepend(locale.zeroDigit());
    if (mins < 24*60)
        return i18nc("@info hours:minutes", "%1:%2", mins/60, minutes);
    // If we render a day count, then we zero-pad the hours, to make the days line up and be more scanable.
    const int hrs = mins / 60;
    QString hours = locale.toString(hrs % 24);
    if (hours.size() == 1)
        hours.prepend(locale.zeroDigit());
    const QString days = locale.toString(hrs / 24);
    return i18nc("@info days hours:minutes", "%1d %2:%3", days, hours, minutes);
}

// vim: et sw=4:
