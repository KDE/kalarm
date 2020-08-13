/*
 *  akonadicalendarupdater.h  -  updates a calendar to current KAlarm format
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AKONADICALENDARUPDATER_H
#define AKONADICALENDARUPDATER_H

#include "calendarupdater.h"

#include <AkonadiCore/Collection>

namespace Akonadi { class AgentInstance; }


// Updates the backend calendar format of a single alarm calendar
class AkonadiCalendarUpdater : public CalendarUpdater
{
    Q_OBJECT
public:
    AkonadiCalendarUpdater(const Akonadi::Collection& collection, bool dirResource,
                    bool ignoreKeepFormat, bool newCollection, QObject* parent, QWidget* promptParent = nullptr);

    /** If an existing resource calendar can be converted to the current KAlarm
     *  format, prompt the user whether to convert it, and if yes, tell the resource
     *  to update the backend storage to the current format.
     *  The resource's KeepFormat property will be updated if the user chooses not to
     *  update the calendar.
     *  This method should call update() on a single shot timer to prompt the
     *  user and convert the calendar.
     *  @param  parent  Parent object. If possible, this should be a QWidget.
     */
    static void updateToCurrentFormat(const Resource&, bool ignoreKeepFormat, QObject* parent);

public Q_SLOTS:
    /** If the calendar is not in the current KAlarm format, prompt the user
     *  whether to convert to the current format, and then perform the conversion.
     *  This method calls deleteLater() on completion.
     *  @return  false if the calendar is not in current format and the user
     *           chose not to update it; true otherwise.
     */
    bool update() override;

private:
    template <class Interface> static bool updateStorageFormat(const Akonadi::AgentInstance&, QString& errorMessage, QObject* parent);

    Akonadi::Collection mCollection;
    const bool          mDirResource;
    const bool          mNewCollection;
};

#endif // AKONADICALENDARUPDATER_H

// vim: et sw=4:
