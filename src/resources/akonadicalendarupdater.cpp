/*
 *  akonadicalendarupdater.cpp  -  updates a calendar to current KAlarm format
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "akonadicalendarupdater.h"

#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "resources/akonadidatamodel.h"
#include "resources/akonadiresource.h"
#include "resources/resources.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KAlarmCal/CollectionAttribute>
#include <KAlarmCal/CompatibilityAttribute>
#include <KAlarmCal/Version>

#include <AkonadiCore/AgentManager>

#include <KLocalizedString>

#include <QTimer>

using namespace Akonadi;
using namespace KAlarmCal;


AkonadiCalendarUpdater::AkonadiCalendarUpdater(const Collection& collection, bool dirResource,
                                               bool ignoreKeepFormat, bool newCollection,
                                               QObject* parent, QWidget* promptParent)
    : CalendarUpdater(collection.id(), ignoreKeepFormat, parent, promptParent)
    , mCollection(collection)
    , mDirResource(dirResource)
    , mNewCollection(newCollection)
{
}

/******************************************************************************
* If an existing Akonadi resource calendar can be converted to the current
* KAlarm format, prompt the user whether to convert it, and if yes, tell the
* Akonadi resource to update the backend storage to the current format.
* The CollectionAttribute's KeepFormat property will be updated if the user
* chooses not to update the calendar.
*
* Note: the collection should be up to date: use AkonadiDataModel::refresh()
*       before calling this function.
*/
void AkonadiCalendarUpdater::updateToCurrentFormat(const Resource& resource, bool ignoreKeepFormat, QObject* parent)
{
    qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::updateToCurrentFormat:" << resource.id();
    if (containsResource(resource.id()))
        return;   // prevent multiple simultaneous user prompts
    const AgentInstance agent = AgentManager::self()->instance(resource.configName());
    const QString id = agent.type().identifier();
    bool dirResource;
    if (id == AkonadiResource::KALARM_RESOURCE)
        dirResource = false;
    else if (id == AkonadiResource::KALARM_DIR_RESOURCE)
        dirResource = true;
    else
    {
        qCCritical(KALARM_LOG) << "AkonadiCalendarUpdater::updateToCurrentFormat: Invalid agent type" << id;
        return;
    }
    const Collection& collection = AkonadiResource::collection(resource);
    AkonadiCalendarUpdater* updater = new AkonadiCalendarUpdater(collection, dirResource, ignoreKeepFormat, false, parent, qobject_cast<QWidget*>(parent));
    QTimer::singleShot(0, updater, &AkonadiCalendarUpdater::update);
}

/******************************************************************************
* If the calendar is not in the current KAlarm format, prompt the user whether
* to convert to the current format, and then perform the conversion.
*/
bool AkonadiCalendarUpdater::update()
{
    qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::update:" << mCollection.id() << (mDirResource ? "directory" : "file");
    bool result = true;
    if (isDuplicate())
        qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::update: Not updating (concurrent update in progress)";
    else if (mCollection.hasAttribute<CompatibilityAttribute>())   // must know format to update
    {
        const CompatibilityAttribute* compatAttr = mCollection.attribute<CompatibilityAttribute>();
        const KACalendar::Compat compatibility = compatAttr->compatibility();
        qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::update: current format:" << compatibility;
        if ((compatibility & ~KACalendar::Converted)
        // The calendar isn't in the current KAlarm format
        &&  !(compatibility & ~(KACalendar::Convertible | KACalendar::Converted)))
        {
            // The calendar format is convertible to the current KAlarm format
            if (!mIgnoreKeepFormat
            &&  mCollection.hasAttribute<CollectionAttribute>()
            &&  mCollection.attribute<CollectionAttribute>()->keepFormat())
                qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::update: Not updating format (previous user choice)";
            else
            {
                // The user hasn't previously said not to convert it
                const QString versionString = KAlarmCal::getVersionString(compatAttr->version());
                const QString msg = conversionPrompt(mCollection.name(), versionString, false);
                qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::update: Version" << versionString;
                if (KAMessageBox::warningYesNo(mPromptParent, msg) != KMessageBox::Yes)
                    result = false;   // the user chose not to update the calendar
                else
                {
                    // Tell the resource to update the backend storage format
                    QString errmsg;
                    if (!mNewCollection)
                    {
                        // Refetch the collection's details because anything could
                        // have happened since the prompt was first displayed.
                        if (!AkonadiDataModel::instance()->refresh(mCollection))
                            errmsg = i18nc("@info", "Invalid collection");
                    }
                    if (errmsg.isEmpty())
                    {
                        const AgentInstance agent = AgentManager::self()->instance(mCollection.resource());
                        if (mDirResource)
                            updateStorageFormat<OrgKdeAkonadiKAlarmDirSettingsInterface>(agent, errmsg, mParent);
                        else
                            updateStorageFormat<OrgKdeAkonadiKAlarmSettingsInterface>(agent, errmsg, mParent);
                    }
                    if (!errmsg.isEmpty())
                    {
                        Resources::notifyResourceMessage(mCollection.id(), ResourceType::MessageType::Error,
                                                         xi18nc("@info", "Failed to update format of calendar <resource>%1</resource>", mCollection.name()),
                                                         errmsg);
                    }
                }
                if (!mNewCollection)
                {
                    // Record the user's choice of whether to update the calendar
                    Resource resource = AkonadiDataModel::instance()->resource(mCollection.id());
                    resource.setKeepFormat(!result);
                }
            }
        }
    }
    setCompleted();
    return result;
}

/******************************************************************************
* Tell an Akonadi resource to update the backend storage format to the current
* KAlarm format.
* Reply = true if success; if false, 'errorMessage' contains the error message.
*/
template <class Interface> bool AkonadiCalendarUpdater::updateStorageFormat(const AgentInstance& agent, QString& errorMessage, QObject* parent)
{
    qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::updateStorageFormat";
    Interface* iface = AkonadiResource::getAgentInterface<Interface>(agent, errorMessage, parent);
    if (!iface)
    {
        qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::updateStorageFormat:" << errorMessage;
        return false;
    }
    iface->setUpdateStorageFormat(true);
    iface->save();
    delete iface;
    qCDebug(KALARM_LOG) << "AkonadiCalendarUpdater::updateStorageFormat: true";
    return true;
}

// vim: et sw=4:
