/*
 *  resourceselector.cpp  -  calendar resource selection widget
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

#include "resourceselector.h"

#include "alarmcalendar.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "resources/akonadidatamodel.h"
#include "resources/akonadiresourcecreator.h"
#include "resources/akonadiresourcemigrator.h"
#include "resources/datamodel.h"
#include "resources/resources.h"
#include "resources/resourcemodel.h"
#include "lib/autoqpointer.h"
#include "lib/messagebox.h"
#include "lib/packedlayout.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KActionCollection>
#include <KToggleAction>

#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QResizeEvent>
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QMenu>

using namespace KCalendarCore;


ResourceSelector::ResourceSelector(QWidget* parent)
    : QFrame(parent)
{
    QBoxLayout* topLayout = new QVBoxLayout(this);

    QLabel* label = new QLabel(i18nc("@title:group", "Calendars"), this);
    topLayout->addWidget(label, 0, Qt::AlignHCenter);

    mAlarmType = new QComboBox(this);
    mAlarmType->addItem(i18nc("@item:inlistbox", "Active Alarms"));
    mAlarmType->addItem(i18nc("@item:inlistbox", "Archived Alarms"));
    mAlarmType->addItem(i18nc("@item:inlistbox", "Alarm Templates"));
    mAlarmType->setFixedHeight(mAlarmType->sizeHint().height());
    mAlarmType->setWhatsThis(i18nc("@info:whatsthis", "Choose which type of data to show alarm calendars for"));
    topLayout->addWidget(mAlarmType);
    // No spacing between combo box and listview.

    ResourceFilterCheckListModel* model = DataModel::createResourceFilterCheckListModel(this);
    mListView = new ResourceView(model, this);
    connect(mListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ResourceSelector::selectionChanged);
    mListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mListView, &ResourceView::customContextMenuRequested, this, &ResourceSelector::contextMenuRequested);
    mListView->setWhatsThis(i18nc("@info:whatsthis",
                                  "List of available calendars of the selected type. The checked state shows whether a calendar "
                                 "is enabled (checked) or disabled (unchecked). The default calendar is shown in bold."));
    topLayout->addWidget(mListView, 1);

    PackedLayout* blayout = new PackedLayout(Qt::AlignHCenter);
    blayout->setContentsMargins(0, 0, 0, 0);
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
    connect(mAddButton, &QPushButton::clicked, this, &ResourceSelector::addResource);
    connect(mEditButton, &QPushButton::clicked, this, &ResourceSelector::editResource);
    connect(mDeleteButton, &QPushButton::clicked, this, &ResourceSelector::removeResource);

    connect(Resources::instance(), &Resources::resourceRemoved,
                             this, &ResourceSelector::selectionChanged);

    connect(mAlarmType, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &ResourceSelector::alarmTypeSelected);
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
    mListView->resourceModel()->setEventTypeFilter(mCurrentAlarmType);
    mAddButton->setWhatsThis(addTip);
    mAddButton->setToolTip(addTip);
    // WORKAROUND: Switch scroll bars back on after allowing geometry to update ...
    QTimer::singleShot(0, this, &ResourceSelector::reinstateAlarmTypeScrollBars);

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
    connect(creator, &AkonadiResourceCreator::resourceAdded, this, &ResourceSelector::slotResourceAdded);
    creator->createResource();
}

/******************************************************************************
* Called when a resource is added to the calendar data model, after being
* created by addResource().
*/
void ResourceSelector::slotResourceAdded(Resource& resource, CalEvent::Type alarmType)
{
    const CalEvent::Types types = resource.alarmTypes();
    resource.setEnabled(types);
    if (!(types & alarmType))
    {
        // The user has selected alarm types for the resource which don't
        // include the currently displayed type.
        // Show a resource list which includes a selected type.
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
}

/******************************************************************************
* Edit the currently selected resource.
*/
void ResourceSelector::editResource()
{
    currentResource().editResource(this);
}

/******************************************************************************
* Update the backend storage format for the currently selected resource in the
* displayed list.
*/
void ResourceSelector::updateResource()
{
    const Resource resource = currentResource();
    if (!resource.isValid())
        return;
    AkonadiResourceMigrator::updateToCurrentFormat(resource, true, this);
}

/******************************************************************************
* Remove the currently selected resource from the displayed list.
*/
void ResourceSelector::removeResource()
{
    Resource resource = currentResource();
    if (!resource.isValid())
        return;
    const QString name = resource.configName();
    // Check if it's the standard or only resource for at least one type.
    const CalEvent::Types allTypes      = resource.alarmTypes();
    const CalEvent::Types standardTypes = Resources::standardTypes(resource, true);
    const CalEvent::Type  currentType   = currentResourceType();
    const CalEvent::Type  stdType = (standardTypes & CalEvent::ACTIVE)   ? CalEvent::ACTIVE
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
            const QString stdTypes = ResourceDataModelBase::typeListForDisplay(standardTypes);
            QString otherTypes;
            const CalEvent::Types nonStandardTypes(allTypes & ~standardTypes);
            if (nonStandardTypes != currentType)
                otherTypes = xi18nc("@info", "<para>It also contains:%1</para>", ResourceDataModelBase::typeListForDisplay(nonStandardTypes));
            text = xi18nc("@info", "<para><resource>%1</resource> is the default calendar for:%2</para>%3"
                                  "<para>Do you really want to remove it from all calendar lists?</para>", name, stdTypes, otherTypes);
        }
        else
            text = xi18nc("@info", "Do you really want to remove your default calendar (<resource>%1</resource>) from the list?", name);
    }
    else if (allTypes != currentType)
        text = xi18nc("@info", "<para><resource>%1</resource> contains:%2</para><para>Do you really want to remove it from all calendar lists?</para>",
                     name, ResourceDataModelBase::typeListForDisplay(allTypes));
    else
        text = xi18nc("@info", "Do you really want to remove the calendar <resource>%1</resource> from the list?", name);
    if (KAMessageBox::warningContinueCancel(this, text, QString(), KStandardGuiItem::remove()) == KMessageBox::Cancel)
        return;

    resource.removeResource();
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
    mActionReload      = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action Reload calendar", "Re&load"), this);
    actions->addAction(QStringLiteral("resReload"), mActionReload);
    connect(mActionReload, &QAction::triggered, this, &ResourceSelector::reloadResource);
    mActionShowDetails = new QAction(QIcon::fromTheme(QStringLiteral("help-about")), i18nc("@action", "Show &Details"), this);
    actions->addAction(QStringLiteral("resDetails"), mActionShowDetails);
    connect(mActionShowDetails, &QAction::triggered, this, &ResourceSelector::showInfo);
    mActionSetColour   = new QAction(QIcon::fromTheme(QStringLiteral("color-picker")), i18nc("@action", "Set &Color..."), this);
    actions->addAction(QStringLiteral("resSetColour"), mActionSetColour);
    connect(mActionSetColour, &QAction::triggered, this, &ResourceSelector::setColour);
    mActionClearColour   = new QAction(i18nc("@action", "Clear C&olor"), this);
    actions->addAction(QStringLiteral("resClearColour"), mActionClearColour);
    connect(mActionClearColour, &QAction::triggered, this, &ResourceSelector::clearColour);
    mActionEdit        = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action", "&Edit..."), this);
    actions->addAction(QStringLiteral("resEdit"), mActionEdit);
    connect(mActionEdit, &QAction::triggered, this, &ResourceSelector::editResource);
    mActionUpdate      = new QAction(i18nc("@action", "&Update Calendar Format"), this);
    actions->addAction(QStringLiteral("resUpdate"), mActionUpdate);
    connect(mActionUpdate, &QAction::triggered, this, &ResourceSelector::updateResource);
    mActionRemove      = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action", "&Remove"), this);
    actions->addAction(QStringLiteral("resRemove"), mActionRemove);
    connect(mActionRemove, &QAction::triggered, this, &ResourceSelector::removeResource);
    mActionSetDefault  = new KToggleAction(this);
    actions->addAction(QStringLiteral("resDefault"), mActionSetDefault);
    connect(mActionSetDefault, &KToggleAction::triggered, this, &ResourceSelector::setStandard);
    QAction* action    = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), i18nc("@action", "&Add..."), this);
    actions->addAction(QStringLiteral("resAdd"), action);
    connect(action, &QAction::triggered, this, &ResourceSelector::addResource);
    mActionImport      = new QAction(i18nc("@action", "Im&port..."), this);
    actions->addAction(QStringLiteral("resImport"), mActionImport);
    connect(mActionImport, &QAction::triggered, this, &ResourceSelector::importCalendar);
    mActionExport      = new QAction(i18nc("@action", "E&xport..."), this);
    actions->addAction(QStringLiteral("resExport"), mActionExport);
    connect(mActionExport, &QAction::triggered, this, &ResourceSelector::exportCalendar);
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
    Resource resource;
    if (mListView->selectionModel()->hasSelection())
    {
        const QModelIndex index = mListView->indexAt(viewportPos);
        if (index.isValid())
            resource = mListView->resourceModel()->resource(index);
        else
            mListView->clearSelection();
    }
    CalEvent::Type type = currentResourceType();
    bool haveCalendar = resource.isValid();
    if (haveCalendar)
    {
        active = resource.isEnabled(type);
        const int rw = resource.writableStatus(type);
        writable = (rw > 0);
        const KACalendar::Compat compatibility = resource.compatibility();
        if (!rw
        &&  (compatibility & ~KACalendar::Converted)
        &&  !(compatibility & ~(KACalendar::Convertible | KACalendar::Converted)))
            updatable = true; // the calendar format is convertible to the current KAlarm format
        if (!(resource.alarmTypes() & type))
            type = CalEvent::EMPTY;
    }
    mActionReload->setEnabled(active);
    mActionShowDetails->setEnabled(haveCalendar);
    mActionSetColour->setEnabled(haveCalendar);
    mActionClearColour->setEnabled(haveCalendar);
    mActionClearColour->setVisible(resource.backgroundColour().isValid());
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
    bool standard = Resources::isStandard(resource, type);
    mActionSetDefault->setChecked(active && writable && standard);
    mActionSetDefault->setEnabled(active && writable);
    mContextMenu->popup(mListView->viewport()->mapToGlobal(viewportPos));
}

/******************************************************************************
* Called from the context menu to reload the selected resource.
*/
void ResourceSelector::reloadResource()
{
    Resource resource = currentResource();
    if (resource.isValid())
        DataModel::reload(resource);
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
* If expired alarms are now to be stored, this also sets any single archived
* alarm resource to be the default.
*/
void ResourceSelector::archiveDaysChanged(int days)
{
    if (days)
    {
        const Resource resource = Resources::getStandard(CalEvent::ARCHIVED);
        if (resource.isValid())
            theApp()->purgeNewArchivedDefault(resource);
    }
}

/******************************************************************************
* Called from the context menu to set the selected resource as the default
* for its alarm type. The resource is automatically made active.
*/
void ResourceSelector::setStandard()
{
    Resource resource = currentResource();
    if (resource.isValid())
    {
        CalEvent::Type alarmType = currentResourceType();
        bool standard = mActionSetDefault->isChecked();
        if (standard)
            resource.setEnabled(alarmType, true);
        Resources::setStandard(resource, alarmType, standard);
        if (alarmType == CalEvent::ARCHIVED)
            theApp()->purgeNewArchivedDefault(resource);
    }
}


/******************************************************************************
* Called from the context menu to merge alarms from an external calendar into
* the selected resource (if any).
*/
void ResourceSelector::importCalendar()
{
    Resource resource = currentResource();
    AlarmCalendar::resources()->importAlarms(this, &resource);
}

/******************************************************************************
* Called from the context menu to copy the selected resource's alarms to an
* external calendar.
*/
void ResourceSelector::exportCalendar()
{
    const Resource resource = currentResource();
    if (resource.isValid())
        AlarmCalendar::exportAlarms(AlarmCalendar::resources()->events(resource), this);
}

/******************************************************************************
* Called from the context menu to set a colour for the selected resource.
*/
void ResourceSelector::setColour()
{
    Resource resource = currentResource();
    if (resource.isValid())
    {
        QColor colour = resource.backgroundColour();
        if (!colour.isValid())
            colour = QApplication::palette().color(QPalette::Base);
        colour = QColorDialog::getColor(colour, this);
        if (colour.isValid())
            resource.setBackgroundColour(colour);
    }
}

/******************************************************************************
* Called from the context menu to clear the display colour for the selected
* resource.
*/
void ResourceSelector::clearColour()
{
    Resource resource = currentResource();
    if (resource.isValid())
        resource.setBackgroundColour(QColor());
}

/******************************************************************************
* Called from the context menu to display information for the selected resource.
*/
void ResourceSelector::showInfo()
{
    const Resource resource = currentResource();
    if (resource.isValid())
    {
        const QString name             = resource.displayName();
        const QString id               = resource.configName();   // resource name
        const QString calType          = resource.storageTypeString(true);
        const CalEvent::Type alarmType = currentResourceType();
        const QString storage          = resource.storageTypeString(false);
        const QString location         = resource.displayLocation();
        const CalEvent::Types altypes  = resource.alarmTypes();
        QStringList alarmTypes;
        if (altypes & CalEvent::ACTIVE)
            alarmTypes << i18nc("@info", "Active alarms");
        if (altypes & CalEvent::ARCHIVED)
            alarmTypes << i18nc("@info", "Archived alarms");
        if (altypes & CalEvent::TEMPLATE)
            alarmTypes << i18nc("@info", "Alarm templates");
        const QString alarmTypeString = alarmTypes.join(i18nc("@info List separator", ", "));
        QString perms = ResourceDataModelBase::readOnlyTooltip(resource);
        if (perms.isEmpty())
            perms = i18nc("@info", "Read-write");
        const QString enabled = resource.isEnabled(alarmType)
                           ? i18nc("@info", "Enabled")
                           : i18nc("@info", "Disabled");
        const QString std = Resources::isStandard(resource, alarmType)
                           ? i18nc("@info Parameter in 'Default calendar: Yes/No'", "Yes")
                           : i18nc("@info Parameter in 'Default calendar: Yes/No'", "No");
        const QString text = xi18nc("@info",
                                    "<title>%1</title>"
                                    "<para>ID: %2<nl/>"
                                    "Calendar type: %3<nl/>"
                                    "Contents: %4<nl/>"
                                    "%5: <filename>%6</filename><nl/>"
                                    "Permissions: %7<nl/>"
                                    "Status: %8<nl/>"
                                    "Default calendar: %9</para>",
                                    name, id, calType, alarmTypeString, storage, location, perms, enabled, std);
        // Display the resource information. Because the user requested
        // the information, don't raise a KNotify event.
        KAMessageBox::information(this, text, QString(), QString(), KMessageBox::Options());
    }
}

/******************************************************************************
* Return the currently selected resource in the list.
*/
Resource ResourceSelector::currentResource() const
{
    return mListView->resource(mListView->selectionModel()->currentIndex());
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
    Q_EMIT resized(re->oldSize(), re->size());
}

// vim: et sw=4:
