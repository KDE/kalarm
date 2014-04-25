/*
 *  akonadiresourcecreator.cpp  -  interactively create an Akonadi resource
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "controlinterface.h"

#include <akonadi/agentfilterproxymodel.h>
#include <AkonadiCore/agentinstancecreatejob.h>
#include <AkonadiCore/agentmanager.h>
#include <akonadi/agenttypedialog.h>
#include <akonadi/dbusconnectionpool.h>

#include <kmessagebox.h>
#include <klocale.h>
#include <kdebug.h>

#include <QTimer>

using namespace Akonadi;
using namespace KAlarmCal;


AkonadiResourceCreator::AkonadiResourceCreator(CalEvent::Type defaultType, QWidget* parent)
    : QObject(),
      mParent(parent),
      mDefaultType(defaultType)
{
}

/******************************************************************************
* Create a new resource. The user will be prompted to enter its configuration.
*/
void AkonadiResourceCreator::createResource()
{
    QTimer::singleShot(0, this, SLOT(getAgentType()));
}

void AkonadiResourceCreator::getAgentType()
{
    kDebug() << "Type:" << mDefaultType;
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
            emit finished(this, false);
            return;
    }
    dlg->agentFilterProxyModel()->addMimeTypeFilter(mimeType);
    dlg->agentFilterProxyModel()->addCapabilityFilter(QLatin1String("Resource"));
    if (dlg->exec() != QDialog::Accepted)
    {
        emit finished(this, false);
        return;
    }
    mAgentType = dlg->agentType();
    if (!mAgentType.isValid())
    {
        emit finished(this, false);
        return;
    }
    AgentInstanceCreateJob* job = new AgentInstanceCreateJob(mAgentType, mParent);
    connect(job, SIGNAL(result(KJob*)), SLOT(agentInstanceCreated(KJob*)));
    job->start();
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
        kError() << "Failed to create new calendar resource:" << j->errorString();
        KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)", i18nc("@info/plain", "Failed to create new calendar resource"), j->errorString()));
        exitWithError();
    }
    else
    {
        // Set the default alarm type for a directory resource config dialog
        mAgentInstance = job->instance();
        QString type = mAgentInstance.type().identifier();
        if (type == QLatin1String("akonadi_kalarm_dir_resource"))
            setResourceAlarmType<OrgKdeAkonadiKAlarmDirSettingsInterface>();
        else if (type == QLatin1String("akonadi_kalarm_resource"))
            setResourceAlarmType<OrgKdeAkonadiKAlarmSettingsInterface>();

        // Display the resource config dialog, but first ensure we get
        // notified of the user cancelling the operation.
        org::freedesktop::Akonadi::Agent::Control* agentControlIface =
                    new org::freedesktop::Akonadi::Agent::Control(QLatin1String("org.freedesktop.Akonadi.Agent.") + mAgentInstance.identifier(),
                                                                  QLatin1String("/"), DBusConnectionPool::threadConnection(), this);
        bool controlOk = agentControlIface && agentControlIface->isValid();
        if (!controlOk)
        {
            delete agentControlIface;
            kWarning() << "Unable to access D-Bus interface of created agent.";
        }
        else
        {
            connect(agentControlIface, SIGNAL(configurationDialogAccepted()), SLOT(configurationDialogAccepted()));
            connect(agentControlIface, SIGNAL(configurationDialogRejected()), SLOT(exitWithError()));
        }
        mAgentInstance.configure(mParent);

        if (!controlOk)
            emit finished(this, true);  // don't actually know the result in this case
    }
}

/******************************************************************************
* Set the alarm type for an Akonadi resource.
*/
template <class Settings>
void AkonadiResourceCreator::setResourceAlarmType()
{
    Settings iface(QLatin1String("org.freedesktop.Akonadi.Resource.") + mAgentInstance.identifier(),
                   QLatin1String("/Settings"), QDBusConnection::sessionBus(), this);
    if (!iface.isValid())
        kError() << "Error creating D-Bus interface for" << mAgentInstance.identifier() << "resource configuration.";
    else
    {
        iface.setAlarmTypes(CalEvent::mimeTypes(mDefaultType));
        iface.writeConfig();
        mAgentInstance.reconfigure();   // notify the agent that its configuration has changed
    }
}

/******************************************************************************
* Called when the user has clicked OK in the resource configuration dialog.
*/
void AkonadiResourceCreator::configurationDialogAccepted()
{
    emit finished(this, true);
}

/******************************************************************************
* Called when the user has clicked cancel in the resource configuration dialog.
* Remove the newly created agent instance.
*/
void AkonadiResourceCreator::exitWithError()
{
    AgentManager::self()->removeInstance(mAgentInstance);
    emit finished(this, false);
}

// vim: et sw=4:
