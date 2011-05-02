/*
 *  resourceselector.cpp  -  calendar resource selection widget
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

#include "resourceselector.moc"

#include "kalarm.h"
#include "alarmcalendar.h"
#include "autoqpointer.h"
#ifndef USE_AKONADI
#include "alarmresources.h"
#include "eventlistmodel.h"
#include "resourcemodelview.h"
#endif
#include "packedlayout.h"
#include "preferences.h"
#include "resourceconfigdialog.h"

#ifdef USE_AKONADI
#include <akonadi/agentmanager.h>
#include <akonadi/agentinstancecreatejob.h>
#include <akonadi/agenttype.h>
#include <akonadi/collectionpropertiesdialog.h>
#include <akonadi/entitydisplayattribute.h>
#else
#include <kcal/resourcecalendar.h>
#endif

#include <kdialog.h>
#include <klocale.h>
#include <kglobal.h>
#include <kmessagebox.h>
#include <kcombobox.h>
#include <kinputdialog.h>
#include <kmenu.h>
#include <kdebug.h>
#include <kicon.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <ktoggleaction.h>
#include <kcolordialog.h>

#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QPainter>
#include <QFont>
#include <QResizeEvent>
#include <QApplication>

#ifdef USE_AKONADI
using namespace KCalCore;
#else
using namespace KCal;
#endif
#ifdef USE_AKONADI
using namespace Akonadi;
#endif


#ifdef USE_AKONADI
ResourceSelector::ResourceSelector(QWidget* parent)
#else
ResourceSelector::ResourceSelector(AlarmResources* calendar, QWidget* parent)
#endif
    : QFrame(parent),
#ifndef USE_AKONADI
      mCalendar(calendar),
#endif
      mContextMenu(0)
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

#ifdef USE_AKONADI
    CollectionFilterCheckListModel* model = new CollectionFilterCheckListModel(this);
    mListView = new CollectionView(model, this);
#else
    ResourceModel* model = ResourceModel::instance();
    ResourceFilterModel* filterModel = new ResourceFilterModel(model, this);
    mListView = new ResourceView(this);
    mListView->setModel(filterModel);
#endif
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), SLOT(selectionChanged()));
    mListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mListView, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(contextMenuRequested(const QPoint&)));
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
    mDeleteButton->setWhatsThis(i18nc("@info:whatsthis", "<para>Remove the highlighted calendar from the list.</para>"
                                     "<para>The calendar itself is left intact, and may subsequently be reinstated in the list if desired.</para>"));
    mEditButton->setDisabled(true);
    mDeleteButton->setDisabled(true);
    connect(mAddButton, SIGNAL(clicked()), SLOT(addResource()));
    connect(mEditButton, SIGNAL(clicked()), SLOT(editResource()));
    connect(mDeleteButton, SIGNAL(clicked()), SLOT(removeResource()));

#ifdef USE_AKONADI
    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant&)),
                                      SLOT(slotStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant&)));
    connect(AkonadiModel::instance(), SIGNAL(collectionAdded(Akonadi::AgentInstanceCreateJob*, bool)),
                                      SLOT(resourceAdded(Akonadi::AgentInstanceCreateJob*, bool)));
    connect(AkonadiModel::instance(), SIGNAL(collectionAdded(const Akonadi::Collection&)),
                                      SLOT(slotCollectionAdded(const Akonadi::Collection&)));
#else
    connect(mCalendar, SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotStatusChanged(AlarmResource*, AlarmResources::Change)));
#endif

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
            mCurrentAlarmType = KAlarm::CalEvent::ACTIVE;
            addTip = i18nc("@info:tooltip", "Add a new active alarm calendar");
            break;
        case 1:
            mCurrentAlarmType = KAlarm::CalEvent::ARCHIVED;
            addTip = i18nc("@info:tooltip", "Add a new archived alarm calendar");
            break;
        case 2:
            mCurrentAlarmType = KAlarm::CalEvent::TEMPLATE;
            addTip = i18nc("@info:tooltip", "Add a new alarm template calendar");
            break;
    }
    // WORKAROUND: Switch scroll bars off to avoid crash (see explanation
    // in reinstateAlarmTypeScrollBars() description).
    mListView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#ifdef USE_AKONADI
    mListView->collectionModel()->setEventTypeFilter(mCurrentAlarmType);
#else
    static_cast<ResourceFilterModel*>(mListView->model())->setFilter(mCurrentAlarmType);
#endif
    mAddButton->setWhatsThis(addTip);
    mAddButton->setToolTip(addTip);
    // WORKAROUND: Switch scroll bars back on after allowing geometry to update ...
    QTimer::singleShot(0, this, SLOT(reinstateAlarmTypeScrollBars()));
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
#ifdef USE_AKONADI
    AgentInstanceCreateJob* job = AkonadiModel::instance()->addCollection(mCurrentAlarmType, this);
    if (job)
        mAddJobs += job;
#else
    AlarmResourceManager* manager = mCalendar->resourceManager();
    QStringList descs = manager->resourceTypeDescriptions();
    bool ok = false;
    QString desc = KInputDialog::getItem(i18nc("@title:window", "Calendar Configuration"),
                                         i18nc("@info", "Select storage type of new calendar:"), descs, 0, false, &ok, this);
    if (!ok  ||  descs.isEmpty())
        return;
    QString type = manager->resourceTypeNames()[descs.indexOf(desc)];
    AlarmResource* resource = dynamic_cast<AlarmResource*>(manager->createResource(type));
    if (!resource)
    {
        KMessageBox::error(this, i18nc("@info", "Unable to create calendar of type <resource>%1</resource>.", type));
        return;
    }
    resource->setResourceName(i18nc("@info/plain", "%1 calendar", type));
    resource->setAlarmType(mCurrentAlarmType);
    resource->setActive(false);   // prevent setReadOnly() declaring it as unwritable before we've tried to load it

    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of ResourceSelector, and on return from this function).
    AutoQPointer<ResourceConfigDialog> dlg = new ResourceConfigDialog(this, resource);
    if (dlg->exec() == QDialog::Accepted)
    {
        resource->setEnabled(true);
        resource->setTimeSpec(Preferences::timeZone());
        manager->add(resource);
        manager->writeConfig();
        mCalendar->resourceAdded(resource);   // load the resource and connect in-process change signals
    }
    else
    {
        delete resource;
        resource = 0;
    }
#endif
}

#ifdef USE_AKONADI
/******************************************************************************
* Called when the job started by AkonadiModel::addCollection() has completed.
*/
void ResourceSelector::resourceAdded(AgentInstanceCreateJob* job, bool success)
{
    int i = mAddJobs.indexOf(job);
    if (i >= 0)
    {
        // The agent has been created by addResource().
        if (success)
        {
            AgentInstance agent = job->instance();
            if (agent.isValid())
            {
                // Note that we're expecting the agent's Collection to be added
                mAddAgents += agent;
            }
        }
        mAddJobs.removeAt(i);
    }
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
                KAlarm::CalEvent::Types types = KAlarm::CalEvent::types(collection.contentMimeTypes());
                CollectionControlModel::setEnabled(collection, types, true);
#ifdef __GNUC__
#warning Display collection list for one of the selected alarm types?
#endif
                mAddAgents.removeAt(i);
            }
        }
    }
}
#endif

/******************************************************************************
* Edit the currently selected resource.
*/
void ResourceSelector::editResource()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (collection.isValid())
    {
        AgentInstance instance = AgentManager::self()->instance(collection.resource());
        if (instance.isValid())
            instance.configure(this);
    }
#else
    AlarmResource* resource = currentResource();
    if (!resource)
        return;
    bool readOnly = resource->readOnly();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of ResourceSelector, and on return from this function).
    AutoQPointer<ResourceConfigDialog> dlg = new ResourceConfigDialog(this, resource);
    if (dlg->exec() == QDialog::Accepted)
    {
        // Act on any changed settings.
        // Read-only is handled automatically by AlarmResource::setReadOnly().
        if (!readOnly  &&  resource->readOnly()  &&  resource->standardResource())
        {
            // A standard resource is being made read-only.
            if (resource->alarmType() == KAlarm::CalEvent::ACTIVE)
            {
                KMessageBox::sorry(this, i18nc("@info", "You cannot make your default active alarm calendar read-only."));
                resource->setReadOnly(false);
            }
            else if (resource->alarmType() == KAlarm::CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
            {
                // Only allow the archived alarms standard resource to be made read-only
                // if we're not saving archived alarms.
                KMessageBox::sorry(this, i18nc("@info", "You cannot make your default archived alarm calendar "
                                              "read-only while expired alarms are configured to be kept."));
                resource->setReadOnly(false);
            }
            else if (KMessageBox::warningContinueCancel(this, i18nc("@info", "Do you really want to make your default calendar read-only?"))
                       == KMessageBox::Cancel)
            {
                resource->setReadOnly(false);
            }
        }
    }
#endif
}

/******************************************************************************
* Remove the currently selected resource from the displayed list.
*/
void ResourceSelector::removeResource()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (!collection.isValid())
        return;
    QString name = collection.name();
    // Check if it's the standard or only resource for at least one type.
    KAlarm::CalEvent::Types standardTypes = CollectionControlModel::standardTypes(collection, true);
    bool std = standardTypes & KAlarm::CalEvent::ALL;
    KAlarm::CalEvent::Type stdType = (standardTypes & KAlarm::CalEvent::ACTIVE)   ? KAlarm::CalEvent::ACTIVE
                                   : (standardTypes & KAlarm::CalEvent::ARCHIVED) ? KAlarm::CalEvent::ARCHIVED
                                   : KAlarm::CalEvent::EMPTY;
#else
    AlarmResource* resource = currentResource();
    if (!resource)
        return;
    QString name = resource->resourceName();
    bool std = resource->standardResource();
    // Check if it's the standard resource for its type.
    KAlarm::CalEvent::Type stdType = std ? resource->alarmType() : KAlarm::CalEvent::EMPTY;
#endif
    if (stdType == KAlarm::CalEvent::ACTIVE)
    {
        KMessageBox::sorry(this, i18nc("@info", "You cannot remove your default active alarm calendar."));
        return;
    }
    if (stdType == KAlarm::CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
    {
        // Only allow the archived alarms standard resource to be removed if
        // we're not saving archived alarms.
        KMessageBox::sorry(this, i18nc("@info", "You cannot remove your default archived alarm calendar "
                                      "while expired alarms are configured to be kept."));
        return;
    }
#ifdef __GNUC__
#warning Ensure default calendars are identified sufficiently (Akonadi only)
#endif
    QString text = std ? i18nc("@info", "Do you really want to remove your default calendar (<resource>%1</resource>) from the list?", name)
                       : i18nc("@info", "Do you really want to remove the calendar <resource>%1</resource> from the list?", name);
    if (KMessageBox::warningContinueCancel(this, text, "", KStandardGuiItem::remove()) == KMessageBox::Cancel)
        return;

#ifdef USE_AKONADI
    AkonadiModel::instance()->removeCollection(collection);
#else
    // Remove resource from alarm and resource lists before deleting it, to avoid
    // crashes when display updates occur immediately after it is deleted.
    if (resource->alarmType() == KAlarm::CalEvent::TEMPLATE)
        EventListModel::templates()->removeResource(resource);
    else
        EventListModel::alarms()->removeResource(resource);
    ResourceModel::instance()->removeResource(resource);
    AlarmResourceManager* manager = mCalendar->resourceManager();
    manager->remove(resource);
    manager->writeConfig();
#endif
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
    mActionReload      = new KAction(KIcon("view-refresh"), i18nc("@action Reload calendar", "Re&load"), this);
    actions->addAction(QLatin1String("resReload"), mActionReload);
    connect(mActionReload, SIGNAL(triggered(bool)), SLOT(reloadResource()));
    mActionSave        = new KAction(KIcon("document-save"), i18nc("@action", "&Save"), this);
    actions->addAction(QLatin1String("resSave"), mActionSave);
    connect(mActionSave, SIGNAL(triggered(bool)), SLOT(saveResource()));
    mActionShowDetails = new KAction(KIcon("help-about"), i18nc("@action", "Show &Details"), this);
    actions->addAction(QLatin1String("resDetails"), mActionShowDetails);
    connect(mActionShowDetails, SIGNAL(triggered(bool)), SLOT(showInfo()));
    mActionSetColour   = new KAction(KIcon("color-picker"), i18nc("@action", "Set &Color..."), this);
    actions->addAction(QLatin1String("resSetColour"), mActionSetColour);
    connect(mActionSetColour, SIGNAL(triggered(bool)), SLOT(setColour()));
    mActionClearColour   = new KAction(i18nc("@action", "Clear C&olor"), this);
    actions->addAction(QLatin1String("resClearColour"), mActionClearColour);
    connect(mActionClearColour, SIGNAL(triggered(bool)), SLOT(clearColour()));
    mActionEdit        = new KAction(KIcon("document-properties"), i18nc("@action", "&Edit..."), this);
    actions->addAction(QLatin1String("resEdit"), mActionEdit);
    connect(mActionEdit, SIGNAL(triggered(bool)), SLOT(editResource()));
    mActionRemove      = new KAction(KIcon("edit-delete"), i18nc("@action", "&Remove"), this);
    actions->addAction(QLatin1String("resRemove"), mActionRemove);
    connect(mActionRemove, SIGNAL(triggered(bool)), SLOT(removeResource()));
    mActionSetDefault  = new KToggleAction(this);
    actions->addAction(QLatin1String("resDefault"), mActionSetDefault);
    connect(mActionSetDefault, SIGNAL(triggered(bool)), SLOT(setStandard()));
    QAction* action    = new KAction(KIcon("document-new"), i18nc("@action", "&Add..."), this);
    actions->addAction(QLatin1String("resAdd"), action);
    connect(action, SIGNAL(triggered(bool)), SLOT(addResource()));
    mActionImport      = new KAction(i18nc("@action", "Im&port..."), this);
    actions->addAction(QLatin1String("resImport"), mActionImport);
    connect(mActionImport, SIGNAL(triggered(bool)), SLOT(importCalendar()));
    mActionExport      = new KAction(i18nc("@action", "E&xport..."), this);
    actions->addAction(QLatin1String("resExport"), mActionExport);
    connect(mActionExport, SIGNAL(triggered(bool)), SLOT(exportCalendar()));
}

void ResourceSelector::setContextMenu(KMenu* menu)
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
    bool active   = false;
    bool writable = false;
#ifdef USE_AKONADI
    Collection collection;
#else
    AlarmResource* resource = 0;
#endif
    if (mListView->selectionModel()->hasSelection())
    {
        QModelIndex index = mListView->indexAt(viewportPos);
        if (index.isValid())
#ifdef USE_AKONADI
            collection = mListView->collectionModel()->collection(index);
#else
            resource = static_cast<ResourceFilterModel*>(mListView->model())->resource(index);
#endif
        else
            mListView->clearSelection();
    }
    KAlarm::CalEvent::Type type = currentResourceType();
#ifdef USE_AKONADI
    bool haveCalendar = collection.isValid();
#else
    bool haveCalendar = resource;
#endif
    if (haveCalendar)
    {
#ifdef USE_AKONADI
        active   = CollectionControlModel::isEnabled(collection, type);
        writable = CollectionControlModel::isWritable(collection, type);
        if (!(AkonadiModel::instance()->types(collection) & type))
            type = KAlarm::CalEvent::EMPTY;
#else
        active   = resource->isEnabled();
        type     = resource->alarmType();
        writable = resource->writable();
#endif
    }
    mActionReload->setEnabled(active);
    mActionSave->setEnabled(active && writable);
    mActionShowDetails->setEnabled(haveCalendar);
    mActionSetColour->setEnabled(haveCalendar);
    mActionClearColour->setEnabled(haveCalendar);
#ifdef USE_AKONADI
    mActionClearColour->setVisible(AkonadiModel::instance()->backgroundColor(collection).isValid());
#else
    mActionClearColour->setVisible(resource && resource->colour().isValid());
#endif
    mActionEdit->setEnabled(haveCalendar);
    mActionRemove->setEnabled(haveCalendar);
    mActionImport->setEnabled(active && writable);
    mActionExport->setEnabled(active);
    QString text;
    switch (type)
    {
        case KAlarm::CalEvent::ACTIVE:   text = i18nc("@action", "Use as &Default for Active Alarms");  break;
        case KAlarm::CalEvent::ARCHIVED: text = i18nc("@action", "Use as &Default for Archived Alarms");  break;
        case KAlarm::CalEvent::TEMPLATE: text = i18nc("@action", "Use as &Default for Alarm Templates");  break;
        default:  break;
    }
    mActionSetDefault->setText(text);
#ifdef USE_AKONADI
    bool standard = CollectionControlModel::isStandard(collection, type);
#else
    bool standard = (resource  &&  resource == mCalendar->getStandardResource(static_cast<KAlarm::CalEvent::Type>(type))  &&  resource->standardResource());
#endif
    mActionSetDefault->setChecked(active && writable && standard);
    mActionSetDefault->setEnabled(active && writable);
    mContextMenu->popup(mListView->viewport()->mapToGlobal(viewportPos));
}

/******************************************************************************
* Called from the context menu to reload the selected resource.
*/
void ResourceSelector::reloadResource()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (collection.isValid())
        AkonadiModel::instance()->reloadCollection(collection);
#else
    AlarmResource* resource = currentResource();
    if (resource)
        AlarmCalendar::resources()->loadResource(resource, this);
#endif
}

/******************************************************************************
* Called from the context menu to save the selected resource.
*/
void ResourceSelector::saveResource()
{
#ifdef USE_AKONADI
#ifdef __GNUC__
#warning saveResource() not implemented
#endif
#else
    AlarmResource* resource = currentResource();
    if (resource)
        resource->save();
#endif
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
#ifdef USE_AKONADI
        if (!CollectionControlModel::getStandard(KAlarm::CalEvent::ARCHIVED).isValid())
        {
            Collection::List cols = CollectionControlModel::enabledCollections(KAlarm::CalEvent::ARCHIVED, true);
            if (cols.count() == 1)
            CollectionControlModel::setStandard(cols[1], KAlarm::CalEvent::ARCHIVED);
        }
#else
        AlarmResources* resources = AlarmResources::instance();
        AlarmResource* std = resources->getStandardResource(KAlarm::CalEvent::ARCHIVED);
        if (std  &&  !std->standardResource())
            resources->setStandardResource(std);
#endif
    }
}

/******************************************************************************
* Called from the context menu to set the selected resource as the default
* for its alarm type. The resource is automatically made active.
*/
void ResourceSelector::setStandard()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (collection.isValid())
    {
        KAlarm::CalEvent::Type alarmType = currentResourceType();
        bool standard = mActionSetDefault->isChecked();
        if (standard)
            CollectionControlModel::setEnabled(collection, alarmType, true);
        CollectionControlModel::setStandard(collection, alarmType, standard);
    }
#else
    AlarmResource* resource = currentResource();
    if (resource)
    {
        if (mActionSetDefault->isChecked())
        {
            resource->setEnabled(true);
            mCalendar->setStandardResource(resource);
        }
        else
            resource->setStandardResource(false);
    }
#endif
}

/******************************************************************************
* Called when a calendar status has changed.
*/
#ifdef USE_AKONADI
void ResourceSelector::slotStatusChanged(const Collection& collection, AkonadiModel::Change change, const QVariant& value)
#else
void ResourceSelector::slotStatusChanged(AlarmResource* resource, AlarmResources::Change change)
#endif
{
#ifndef USE_AKONADI
    if (change == AlarmResources::WrongType  &&  resource->isWrongAlarmType())
    {
        QString text;
        switch (resource->alarmType())
        {
            case KAlarm::CalEvent::ACTIVE:
                text = i18nc("@info/plain", "It is not an active alarm calendar.");
                break;
            case KAlarm::CalEvent::ARCHIVED:
                text = i18nc("@info/plain", "It is not an archived alarm calendar.");
                break;
            case KAlarm::CalEvent::TEMPLATE:
                text = i18nc("@info/plain", "It is not an alarm template calendar.");
                break;
            default:
                return;
        }
        KMessageBox::sorry(this, i18nc("@info", "<para>Calendar <resource>%1</resource> has been disabled:</para><para>%2</para>", resource->resourceName(), text));
    }
#endif
}

/******************************************************************************
* Called from the context menu to merge alarms from an external calendar into
* the selected resource (if any).
*/
void ResourceSelector::importCalendar()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    AlarmCalendar::importAlarms(this, (collection.isValid() ? &collection : 0));
#else
    AlarmCalendar::importAlarms(this, currentResource());
#endif
}

/******************************************************************************
* Called from the context menu to copy the selected resource's alarms to an
* external calendar.
*/
void ResourceSelector::exportCalendar()
{
#ifdef USE_AKONADI
    Collection calendar = currentResource();
    if (calendar.isValid())
#else
    AlarmResource* calendar = currentResource();
    if (calendar)
#endif
        AlarmCalendar::exportAlarms(AlarmCalendar::resources()->events(calendar), this);
}

/******************************************************************************
* Called from the context menu to set a colour for the selected resource.
*/
void ResourceSelector::setColour()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (collection.isValid())
    {
        QColor colour = AkonadiModel::instance()->backgroundColor(collection);
        if (!colour.isValid())
            colour = QApplication::palette().color(QPalette::Base);
        if (KColorDialog::getColor(colour, QColor(), this) == KColorDialog::Accepted)
            AkonadiModel::instance()->setBackgroundColor(collection, colour);
    }
#else
    AlarmResource* resource = currentResource();
    if (resource)
    {
        QColor colour = resource->colour();
        if (!colour.isValid())
            colour = QApplication::palette().color(QPalette::Base);
        if (KColorDialog::getColor(colour, QColor(), this) == KColorDialog::Accepted)
            resource->setColour(colour);
    }
#endif
}

/******************************************************************************
* Called from the context menu to clear the display colour for the selected
* resource.
*/
void ResourceSelector::clearColour()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (collection.isValid())
        AkonadiModel::instance()->setBackgroundColor(collection, QColor());
#else
    AlarmResource* resource = currentResource();
    if (resource)
        resource->setColour(QColor());
#endif
}

/******************************************************************************
* Called from the context menu to display information for the selected resource.
*/
void ResourceSelector::showInfo()
{
#ifdef USE_AKONADI
    Collection collection = currentResource();
    if (collection.isValid())
    {
        QString id = collection.name();
        QString name;
        if (collection.hasAttribute<EntityDisplayAttribute>())
            name = collection.attribute<EntityDisplayAttribute>()->displayName();
        KAlarm::CalEvent::Type alarmType = currentResourceType();
        QString calType = AgentManager::self()->instance(collection.resource()).type().name();
        QString storage = AkonadiModel::instance()->storageType(collection);
        QString location = collection.remoteId();
        KUrl url(location);
        if (url.isLocalFile())
            location = url.path();
        QString perms = CollectionControlModel::isWritable(collection, alarmType, true)
                  ? i18nc("@info/plain", "Read-write")
                  : i18nc("@info/plain", "Read-only");
bool wrongAlarmType = false;  //(applies only to resourcelocaldir)
        QString enabled = CollectionControlModel::isEnabled(collection, alarmType)
                    ? i18nc("@info/plain", "Enabled")
                : wrongAlarmType ? i18nc("@info/plain", "Disabled (wrong alarm type)")
                : i18nc("@info/plain", "Disabled");
        QString std = CollectionControlModel::isStandard(collection, alarmType)
                ? i18nc("@info/plain Parameter in 'Default calendar: Yes/No'", "Yes")
                : i18nc("@info/plain Parameter in 'Default calendar: Yes/No'", "No");
        QString text = (name.isEmpty() || name == id)
                     ? i18nc("@info",
                             "<title>%1</title>"
                             "Contents: %2<nl/>"
                             "%3: <filename>%4</filename><nl/>"
                             "Permissions: %5<nl/>"
                             "Status: %6<nl/>"
                             "Default calendar: %7</para>",
                             id, calType, storage, location, perms, enabled, std)
                     : i18nc("@info",
                             "<title>%1</title>"
                             "<para>ID: %2<nl/>"
                             "Contents: %3<nl/>"
                             "%4: <filename>%5</filename><nl/>"
                             "Permissions: %6<nl/>"
                             "Status: %7<nl/>"
                             "Default calendar: %8</para>",
                             name, id, calType, storage, location, perms, enabled, std);
        // Display the collection information. Because the user requested
        // the information, don't raise a KNotify event.
        KMessageBox::information(this, text, QString(), QString(), 0);
    }
#else
    AlarmResource* resource = currentResource();
    if (resource)
    {
        // Display the collection information. Because the user requested
        // the information, don't raise a KNotify event.
        KMessageBox::information(this, resource->infoText(), QString(), QString(), 0);
    }
#endif
}

/******************************************************************************
* Return the currently selected resource in the list.
*/
#ifdef USE_AKONADI
Collection ResourceSelector::currentResource() const
{
    return mListView->collection(mListView->selectionModel()->currentIndex());
}
#else
AlarmResource* ResourceSelector::currentResource() const
{
    return mListView->resource(mListView->selectionModel()->currentIndex());
}
#endif

/******************************************************************************
* Return the currently selected resource type.
*/
KAlarm::CalEvent::Type ResourceSelector::currentResourceType() const
{
    switch (mAlarmType->currentIndex())
    {
        case 0:  return KAlarm::CalEvent::ACTIVE;
        case 1:  return KAlarm::CalEvent::ARCHIVED;
        case 2:  return KAlarm::CalEvent::TEMPLATE;
        default:  return KAlarm::CalEvent::EMPTY;
    }
}

void ResourceSelector::resizeEvent(QResizeEvent* re)
{
    emit resized(re->oldSize(), re->size());
}

// vim: et sw=4:
