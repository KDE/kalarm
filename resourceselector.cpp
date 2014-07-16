/*
 *  resourceselector.cpp  -  calendar resource selection widget
 *  Program:  kalarm
 *  Copyright © 2006-2013 by David Jarvie <djarvie@kde.org>
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

#include "resourceselector.h"

#include "kalarm.h"
#include "alarmcalendar.h"
#include "autoqpointer.h"
#include "akonadiresourcecreator.h"
#include "calendarmigrator.h"
#include "kalarmapp.h"
#include "messagebox.h"
#include "packedlayout.h"
#include "preferences.h"
//#include "resourceconfigdialog.h"

#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/agentinstancecreatejob.h>
#include <AkonadiCore/agenttype.h>
#include <AkonadiWidgets/collectionpropertiesdialog.h>
#include <AkonadiCore/entitydisplayattribute.h>

#include <kdialog.h>
#include <klocale.h>
#include <kglobal.h>
#include <kcombobox.h>
#include <qinputdialog.h>
#include <QMenu>
#include <qdebug.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <ktoggleaction.h>
#include <kcolordialog.h>

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QResizeEvent>
#include <QApplication>

using namespace KCalCore;
using namespace Akonadi;


ResourceSelector::ResourceSelector(QWidget* parent)
    : QFrame(parent),
      mContextMenu(0),
      mActionReload(0),
      mActionShowDetails(0),
      mActionSetColour(0),
      mActionClearColour(0),
      mActionEdit(0),
      mActionUpdate(0),
      mActionRemove(0),
      mActionImport(0),
      mActionExport(0),
      mActionSetDefault(0)
{
    QBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->setMargin(KDialog::spacingHint());   // use spacingHint for the margin

    QLabel* label = new QLabel(i18nc("@title:group", "Calendars"), this);
    topLayout->addWidget(label, 0, Qt::AlignHCenter);

    mAlarmType = new KComboBox(this);
    mAlarmType->addItem(i18nc("@item:inlistbox", "Active Alarms"));
    mAlarmType->addItem(i18nc("@item:inlistbox", "Archived Alarms"));
    mAlarmType->addItem(i18nc("@item:inlistbox", "Alarm Templates"));
    mAlarmType->setFixedHeight(mAlarmType->sizeHint().height());
    mAlarmType->setWhatsThis(i18nc("@info:whatsthis", "Choose which type of data to show alarm calendars for"));
    topLayout->addWidget(mAlarmType);
    // No spacing between combo box and listview.

    CollectionFilterCheckListModel* model = new CollectionFilterCheckListModel(this);
    mListView = new CollectionView(model, this);
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(selectionChanged()));
    mListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mListView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(contextMenuRequested(QPoint)));
    mListView->setWhatsThis(i18nc("@info:whatsthis",
                                  "List of available calendars of the selected type. The checked state shows whether a calendar "
                                 "is enabled (checked) or disabled (unchecked). The default calendar is shown in bold."));
    topLayout->addWidget(mListView, 1);
    topLayout->addSpacing(KDialog::spacingHint());

    PackedLayout* blayout = new PackedLayout(Qt::AlignHCenter);
    blayout->setMargin(0);
    blayout->setSpacing(KDialog::spacingHint());
    topLayout->addLayout(blayout);

    mAddButton    = new QPushButton(i18nc("@action:button", "Add..."), this);
    mEditButton   = new QPushButton(i18nc("@action:button", "Edit..."), this);
    mDeleteButton = new QPushButton(i18nc("@action:button", "Remove"), this);
    blayout->addWidget(mAddButton);
    blayout->addWidget(mEditButton);
    blayout->addWidget(mDeleteButton);
    mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the highlighted calendar"));
    mDeleteButton->setWhatsThis(xi18nc("@info:whatsthis", "<para>Remove the highlighted calendar from the list.</para>"
                                     "<para>The calendar itself is left intact, and may subsequently be reinstated in the list if desired.</para>"));
    mEditButton->setDisabled(true);
    mDeleteButton->setDisabled(true);
    connect(mAddButton, SIGNAL(clicked()), SLOT(addResource()));
    connect(mEditButton, SIGNAL(clicked()), SLOT(editResource()));
    connect(mDeleteButton, SIGNAL(clicked()), SLOT(removeResource()));

    connect(AkonadiModel::instance(), SIGNAL(collectionAdded(Akonadi::Collection)),
                                      SLOT(slotCollectionAdded(Akonadi::Collection)));

    connect(mAlarmType, SIGNAL(activated(int)), SLOT(alarmTypeSelected()));
    QTimer::singleShot(0, this, SLOT(alarmTypeSelected()));

    Preferences::connect(SIGNAL(archivedKeepDaysChanged(int)), this, SLOT(archiveDaysChanged(int)));
}

/******************************************************************************
* Called when an alarm type has been selected.
* Filter the resource list to show resources of the selected alarm type, and
* add appropriate whatsThis texts to the list and to the Add button.
*/
void ResourceSelector::alarmTypeSelected()
{
    QString addTip;
    switch (mAlarmType->currentIndex())
    {
        case 0:
            mCurrentAlarmType = CalEvent::ACTIVE;
            addTip = i18nc("@info:tooltip", "Add a new active alarm calendar");
            break;
        case 1:
            mCurrentAlarmType = CalEvent::ARCHIVED;
            addTip = i18nc("@info:tooltip", "Add a new archived alarm calendar");
            break;
        case 2:
            mCurrentAlarmType = CalEvent::TEMPLATE;
            addTip = i18nc("@info:tooltip", "Add a new alarm template calendar");
            break;
    }
    // WORKAROUND: Switch scroll bars off to avoid crash (see explanation
    // in reinstateAlarmTypeScrollBars() description).
    mListView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mListView->collectionModel()->setEventTypeFilter(mCurrentAlarmType);
    mAddButton->setWhatsThis(addTip);
    mAddButton->setToolTip(addTip);
    // WORKAROUND: Switch scroll bars back on after allowing geometry to update ...
    QTimer::singleShot(0, this, SLOT(reinstateAlarmTypeScrollBars()));

    selectionChanged();   // enable/disable buttons
}

/******************************************************************************
* WORKAROUND for crash due to presumed Qt bug.
* Switch scroll bars off. This is to avoid a crash which can very occasionally
* happen when changing from a list of calendars which requires vertical scroll
* bars, to a list whose text is very slightly wider but which doesn't require
* scroll bars at all. (The suspicion is that the width is such that it would
* require horizontal scroll bars if the vertical scroll bars were still
* present.) Presumably due to a Qt bug, this can result in a recursive call to
* ResourceView::viewportEvent() with a Resize event.
*
* The crash only occurs if the ResourceSelector happens to have exactly (within
* one pixel) the "right" width to create the crash.
*/
void ResourceSelector::reinstateAlarmTypeScrollBars()
{
    mListView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

/******************************************************************************
* Prompt the user for a new resource to add to the list.
*/
void ResourceSelector::addResource()
{
    AkonadiResourceCreator* creator = new AkonadiResourceCreator(mCurrentAlarmType, this);
    connect(creator, SIGNAL(finished(AkonadiResourceCreator*,bool)), 
                     SLOT(resourceAdded(AkonadiResourceCreator*,bool)));
    creator->createResource();
}

/******************************************************************************
* Called when the job started by AkonadiModel::addCollection() has completed.
*/
void ResourceSelector::resourceAdded(AkonadiResourceCreator* creator, bool success)
{
    if (success)
    {
        AgentInstance agent = creator->agentInstance();
        if (agent.isValid())
        {
            // Note that we're expecting the agent's Collection to be added
            mAddAgents += agent;
        }
    }
    delete creator;
}

/******************************************************************************
* Called when a collection is added to the AkonadiModel.
*/
void ResourceSelector::slotCollectionAdded(const Collection& collection)
{
    if (collection.isValid())
    {
        AgentInstance agent = AgentManager::self()->instance(collection.resource());
        if (agent.isValid())
        {
            int i = mAddAgents.indexOf(agent);
            if (i >= 0)
            {
                // The collection belongs to an agent created by addResource()
                CalEvent::Types types = CalEvent::types(collection.contentMimeTypes());
                CollectionControlModel::setEnabled(collection, types, true);
                if (!(types & mCurrentAlarmType))
                {
                    // The user has selected alarm types for the resource
                    // which don't include the currently displayed type.
                    // Show a collection list which includes a selected type.
                    int index = -1;
                    if (types & CalEvent::ACTIVE)
                        index = 0;
                    else if (types & CalEvent::ARCHIVED)
                        index = 1;
                    else if (types & CalEvent::TEMPLATE)
                        index = 2;
                    if (index >= 0)
                    {
                        mAlarmType->setCurrentIndex(index);
                        alarmTypeSelected();
                    }
                }
                mAddAgents.removeAt(i);
            }
        }
    }
}

/******************************************************************************
* Edit the currently selected resource.
*/
void ResourceSelector::editResource()
{
    Collection collection = currentResource();
    if (collection.isValid())
    {
        AgentInstance instance = AgentManager::self()->instance(collection.resource());
        if (instance.isValid())
            instance.configure(this);
    }
}

/******************************************************************************
* Update the backend storage format for the currently selected resource in the
* displayed list.
*/
void ResourceSelector::updateResource()
{
    Collection collection = currentResource();
    if (!collection.isValid())
        return;
    AkonadiModel::instance()->refresh(collection);  // update with latest data
    CalendarMigrator::updateToCurrentFormat(collection, true, this);
}

/******************************************************************************
* Remove the currently selected resource from the displayed list.
*/
void ResourceSelector::removeResource()
{
    Collection collection = currentResource();
    if (!collection.isValid())
        return;
    QString name = collection.name();
    // Check if it's the standard or only resource for at least one type.
    CalEvent::Types allTypes      = AkonadiModel::types(collection);
    CalEvent::Types standardTypes = CollectionControlModel::standardTypes(collection, true);
    CalEvent::Type  currentType   = currentResourceType();
    CalEvent::Type stdType = (standardTypes & CalEvent::ACTIVE)   ? CalEvent::ACTIVE
                                   : (standardTypes & CalEvent::ARCHIVED) ? CalEvent::ARCHIVED
                                   : CalEvent::EMPTY;
    if (stdType == CalEvent::ACTIVE)
    {
        KAMessageBox::sorry(this, i18nc("@info", "You cannot remove your default active alarm calendar."));
        return;
    }
    if (stdType == CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
    {
        // Only allow the archived alarms standard resource to be removed if
        // we're not saving archived alarms.
        KAMessageBox::sorry(this, i18nc("@info", "You cannot remove your default archived alarm calendar "
                                        "while expired alarms are configured to be kept."));
        return;
    }
    QString text;
    if (standardTypes)
    {
        // It's a standard resource for at least one alarm type
        if (allTypes != currentType)
        {
            // It also contains alarm types other than the currently displayed type
            QString stdTypes = CollectionControlModel::typeListForDisplay(standardTypes);
            QString otherTypes;
            CalEvent::Types nonStandardTypes(allTypes & ~standardTypes);
            if (nonStandardTypes != currentType)
                otherTypes = xi18nc("@info", "<para>It also contains:%1</para>", CollectionControlModel::typeListForDisplay(nonStandardTypes));
            text = xi18nc("@info", "<para><resource>%1</resource> is the default calendar for:%2</para>%3"
                                  "<para>Do you really want to remove it from all calendar lists?</para>", name, stdTypes, otherTypes);
        }
        else
            text = xi18nc("@info", "Do you really want to remove your default calendar (<resource>%1</resource>) from the list?", name);
    }
    else if (allTypes != currentType)
        text = xi18nc("@info", "<para><resource>%1</resource> contains:%2</para><para>Do you really want to remove it from all calendar lists?</para>",
                     name, CollectionControlModel::typeListForDisplay(allTypes));
    else
        text = xi18nc("@info", "Do you really want to remove the calendar <resource>%1</resource> from the list?", name);
    if (KAMessageBox::warningContinueCancel(this, text, QString(), KStandardGuiItem::remove()) == KMessageBox::Cancel)
        return;

    AkonadiModel::instance()->removeCollection(collection);
}

/******************************************************************************
* Called when the current selection changes, to enable/disable the
* Delete and Edit buttons accordingly.
*/
void ResourceSelector::selectionChanged()
{
    bool state = mListView->selectionModel()->selectedRows().count();
    mDeleteButton->setEnabled(state);
    mEditButton->setEnabled(state);
}

/******************************************************************************
* Initialise the button and context menu actions.
*/
void ResourceSelector::initActions(KActionCollection* actions)
{
    mActionReload      = new QAction(QIcon::fromTheme(QLatin1String("view-refresh")), i18nc("@action Reload calendar", "Re&load"), this);
    actions->addAction(QLatin1String("resReload"), mActionReload);
    connect(mActionReload, SIGNAL(triggered(bool)), SLOT(reloadResource()));
    mActionShowDetails = new QAction(QIcon::fromTheme(QLatin1String("help-about")), i18nc("@action", "Show &Details"), this);
    actions->addAction(QLatin1String("resDetails"), mActionShowDetails);
    connect(mActionShowDetails, SIGNAL(triggered(bool)), SLOT(showInfo()));
    mActionSetColour   = new QAction(QIcon::fromTheme(QLatin1String("color-picker")), i18nc("@action", "Set &Color..."), this);
    actions->addAction(QLatin1String("resSetColour"), mActionSetColour);
    connect(mActionSetColour, SIGNAL(triggered(bool)), SLOT(setColour()));
    mActionClearColour   = new QAction(i18nc("@action", "Clear C&olor"), this);
    actions->addAction(QLatin1String("resClearColour"), mActionClearColour);
    connect(mActionClearColour, SIGNAL(triggered(bool)), SLOT(clearColour()));
    mActionEdit        = new QAction(QIcon::fromTheme(QLatin1String("document-properties")), i18nc("@action", "&Edit..."), this);
    actions->addAction(QLatin1String("resEdit"), mActionEdit);
    connect(mActionEdit, SIGNAL(triggered(bool)), SLOT(editResource()));
    mActionUpdate      = new QAction(i18nc("@action", "&Update Calendar Format"), this);
    actions->addAction(QLatin1String("resUpdate"), mActionUpdate);
    connect(mActionUpdate, SIGNAL(triggered(bool)), SLOT(updateResource()));
    mActionRemove      = new QAction(QIcon::fromTheme(QLatin1String("edit-delete")), i18nc("@action", "&Remove"), this);
    actions->addAction(QLatin1String("resRemove"), mActionRemove);
    connect(mActionRemove, SIGNAL(triggered(bool)), SLOT(removeResource()));
    mActionSetDefault  = new KToggleAction(this);
    actions->addAction(QLatin1String("resDefault"), mActionSetDefault);
    connect(mActionSetDefault, SIGNAL(triggered(bool)), SLOT(setStandard()));
    QAction* action    = new QAction(QIcon::fromTheme(QLatin1String("document-new")), i18nc("@action", "&Add..."), this);
    actions->addAction(QLatin1String("resAdd"), action);
    connect(action, SIGNAL(triggered(bool)), SLOT(addResource()));
    mActionImport      = new QAction(i18nc("@action", "Im&port..."), this);
    actions->addAction(QLatin1String("resImport"), mActionImport);
    connect(mActionImport, SIGNAL(triggered(bool)), SLOT(importCalendar()));
    mActionExport      = new QAction(i18nc("@action", "E&xport..."), this);
    actions->addAction(QLatin1String("resExport"), mActionExport);
    connect(mActionExport, SIGNAL(triggered(bool)), SLOT(exportCalendar()));
}

void ResourceSelector::setContextMenu(QMenu* menu)
{
    mContextMenu = menu;
}

/******************************************************************************
* Display the context menu for the selected calendar.
*/
void ResourceSelector::contextMenuRequested(const QPoint& viewportPos)
{
    if (!mContextMenu)
        return;
    bool active    = false;
    bool writable  = false;
    bool updatable = false;
    Collection collection;
    if (mListView->selectionModel()->hasSelection())
    {
        QModelIndex index = mListView->indexAt(viewportPos);
        if (index.isValid())
            collection = mListView->collectionModel()->collection(index);
        else
            mListView->clearSelection();
    }
    CalEvent::Type type = currentResourceType();
    bool haveCalendar = collection.isValid();
    if (haveCalendar)
    {
        // Note: the CollectionControlModel functions call AkonadiModel::refresh(collection)
        active   = CollectionControlModel::isEnabled(collection, type);
        KACalendar::Compat compatibility;
        int rw = CollectionControlModel::isWritableEnabled(collection, type, compatibility);
        writable = (rw > 0);
        if (!rw
        &&  (compatibility & ~KACalendar::Converted)
        &&  !(compatibility & ~(KACalendar::Convertible | KACalendar::Converted)))
            updatable = true; // the calendar format is convertible to the current KAlarm format
        if (!(AkonadiModel::instance()->types(collection) & type))
            type = CalEvent::EMPTY;
    }
    mActionReload->setEnabled(active);
    mActionShowDetails->setEnabled(haveCalendar);
    mActionSetColour->setEnabled(haveCalendar);
    mActionClearColour->setEnabled(haveCalendar);
    mActionClearColour->setVisible(AkonadiModel::instance()->backgroundColor(collection).isValid());
    mActionEdit->setEnabled(haveCalendar);
    mActionUpdate->setEnabled(updatable);
    mActionRemove->setEnabled(haveCalendar);
    mActionImport->setEnabled(active && writable);
    mActionExport->setEnabled(active);
    QString text;
    switch (type)
    {
        case CalEvent::ACTIVE:   text = i18nc("@action", "Use as &Default for Active Alarms");  break;
        case CalEvent::ARCHIVED: text = i18nc("@action", "Use as &Default for Archived Alarms");  break;
        case CalEvent::TEMPLATE: text = i18nc("@action", "Use as &Default for Alarm Templates");  break;
        default:  break;
    }
    mActionSetDefault->setText(text);
    bool standard = CollectionControlModel::isStandard(collection, type);
    mActionSetDefault->setChecked(active && writable && standard);
    mActionSetDefault->setEnabled(active && writable);
    mContextMenu->popup(mListView->viewport()->mapToGlobal(viewportPos));
}

/******************************************************************************
* Called from the context menu to reload the selected resource.
*/
void ResourceSelector::reloadResource()
{
    Collection collection = currentResource();
    if (collection.isValid())
        AkonadiModel::instance()->reloadCollection(collection);
}

/******************************************************************************
* Called from the context menu to save the selected resource.
*/
void ResourceSelector::saveResource()
{
    // Save resource is not applicable to Akonadi
}

/******************************************************************************
* Called when the length of time archived alarms are to be stored changes.
* If expired alarms are now to be stored, set any single archived alarm
* resource to be the default.
*/
void ResourceSelector::archiveDaysChanged(int days)
{
    if (days)
    {
        if (!CollectionControlModel::getStandard(CalEvent::ARCHIVED).isValid())
        {
            Collection::List cols = CollectionControlModel::enabledCollections(CalEvent::ARCHIVED, true);
            if (cols.count() == 1)
            {
                CollectionControlModel::setStandard(cols[0], CalEvent::ARCHIVED);
                theApp()->purgeNewArchivedDefault(cols[0]);
            }
        }
    }
}

/******************************************************************************
* Called from the context menu to set the selected resource as the default
* for its alarm type. The resource is automatically made active.
*/
void ResourceSelector::setStandard()
{
    Collection collection = currentResource();
    if (collection.isValid())
    {
        CalEvent::Type alarmType = currentResourceType();
        bool standard = mActionSetDefault->isChecked();
        if (standard)
            CollectionControlModel::setEnabled(collection, alarmType, true);
        CollectionControlModel::setStandard(collection, alarmType, standard);
        if (alarmType == CalEvent::ARCHIVED)
            theApp()->purgeNewArchivedDefault(collection);
    }
}


/******************************************************************************
* Called from the context menu to merge alarms from an external calendar into
* the selected resource (if any).
*/
void ResourceSelector::importCalendar()
{
    Collection collection = currentResource();
    AlarmCalendar::importAlarms(this, (collection.isValid() ? &collection : 0));
}

/******************************************************************************
* Called from the context menu to copy the selected resource's alarms to an
* external calendar.
*/
void ResourceSelector::exportCalendar()
{
    Collection calendar = currentResource();
    if (calendar.isValid())
        AlarmCalendar::exportAlarms(AlarmCalendar::resources()->events(calendar), this);
}

/******************************************************************************
* Called from the context menu to set a colour for the selected resource.
*/
void ResourceSelector::setColour()
{
    Collection collection = currentResource();
    if (collection.isValid())
    {
        QColor colour = AkonadiModel::instance()->backgroundColor(collection);
        if (!colour.isValid())
            colour = QApplication::palette().color(QPalette::Base);
        if (KColorDialog::getColor(colour, QColor(), this) == KColorDialog::Accepted)
            AkonadiModel::instance()->setBackgroundColor(collection, colour);
    }
}

/******************************************************************************
* Called from the context menu to clear the display colour for the selected
* resource.
*/
void ResourceSelector::clearColour()
{
    Collection collection = currentResource();
    if (collection.isValid())
        AkonadiModel::instance()->setBackgroundColor(collection, QColor());
}

/******************************************************************************
* Called from the context menu to display information for the selected resource.
*/
void ResourceSelector::showInfo()
{
    Collection collection = currentResource();
    if (collection.isValid())
    {
        const QString name = collection.displayName();
        QString id = collection.resource();   // resource name
        CalEvent::Type alarmType = currentResourceType();
        QString calType = AgentManager::self()->instance(id).type().name();
        QString storage = AkonadiModel::instance()->storageType(collection);
        QString location = collection.remoteId();
        KUrl url(location);
        if (url.isLocalFile())
            location = url.path();
        CalEvent::Types altypes = AkonadiModel::instance()->types(collection);
        QStringList alarmTypes;
        if (altypes & CalEvent::ACTIVE)
            alarmTypes << i18nc("@info/plain", "Active alarms");
        if (altypes & CalEvent::ARCHIVED)
            alarmTypes << i18nc("@info/plain", "Archived alarms");
        if (altypes & CalEvent::TEMPLATE)
            alarmTypes << i18nc("@info/plain", "Alarm templates");
        QString alarmTypeString = alarmTypes.join(i18nc("@info/plain List separator", ", "));
        KACalendar::Compat compat;
        QString perms = AkonadiModel::readOnlyTooltip(collection);
        if (perms.isEmpty())
            perms = i18nc("@info/plain", "Read-write");
        QString enabled = CollectionControlModel::isEnabled(collection, alarmType)
                    ? i18nc("@info/plain", "Enabled")
                    : i18nc("@info/plain", "Disabled");
        QString std = CollectionControlModel::isStandard(collection, alarmType)
                    ? i18nc("@info/plain Parameter in 'Default calendar: Yes/No'", "Yes")
                    : i18nc("@info/plain Parameter in 'Default calendar: Yes/No'", "No");
        QString text = xi18nc("@info",
                             "<title>%1</title>"
                             "<para>ID: %2<nl/>"
                             "Calendar type: %3<nl/>"
                             "Contents: %4<nl/>"
                             "%5: <filename>%6</filename><nl/>"
                             "Permissions: %7<nl/>"
                             "Status: %8<nl/>"
                             "Default calendar: %9</para>",
                             name, id, calType, alarmTypeString, storage, location, perms, enabled, std);
        // Display the collection information. Because the user requested
        // the information, don't raise a KNotify event.
        KAMessageBox::information(this, text, QString(), QString(), 0);
    }
}

/******************************************************************************
* Return the currently selected resource in the list.
*/
Collection ResourceSelector::currentResource() const
{
    return mListView->collection(mListView->selectionModel()->currentIndex());
}

/******************************************************************************
* Return the currently selected resource type.
*/
CalEvent::Type ResourceSelector::currentResourceType() const
{
    switch (mAlarmType->currentIndex())
    {
        case 0:  return CalEvent::ACTIVE;
        case 1:  return CalEvent::ARCHIVED;
        case 2:  return CalEvent::TEMPLATE;
        default:  return CalEvent::EMPTY;
    }
}

void ResourceSelector::resizeEvent(QResizeEvent* re)
{
    emit resized(re->oldSize(), re->size());
}

// vim: et sw=4:
