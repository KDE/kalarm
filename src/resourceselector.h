/*
 *  resourceselector.h  -  alarm calendar resource selection widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006-2021 David Jarvie <djarvie@kde.org>
 *  Based on KOrganizer's ResourceView class and KAddressBook's ResourceSelection class,
 *  SPDX-FileCopyrightText: 2003, 2004 Cornelius Schumacher <schumacher@kde.org>
 *  SPDX-FileCopyrightText: 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
 *  SPDX-FileCopyrightText: 2004 Tobias Koenig <tokoe@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
class MainWindow;
class ResourceView;


/**
  This class provides a view of alarm calendar resources.
*/
class ResourceSelector : public QFrame
{
    Q_OBJECT
public:
    explicit ResourceSelector(MainWindow* parentWindow, QWidget* parent = nullptr);
    void  initActions(KActionCollection*);

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

    MainWindow*     mMainWindow;
    ResourceView*   mListView;
    QComboBox*      mAlarmType;
    QPushButton*    mAddButton;
    QPushButton*    mDeleteButton;
    QPushButton*    mEditButton;
    CalEvent::Type  mCurrentAlarmType;
    QMenu*          mContextMenu {nullptr};
    QAction*        mActionReload {nullptr};
    QAction*        mActionShowDetails {nullptr};
    QAction*        mActionSetColour {nullptr};
    QAction*        mActionClearColour {nullptr};
    QAction*        mActionEdit {nullptr};
    QAction*        mActionUpdate {nullptr};
    QAction*        mActionRemove {nullptr};
    QAction*        mActionImport {nullptr};
    QAction*        mActionExport {nullptr};
    KToggleAction*  mActionSetDefault {nullptr};
};

// vim: et sw=4:
