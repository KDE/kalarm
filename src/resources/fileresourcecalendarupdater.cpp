/*
 *  fileresourcecalendarupdater.cpp  -  updates a file resource calendar to current KAlarm format
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileresourcecalendarupdater.h"

#include "resources.h"
#include "fileresource.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/kacalendar.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QTimer>
#include <kwidgetsaddons_version.h>

using namespace KAlarmCal;


/*=============================================================================
= Class to prompt the user to update the storage format for a resource, if it
= currently uses an old KAlarm storage format.
=============================================================================*/

FileResourceCalendarUpdater::FileResourceCalendarUpdater(Resource& resource, bool ignoreKeepFormat, QObject* parent, QWidget* promptParent)
    : CalendarUpdater(resource.id(), ignoreKeepFormat, parent, promptParent)
{
}

/******************************************************************************
* If an existing resource calendar can be converted to the current KAlarm
* format, prompt the user whether to convert it, and if yes, tell the resource
* to update the backend storage to the current format.
* The resource's KeepFormat property will be updated if the user chooses not to
* update the calendar.
*/
void FileResourceCalendarUpdater::updateToCurrentFormat(Resource& resource, bool ignoreKeepFormat, QObject* parent)
{
    qCDebug(KALARM_LOG) << "FileResourceCalendarUpdater::updateToCurrentFormat:" << resource.displayId();

    if (containsResource(resource.id()))
        return;   // prevent multiple simultaneous user prompts
    auto updater = new FileResourceCalendarUpdater(resource, ignoreKeepFormat, parent, qobject_cast<QWidget*>(parent));
    updater->update(true);
}

/******************************************************************************
* If the calendar is not in the current KAlarm format, prompt the user whether
* to convert to the current format, and then perform the conversion.
*/
bool FileResourceCalendarUpdater::update()
{
    return update(false);
}

/******************************************************************************
* If the calendar is not in the current KAlarm format, prompt the user whether
* to convert to the current format, and then perform the conversion.
*/
bool FileResourceCalendarUpdater::update(bool useTimer)
{
    mResource = Resources::resource(mResourceId);
    if (!mResource.isValid()  ||  mResource.readOnly()  ||  !mResource.is<FileResource>())
        ;
    else if (mDuplicate)
        qCDebug(KALARM_LOG) << "FileResourceCalendarUpdater::update: Not updating (concurrent update in progress)";
    else
    {
        QString versionString;
        const KACalendar::Compat compatibility = mResource.compatibilityVersion(versionString);
        qCDebug(KALARM_LOG) << "FileResourceCalendarUpdater::update:" << mResource.displayId() << "current format:" << compatibility;
        if ((compatibility & ~KACalendar::Converted)
        // The calendar isn't in the current KAlarm format
        &&  !(compatibility & ~(KACalendar::Convertible | KACalendar::Converted)))
        {
            // The calendar format is convertible to the current KAlarm format
            if (!mIgnoreKeepFormat  &&  mResource.keepFormat())
                qCDebug(KALARM_LOG) << "FileResourceCalendarUpdater::update: Not updating format (previous user choice)";
            else
            {
                qCDebug(KALARM_LOG) << "FileResourceCalendarUpdater::update: Version" << versionString;
                mPromptMessage = conversionPrompt(mResource.displayName(), versionString, false);
                if (useTimer)
                {
                    QTimer::singleShot(0, this, &FileResourceCalendarUpdater::prompt);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
                    return true;
                }
                return prompt();
            }
        }
    }
    setCompleted();
    return true;
}

bool FileResourceCalendarUpdater::prompt()
{
    bool result = true;
    const bool convert = (KAMessageBox::warningYesNo(mPromptParent, mPromptMessage) == KMessageBox::ButtonCode::PrimaryAction);
    if (!convert)
        result = false;
    if (convert)
    {
        // The user chose to update the calendar.
        // Tell the resource to update the backend storage format.
        if (!FileResource::updateStorageFormat(mResource))
        {
            Resources::notifyResourceMessage(mResource.id(), ResourceType::MessageType::Error,
                                             xi18nc("@info", "Failed to update format of calendar <resource>%1</resource>", mResource.displayName()),
                                             QString());
        }
    }
    // Record the user's choice of whether to update the calendar
    mResource.setKeepFormat(!convert);
    setCompleted();
    return result;
}

// vim: et sw=4:
