/*
 *  calendarfunctions.cpp  -  miscellaneous calendar access functions
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#include "calendarfunctions.h"

#include "preferences.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KCalendarCore/CalFormat>
#include <KCalendarCore/Event>
#include <KCalendarCore/MemoryCalendar>
using namespace KCalendarCore;

#include <KLocalizedString>
#include <KJobWidgets>
#include <KIO/StoredTransferJob>

#include <QTemporaryFile>


namespace KAlarm
{

/******************************************************************************
* Import alarms from a calendar file. The alarms are converted to the current
* KAlarm format and are given new unique event IDs.
* Parameters: parent:    parent widget for error message boxes
*             alarmList: imported alarms are appended to this list
*/
bool importCalendarFile(const QUrl& url, CalEvent::Types alarmTypes, bool newId, QWidget* parent, QHash<CalEvent::Type, QVector<KAEvent>>& alarmList)
{
    if (!url.isValid())
    {
        qCDebug(KALARM_LOG) << "KAlarm::importCalendarFile: Invalid URL";
        return false;
    }

    // If the URL is remote, download it into a temporary local file.
    QString filename;
    bool local = url.isLocalFile();
    if (local)
    {
        filename = url.toLocalFile();
        if (!QFile::exists(filename))
        {
            qCDebug(KALARM_LOG) << "KAlarm::importCalendarFile:" << url.toDisplayString() << "not found";
            KAMessageBox::error(parent, xi18nc("@info", "Could not load calendar <filename>%1</filename>.", url.toDisplayString()));
            return false;
        }
    }
    else
    {
        auto getJob = KIO::storedGet(url);
        KJobWidgets::setWindow(getJob, parent);
        if (!getJob->exec())
        {
            qCCritical(KALARM_LOG) << "KAlarm::importCalendarFile: Download failure";
            KAMessageBox::error(parent, xi18nc("@info", "Cannot download calendar: <filename>%1</filename>", url.toDisplayString()));
            return false;
        }
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        tmpFile.write(getJob->data());
        tmpFile.seek(0);
        filename = tmpFile.fileName();
        qCDebug(KALARM_LOG) << "KAlarm::importCalendarFile: --- Downloaded to" << filename;
    }

    // Read the calendar and add its alarms to the current calendars
    MemoryCalendar::Ptr cal(new MemoryCalendar(Preferences::timeSpecAsZone()));
    FileStorage::Ptr calStorage(new FileStorage(cal, filename));
    bool success = calStorage->load();
    if (!local)
        QFile::remove(filename);
    if (!success)
    {
        qCDebug(KALARM_LOG) << "KAlarm::importCalendarFile: Error loading calendar '" << filename <<"'";
        KAMessageBox::error(parent, xi18nc("@info", "Could not load calendar <filename>%1</filename>.", url.toDisplayString()));
        return false;
    }

    QString versionString;
    const bool currentFormat = (KACalendar::updateVersion(calStorage, versionString) != KACalendar::IncompatibleFormat);
    const Event::List events = cal->rawEvents();
    for (Event::Ptr event : events)
    {
        if (event->alarms().isEmpty()  ||  !KAEvent(event).isValid())
            continue;    // ignore events without alarms, or usable alarms
        CalEvent::Type type = CalEvent::status(event);
        if (type == CalEvent::TEMPLATE)
        {
            // If we know the event was not created by KAlarm, don't treat it as a template
            if (!currentFormat)
                type = CalEvent::ACTIVE;
        }
        if (!(type & alarmTypes))
            continue;

        Event::Ptr newev(new Event(*event));

        // If there is a display alarm without display text, use the event
        // summary text instead.
        if (type == CalEvent::ACTIVE  &&  !newev->summary().isEmpty())
        {
            const Alarm::List& alarms = newev->alarms();
            for (Alarm::Ptr alarm : alarms)
            {
                if (alarm->type() == Alarm::Display  &&  alarm->text().isEmpty())
                    alarm->setText(newev->summary());
            }
            newev->setSummary(QString());   // KAlarm only uses summary for template names
        }

        // Give the event a new ID, or ensure that it is in the correct format.
        const QString id = newId ? CalFormat::createUniqueId() : newev->uid();
        newev->setUid(CalEvent::uid(id, type));

        alarmList[type] += KAEvent(newev);
    }
    return true;
}

}

// vim: et sw=4:
