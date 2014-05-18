/*
 *  resourceselector.h  -  alarm calendar resource selection widget
 *  Program:  kalarm
 *  Copyright © 2006-2011 by David Jarvie <djarvie@kde.org>
 *  Based on KOrganizer's ResourceView class and KAddressBook's ResourceSelection class,
 *  Copyright (C) 2003,2004 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
 *  Copyright (c) 2004 Tobias Koenig <tokoe@kde.org>
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

#ifndef RESOURCESELECTOR_H
#define RESOURCESELECTOR_H

#include "akonadimodel.h"
#include "collectionmodel.h"

#include <AkonadiCore/agentinstance.h>

#include <QFrame>
#include <QSize>
#include <QList>

using namespace KAlarmCal;

class QPushButton;
class QResizeEvent;
class KAction;
class KActionCollection;
class KToggleAction;
class KComboBox;
class KMenu;
class ResourceView;
class AkonadiResourceCreator;
namespace Akonadi {
    class Collection;
}


/**
  This class provides a view of alarm calendar resources.
*/
class ResourceSelector : public QFrame
{
        Q_OBJECT
    public:
        explicit ResourceSelector(QWidget* parent = 0);
        void  initActions(KActionCollection*);
        void  setContextMenu(KMenu*);

    signals:
        void  resized(const QSize& oldSize, const QSize& newSize);

    protected:
        virtual void resizeEvent(QResizeEvent*);

    private slots:
        void  alarmTypeSelected();
        void  addResource();
        void  editResource();
        void  updateResource();
        void  removeResource();
        void  selectionChanged();
        void  contextMenuRequested(const QPoint&);
        void  reloadResource();
        void  saveResource();
        void  setStandard();
        void  setColour();
        void  clearColour();
        void  importCalendar();
        void  exportCalendar();
        void  showInfo();
        void  archiveDaysChanged(int days);
        void  resourceAdded(AkonadiResourceCreator*, bool success);
        void  slotCollectionAdded(const Akonadi::Collection&);
        void  reinstateAlarmTypeScrollBars();

    private:
        CalEvent::Type currentResourceType() const;
        Akonadi::Collection currentResource() const;

        CollectionView* mListView;
        QList<Akonadi::AgentInstance> mAddAgents;   // agent added by addResource()
        KComboBox*      mAlarmType;
        QPushButton*    mAddButton;
        QPushButton*    mDeleteButton;
        QPushButton*    mEditButton;
        CalEvent::Type  mCurrentAlarmType;
        KMenu*          mContextMenu;
        KAction*        mActionReload;
        KAction*        mActionShowDetails;
        KAction*        mActionSetColour;
        KAction*        mActionClearColour;
        KAction*        mActionEdit;
        KAction*        mActionUpdate;
        KAction*        mActionRemove;
        KAction*        mActionImport;
        KAction*        mActionExport;
        KToggleAction*  mActionSetDefault;
};

#endif

// vim: et sw=4:
