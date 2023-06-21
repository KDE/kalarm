/*
 *  calendarupdater.cpp  -  base class to update a calendar to current KAlarm format
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "calendarupdater.h"

#include "lib/desktop.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QThread>


/*=============================================================================
= Class to prompt the user to update the storage format for a resource, if it
= currently uses an old KAlarm storage format.
=============================================================================*/

QList<CalendarUpdater*> CalendarUpdater::mInstances;

CalendarUpdater::CalendarUpdater(ResourceId resourceId, bool ignoreKeepFormat, QObject* parent, QWidget* promptParent)
    : QObject(parent)
    , mResourceId(resourceId)
    , mParent(parent)
    , mPromptParent(promptParent ? promptParent : Desktop::mainWindow())
    , mIgnoreKeepFormat(ignoreKeepFormat)
    , mDuplicate(containsResource(resourceId))
{
    mInstances.append(this);
}

CalendarUpdater::~CalendarUpdater()
{
    mInstances.removeAll(this);
}

bool CalendarUpdater::containsResource(ResourceId id)
{
    for (CalendarUpdater* instance : std::as_const(mInstances))
    {
        if (instance->mResourceId == id)
            return true;
    }
    return false;
}

/******************************************************************************
* Wait until all instances have completed and been deleted.
*/
void CalendarUpdater::waitForCompletion()
{
    while (!mInstances.isEmpty())
    {
        for (int i = mInstances.count();  --i >= 0;  )
            if (mInstances.at(i)->isComplete())
                delete mInstances.at(i);    // the destructor removes the instance from mInstances

        QCoreApplication::processEvents();
        if (!mInstances.isEmpty())     //cppcheck-suppress knownConditionTrueFalse; 'delete' removes from mInstances
            QThread::msleep(100);
    }
}

/******************************************************************************
* Mark the instance as completed, and schedule its deletion.
*/
void CalendarUpdater::setCompleted()
{
    mCompleted = true;
    deleteLater();
}

/******************************************************************************
* Return a prompt string to ask the user whether to convert the calendar to the
* current format.
*/
QString CalendarUpdater::conversionPrompt(const QString& calendarName, const QString& calendarVersion, bool whole)
{
    const QString msg = whole
                ? xi18nc("@info", "Calendar <resource>%1</resource> is in an old format (<application>KAlarm</application> version %2), "
                        "and will be read-only unless you choose to update it to the current format.",
                        calendarName, calendarVersion)
                : xi18nc("@info", "Some or all of the alarms in calendar <resource>%1</resource> are in an old <application>KAlarm</application> format, "
                        "and will be read-only unless you choose to update them to the current format.",
                        calendarName);
    return xi18nc("@info", "<para>%1</para><para>"
                 "<warning>Do not update the calendar if it is also used with an older version of <application>KAlarm</application> "
                 "(e.g. on another computer). If you do so, the calendar may become unusable there.</warning></para>"
                 "<para>Do you wish to update the calendar?</para>", msg);
}

// vim: et sw=4:

#include "moc_calendarupdater.cpp"
