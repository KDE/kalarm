/*
 *  resourceselector.cpp  -  alarm calendar resource selection widget
 *  Program:  kalarm
 *  Copyright © 2006,2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QModelIndex>
#include <QFrame>
#include <QSize>
#include "alarmresource.h"
#include "alarmresources.h"

class QPushButton;
class QResizeEvent;
class KAction;
class KActionCollection;
class KToggleAction;
class KComboBox;
class KMenu;
class ResourceView;
using KCal::ResourceCalendar;


/**
  This class provides a view of alarm calendar resources.
*/
class ResourceSelector : public QFrame
{
	Q_OBJECT
    public:
	explicit ResourceSelector(AlarmResources*, QWidget* parent = 0);
	AlarmResources* calendar() const    { return mCalendar; }
	void  initActions(KActionCollection*);
	void  setContextMenu(KMenu*);

    signals:
	/** Signal that a resource has been added, removed or its active status changed. */
	void  resourcesChanged();
	void  resized(const QSize& oldSize, const QSize& newSize);

    protected:
	virtual void resizeEvent(QResizeEvent*);

    private slots:
	void  alarmTypeSelected();
	void  addResource();
	void  editResource();
	void  removeResource();
	void  selectionChanged();
	void  contextMenuRequested(const QPoint&);
	void  reloadResource();
	void  saveResource();
	void  setStandard();
	void  setColour();
	void  clearColour();
	void  importCalendar();
	void  showInfo();

    private:
	AlarmResource* currentResource() const;

	AlarmResources* mCalendar;
	KComboBox*      mAlarmType;
	ResourceView*   mListView;
	QPushButton*    mAddButton;
	QPushButton*    mDeleteButton;
	QPushButton*    mEditButton;
	QList<AlarmResource*> mResourcesToClose;
	AlarmResource::Type mCurrentAlarmType;
	KMenu*          mContextMenu;
	KAction*        mActionReload;
	KAction*        mActionSave;
	KAction*        mActionShowDetails;
	KAction*        mActionSetColour;
	KAction*        mActionClearColour;
	KAction*        mActionEdit;
	KAction*        mActionRemove;
	KAction*        mActionImport;
	KToggleAction*  mActionSetDefault;
};

#endif
