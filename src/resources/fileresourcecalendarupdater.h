/*
 *  fileresourcecalendarupdater.h  -  updates a file resource calendar to current KAlarm format
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

#ifndef FILERESOURCECALENDARUPDATER_H
#define FILERESOURCECALENDARUPDATER_H

#include "calendarupdater.h"


// Updates the backend calendar format of a single file resource alarm calendar
class FileResourceCalendarUpdater : public CalendarUpdater
{
    Q_OBJECT
public:
    FileResourceCalendarUpdater(Resource&, bool ignoreKeepFormat, QObject* parent, QWidget* promptParent = nullptr);

    /** If an existing resource calendar can be converted to the current KAlarm
     *  format, prompt the user whether to convert it, and if yes, tell the resource
     *  to update the backend storage to the current format.
     *  The resource's KeepFormat property will be updated if the user chooses not to
     *  update the calendar.
     *  This method should call update() on a single shot timer to prompt the
     *  user and convert the calendar.
     *  @param  parent  Parent object. If possible, this should be a QWidget.
     */
    static void updateToCurrentFormat(Resource&, bool ignoreKeepFormat, QObject* parent);

    /** If the calendar is not in the current KAlarm format, prompt the user
     *  whether to convert to the current format, and then perform the conversion.
     *  This method calls deleteLater() on completion.
     *  @return  false if the calendar is not in current format and the user
     *           chose not to update it; true otherwise.
     */
    bool update() override;

private Q_SLOTS:
    bool update(bool useTimer);
    bool prompt();

private:
    Resource mResource;
    QString  mPromptMessage;
};

#endif // FILERESOURCECALENDARUPDATER_H

// vim: et sw=4:
