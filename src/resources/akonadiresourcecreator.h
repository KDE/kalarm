/*
 *  akonadiresourcecreator.h  -  interactively create an Akonadi resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
