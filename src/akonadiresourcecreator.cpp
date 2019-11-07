/*
 *  akonadiresourcecreator.cpp  -  interactively create an Akonadi resource
 *  Program:  kalarm
 *  Copyright © 2011,2019 David Jarvie <djarvie@kde.org>
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

#include "akonadiresourcecreator.h"

#include "autoqpointer.h"
#include "collectionmodel.h"
#include "resources/resources.h"
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "kalarm_debug.h"

#include <AkonadiCore/agentfilterproxymodel.h>
#include <AkonadiCore/agentinstancecreatejob.h>
#include <AkonadiCore/agentmanager.h>
#include <AkonadiWidgets/agenttypedialog.h>
#include <AkonadiWidgets/AgentConfigurationDialog>

#include <KDBusConnectionPool>
#include <KMessageBox>
#include <KLocalizedString>

#include <QTimer>

using namespace Akonadi;
using namespace KAlarmCal;


AkonadiResourceCreator::AkonadiResourceCreator(CalEvent::Type defaultType, QWidget* parent)
    : QObject()
    , mParent(parent)
    , mDefaultType(defaultType)
{
}

/******************************************************************************
* Create a new resource. The user will be prompted to enter its configuration.
*/
void AkonadiResourceCreator::createResource()
{
    QTimer::singleShot(0, this, &AkonadiResourceCreator::getAgentType);
}

void AkonadiResourceCreator::getAgentType()
{
    qCDebug(KALARM_LOG) << "AkonadiResourceCreator::getAgentType: Type:" << mDefaultType;
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<AgentTypeDialog> dlg = new AgentTypeDialog(mParent);
    QString mimeType;
    switch (mDefaultType)
    {
        case CalEvent::ACTIVE:
            mimeType = KAlarmCal::MIME_ACTIVE;
            break;
        case CalEvent::ARCHIVED:
            mimeType = KAlarmCal::MIME_ARCHIVED;
            break;
        case CalEvent::TEMPLATE:
            mimeType = KAlarmCal::MIME_TEMPLATE;
            break;
        default:
            deleteLater();   // error result
            return;
    }
    dlg->agentFilterProxyModel()->addMimeTypeFilter(mimeType);
    dlg->agentFilterProxyModel()->addCapabilityFilter(QStringLiteral("Resource"));
    if (dlg->exec() == QDialog::Accepted)
    {
        mAgentType = dlg->agentType();
        if (mAgentType.isValid())
        {
            connect(Resources::instance(), &Resources::resourceAdded,
                                     this, &AkonadiResourceCreator::slotResourceAdded);

            AgentInstanceCreateJob* job = new AgentInstanceCreateJob(mAgentType, mParent);
            connect(job, &AgentInstanceCreateJob::result, this, &AkonadiResourceCreator::agentInstanceCreated);
            job->start();
            return;
        }
    }
    deleteLater();   // error result
}

/******************************************************************************
* Called when an agent creation job has completed.
* Checks for any error.
*/
void AkonadiResourceCreator::agentInstanceCreated(KJob* j)
{
    AgentInstanceCreateJob* job = static_cast<AgentInstanceCreateJob*>(j);
    if (j->error())
    {
        qCCritical(KALARM_LOG) << "AkonadiResourceCreator::agentInstanceCreated: Failed to create new calendar resource:" << j->errorString();
        KMessageBox::error(nullptr, xi18nc("@info", "%1<nl/>(%2)", i18nc("@info", "Failed to create new calendar resource"), j->errorString()));
        deleteLater();   // error result
        return;
    }

    // Set the default alarm type for the resource config dialog
    mAgentInstance = job->instance();
    const QString type = mAgentInstance.type().identifier();
    qCDebug(KALARM_LOG) << "AkonadiResourceCreator::agentInstanceCreated:" << type;
    bool result = true;
    bool dirResource = (type == QLatin1String("akonadi_kalarm_dir_resource"));
    if (dirResource)
        setResourceAlarmType<OrgKdeAkonadiKAlarmDirSettingsInterface>();
    else if (type == QLatin1String("akonadi_kalarm_resource"))
        setResourceAlarmType<OrgKdeAkonadiKAlarmSettingsInterface>();
    else
        result = false;

    if (result)
    {
        const Collection::List cols = CollectionControlModel::allCollections();
        QPointer<AgentConfigurationDialog> dlg = new AgentConfigurationDialog(mAgentInstance, mParent);
        result = (dlg->exec() == QDialog::Accepted);
        delete dlg;
        if (result)
        {
            // Ensure that the new resource doesn't use the same file or directory
            // as an existing resource. This would result in duplicate resource
            // executables eating up processing time for no purpose.
            QString path = dirResource ? getResourcePath<OrgKdeAkonadiKAlarmDirSettingsInterface>()
                                       : getResourcePath<OrgKdeAkonadiKAlarmSettingsInterface>();
            for (const Collection& c : cols)
            {
                if (c.remoteId() == path)
                {
                    qCWarning(KALARM_LOG) << "AkonadiResourceCreator::agentInstanceCreated: Duplicate path for new resource:" << path;
                    AgentManager::self()->removeInstance(mAgentInstance);
                    const QUrl url = QUrl::fromUserInput(path, QString(), QUrl::AssumeLocalFile);
                    if (url.isLocalFile())
                        path = url.path();
                    KMessageBox::sorry(nullptr, xi18nc("@info", "<para>The file or directory is already used by an existing resource:</para><para><filename>%1</filename></para>", path));
                    deleteLater();   // error result
                    return;
                }
            }
        }
    }
    if (!result)
    {
        // User has clicked cancel in the resource configuration dialog, or
        // other error, so remove the newly created agent instance.
        AgentManager::self()->removeInstance(mAgentInstance);
        deleteLater();   // error result
        return;
    }

    // AkonadiModel will notify when it has added the resource, which will
    // call slotResourceAdded().
}

/******************************************************************************
* Called when a collection is added to the AkonadiModel.
* If it's the resource which has just been created, notify the fact.
*/
void AkonadiResourceCreator::slotResourceAdded(Resource& resource)
{
    if (resource.isValid()  &&  resource.is<AkonadiResource>())
    {
        AgentInstance agent = AgentManager::self()->instance(resource.configName());
        if (agent == mAgentInstance)
            Q_EMIT resourceAdded(resource, mDefaultType);
    }

    // This object has done its job, so delete it.
    deleteLater();
}

/******************************************************************************
* Set the alarm type for an Akonadi resource.
*/
template <class Settings>
void AkonadiResourceCreator::setResourceAlarmType()
{
    Settings iface(QStringLiteral("org.freedesktop.Akonadi.Resource.") + mAgentInstance.identifier(),
                   QStringLiteral("/Settings"), QDBusConnection::sessionBus(), this);
    if (!iface.isValid())
        qCCritical(KALARM_LOG) << "AkonadiResourceCreator::setResourceAlarmType: Error creating D-Bus interface for" << mAgentInstance.identifier() << "resource configuration.";
    else
    {
        iface.setAlarmTypes(CalEvent::mimeTypes(mDefaultType));
        iface.save();
        mAgentInstance.reconfigure();   // notify the agent that its configuration has changed
    }
}

/******************************************************************************
* Get the path for an Akonadi resource.
*/
template <class Settings>
QString AkonadiResourceCreator::getResourcePath()
{
    Settings iface(QStringLiteral("org.freedesktop.Akonadi.Resource.") + mAgentInstance.identifier(),
                   QStringLiteral("/Settings"), QDBusConnection::sessionBus(), this);
    if (!iface.isValid())
    {
        qCCritical(KALARM_LOG) << "AkonadiResourceCreator::getResourcePath: Error creating D-Bus interface for" << mAgentInstance.identifier() << "resource configuration.";
        return QString();
    }
    return iface.path();
}

// vim: et sw=4:
