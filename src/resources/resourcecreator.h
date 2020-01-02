/*
 *  resourcecreator.h  -  base class to interactively create a resource
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

#ifndef RESOURCECREATOR_H
#define RESOURCECREATOR_H

#include <KAlarmCal/KACalendar>

class QWidget;
class Resource;

/**
 * Base class for interactively creating a resource.
 * Derived classes must call deleteLater() to delete themselves if resource
 * creation is unsuccessful, i.e. if the resourceAdded() signal is not called.
 */
class ResourceCreator : public QObject
{
    Q_OBJECT
public:
    ResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent);
    void createResource();

Q_SIGNALS:
    /** Signal emitted when a resource has been created.
     *  @param type  The default alarm type specified in the constructor.
     */
    void resourceAdded(Resource&, KAlarmCal::CalEvent::Type);

protected Q_SLOTS:
    virtual void doCreateResource() = 0;

protected:
    QWidget*                  mParent;
    KAlarmCal::CalEvent::Type mDefaultType;
};

#endif // RESOURCECREATOR_H

// vim: et sw=4:
