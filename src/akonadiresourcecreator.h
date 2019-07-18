/*
 *  akonadiresourcecreator.h  -  interactively create an Akonadi resource
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

#ifndef AKONADIRESOURCECREATOR_H
#define AKONADIRESOURCECREATOR_H

#include "kalarmsettings.h"
#include "kalarmdirsettings.h"

#include <kalarmcal/kacalendar.h>

#include <AkonadiCore/agentinstance.h>
#include <AkonadiCore/agenttype.h>

using namespace KAlarmCal;

class QWidget;
class KJob;

class AkonadiResourceCreator : public QObject
{
        Q_OBJECT
    public:
        AkonadiResourceCreator(CalEvent::Type defaultType, QWidget* parent);
        void createResource();
        Akonadi::AgentInstance agentInstance() const   { return mAgentInstance; }

    Q_SIGNALS:
        void finished(AkonadiResourceCreator*, bool success);

    private Q_SLOTS:
        void getAgentType();
        void agentInstanceCreated(KJob*);

    private:
        template <class Settings> void setResourceAlarmType();
        template <class Settings> QString getResourcePath();

        QWidget*               mParent;
        CalEvent::Type         mDefaultType;
        Akonadi::AgentType     mAgentType;
        Akonadi::AgentInstance mAgentInstance;
};

#endif // AKONADIRESOURCECREATOR_H

// vim: et sw=4:
