/*
 *  akonadiresourcecreator.h  -  interactively create an Akonadi resource
 *  Program:  kalarm
 *  Copyright © 2011-2020 David Jarvie <djarvie@kde.org>
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

#ifndef AKONADIRESOURCECREATOR_H
#define AKONADIRESOURCECREATOR_H

#include "resourcecreator.h"

#include <AkonadiCore/AgentInstance>
#include <AkonadiCore/AgentType>

class KJob;

class AkonadiResourceCreator : public ResourceCreator
{
    Q_OBJECT
public:
    AkonadiResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent);
    Akonadi::AgentInstance agentInstance() const   { return mAgentInstance; }

protected Q_SLOTS:
    void doCreateResource() override;

private Q_SLOTS:
    void agentInstanceCreated(KJob*);
    void slotResourceAdded(Resource&);

private:
    template <class Settings> void setResourceAlarmType();
    template <class Settings> QString getResourcePath();

    Akonadi::AgentType     mAgentType;
    Akonadi::AgentInstance mAgentInstance;
};

#endif // AKONADIRESOURCECREATOR_H

// vim: et sw=4: