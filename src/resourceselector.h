/*
 *  resourceselector.h  -  alarm calendar resource selection widget
 *  Program:  kalarm
 *  Copyright © 2006-2019 David Jarvie <djarvie@kde.org>
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

#include "resources/resource.h"

#include <QFrame>
#include <QSize>

using namespace KAlarmCal;

class QPushButton;
class QResizeEvent;
class QAction;
class KActionCollection;
class KToggleAction;
class QComboBox;
class QMenu;
class ResourceView;
class AkonadiResourceCreator;


/**
  This class provides a view of alarm calendar resources.
*/
class ResourceSelector : public QFrame
{
        Q_OBJECT
    public:
        explicit ResourceSelector(QWidget* parent = nullptr);
        void  initActions(KActionCollection*);
        void  setContextMenu(QMenu*);

    Q_SIGNALS:
        void  resized(const QSize& oldSize, const QSize& newSize);

    protected:
        void  resizeEvent(QResizeEvent*) override;

    private Q_SLOTS:
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
        void  slotResourceAdded(Resource&, CalEvent::Type);
        void  reinstateAlarmTypeScrollBars();

    private:
        CalEvent::Type currentResourceType() const;
        Resource currentResource() const;

        ResourceView*   mListView;
        QComboBox*      mAlarmType;
        QPushButton*    mAddButton;
        QPushButton*    mDeleteButton;
        QPushButton*    mEditButton;
        CalEvent::Type  mCurrentAlarmType;
        QMenu*          mContextMenu{nullptr};
        QAction*        mActionReload{nullptr};
        QAction*        mActionShowDetails{nullptr};
        QAction*        mActionSetColour{nullptr};
        QAction*        mActionClearColour{nullptr};
        QAction*        mActionEdit{nullptr};
        QAction*        mActionUpdate{nullptr};
        QAction*        mActionRemove{nullptr};
        QAction*        mActionImport{nullptr};
        QAction*        mActionExport{nullptr};
        KToggleAction*  mActionSetDefault{nullptr};
};

#endif

// vim: et sw=4:
