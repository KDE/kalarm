/*
 *  resourceselector.cpp  -  calendar resource selection widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006-2025 David Jarvie <djarvie@kde.org>
 *  Based on KOrganizer's ResourceView class and KAddressBook's ResourceSelection class,
 *  SPDX-FileCopyrightText: 2003, 2004 Cornelius Schumacher <schumacher@kde.org>
 *  SPDX-FileCopyrightText: 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
 *  SPDX-FileCopyrightText: 2004 Tobias Koenig <tokoe@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resourceselector.h"

#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "resourcescalendar.h"
#include "resources/datamodel.h"
#include "resources/resourcecreator.h"
#include "resources/resourcedatamodelbase.h"
#include "resources/resourcemodel.h"
#include "resources/resources.h"
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


ResourceSelector::ResourceSelector(MainWindow* parentWindow, QWidget* parent)
    : QFrame(parent)
    , mMainWindow(parentWindow)
{
    QBoxLayout* topLayout = new QVBoxLayout(this);

    QLabel* label = new QLabel(i18nc("@title:group", "Calendars"), this);
    topLayout->addWidget(label, 0, Qt::AlignHCenter);

    mAlarmType = new QComboBox(this);
    mAlarmType->addItem(i18nc("@item:inlistbox", "Active Alarms"));
    mAlarmType->addItem(i18nc("@item:inlistbox", "Archived Alarms"));
    mAlarmType->addItem(i18nc("@item:inlistbox", "Alarm Templates"));
    mAlarmType->setWhatsThis(i18nc("@info:whatsthis", "Choose which type of data to show alarm calendars for"));
    topLayout->addWidget(mAlarmType);
    // No spacing between combo box and listview.

    ResourceFilterCheckListModel* model = DataModel::createResourceFilterCheckListModel(this);
    mListView = new ResourceView(model, this);
    connect(mListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ResourceSelector::selectionChanged);
    mListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mListView, &ResourceView::customContextMenuRequested, this, &ResourceSelector::contextMenuRequested);
    connect(mListView, &ResourceView::rowCountChanged, this, &ResourceSelector::resizeToList);
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
    mEditButton->setToolTip(i18nc("@info:tooltip", "Reconfigure the selected calendar"));
    mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the highlighted calendar's configuration"));
    mDeleteButton->setToolTip(i18nc("@info:tooltip", "Remove the selected calendar from the list"));
    mDeleteButton->setWhatsThis(xi18nc("@info:whatsthis", "<para>Remove the highlighted calendar from the list.</para>"
                                     "<para>The calendar itself is left intact, and may subsequently be reinstated in the list if desired.</para>"));
    mEditButton->setDisabled(true);
    mDeleteButton->setDisabled(true);
    connect(mAddButton, &QPushButton::clicked, this, &ResourceSelector::addResource);
    connect(mEditButton, &QPushButton::clicked, this, &ResourceSelector::editResource);
    connect(mDeleteButton, &QPushButton::clicked, this, &ResourceSelector::removeResource);

    connect(mAlarmType, &QComboBox::activated, this, &ResourceSelector::alarmTypeSelected);
    QTimer::singleShot(0, this, &ResourceSelector::alarmTypeSelected);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    Preferences::connect(&Preferences::archivedKeepDaysChanged, this, &ResourceSelector::archiveDaysChanged);
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
    QTimer::singleShot(0, this, &ResourceSelector::reinstateAlarmTypeScrollBars);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

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
    ResourceCreator* creator = DataModel::createResourceCreator(mCurrentAlarmType, this);
    connect(creator, &ResourceCreator::resourceAdded, this, &ResourceSelector::slotResourceAdded);
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
    Resource resource = currentResource();
    if (!resource.isValid())
        return;
    DataModel::updateCalendarToCurrentFormat(resource, true, this);
}

/******************************************************************************
* Remove the currently selected resource from the displayed list.
*/
void ResourceSelector::removeResource()
{
    Resource resource = currentResource();
    if (!resource.isValid())
        return;
    qCDebug(KALARM_LOG) << "ResourceSelector::removeResource:" << resource.displayName();
    const QString name = resource.displayName();
    // Check if it's the standard or only resource for at least one type.
    const CalEvent::Types allTypes      = resource.alarmTypes();
    const CalEvent::Types standardTypes = Resources::standardTypes(resource, true);
    const CalEvent::Type  currentType   = currentResourceType();
    const CalEvent::Type  stdType = (standardTypes & CalEvent::ACTIVE)   ? CalEvent::ACTIVE
                                  : (standardTypes & CalEvent::ARCHIVED) ? CalEvent::ARCHIVED
                                  : CalEvent::EMPTY;
    if (stdType == CalEvent::ACTIVE)
    {
        KAMessageBox::error(this, i18nc("@info", "You cannot remove your default active alarm calendar."));
        return;
    }
    if (stdType == CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
    {
        // Only allow the archived alarms standard resource to be removed if
        // we're not saving archived alarms.
        KAMessageBox::error(this, i18nc("@info", "You cannot remove your default archived alarm calendar "
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
                otherTypes = i18nc("@info", "It also contains %1", ResourceDataModelBase::typeListForDisplay(nonStandardTypes));
            text = xi18nc("@info", "<para><resource>%1</resource> is the default calendar for %2</para><para>%3</para>"
                                  "<para>Do you really want to remove it from all calendar lists?</para>", name, stdTypes, otherTypes);
        }
        else
            text = xi18nc("@info", "Do you really want to remove your default calendar (<resource>%1</resource>) from the list?", name);
    }
    else if (allTypes != currentType)
        text = xi18nc("@info", "<para><resource>%1</resource> contains %2</para><para>Do you really want to remove it from all calendar lists?</para>",
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
    bool state = !mListView->selectionModel()->selectedRows().isEmpty();
    mDeleteButton->setEnabled(state);
    mEditButton->setEnabled(state);
}

/******************************************************************************
* Initialise the button and context menu actions.
*/
void ResourceSelector::initActions(KActionCollection* actions)
{
    if (mActionReload)
        return;   // this function can only be called once
    mActionReload      = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action Reload calendar", "Reload"), this);
    actions->addAction(QStringLiteral("resReload"), mActionReload);
    connect(mActionReload, &QAction::triggered, this, &ResourceSelector::reloadResource);
    mActionShowDetails = new QAction(QIcon::fromTheme(QStringLiteral("help-about")), i18nc("@action", "Show Details"), this);
    actions->addAction(QStringLiteral("resDetails"), mActionShowDetails);
    connect(mActionShowDetails, &QAction::triggered, this, &ResourceSelector::showInfo);
    mActionSetColour   = new QAction(QIcon::fromTheme(QStringLiteral("color-picker")), i18nc("@action", "Set Color..."), this);
    actions->addAction(QStringLiteral("resSetColour"), mActionSetColour);
    connect(mActionSetColour, &QAction::triggered, this, &ResourceSelector::setColour);
    mActionClearColour   = new QAction(i18nc("@action", "Clear Color"), this);
    actions->addAction(QStringLiteral("resClearColour"), mActionClearColour);
    connect(mActionClearColour, &QAction::triggered, this, &ResourceSelector::clearColour);
    mActionEdit        = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action", "Edit..."), this);
    actions->addAction(QStringLiteral("resEdit"), mActionEdit);
    connect(mActionEdit, &QAction::triggered, this, &ResourceSelector::editResource);
    mActionUpdate      = new QAction(i18nc("@action", "Update Calendar Format"), this);
    actions->addAction(QStringLiteral("resUpdate"), mActionUpdate);
    connect(mActionUpdate, &QAction::triggered, this, &ResourceSelector::updateResource);
    mActionRemove      = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action", "Remove"), this);
    actions->addAction(QStringLiteral("resRemove"), mActionRemove);
    connect(mActionRemove, &QAction::triggered, this, &ResourceSelector::removeResource);
    mActionSetDefault  = new KToggleAction(this);
    actions->addAction(QStringLiteral("resDefault"), mActionSetDefault);
    connect(mActionSetDefault, &KToggleAction::triggered, this, &ResourceSelector::setStandard);
    QAction* action    = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), i18nc("@action", "Add..."), this);
    actions->addAction(QStringLiteral("resAdd"), action);
    connect(action, &QAction::triggered, this, &ResourceSelector::addResource);
    mActionImport      = new QAction(i18nc("@action", "Import..."), this);
    actions->addAction(QStringLiteral("resImport"), mActionImport);
    connect(mActionImport, &QAction::triggered, this, &ResourceSelector::importCalendar);
    mActionExport      = new QAction(i18nc("@action", "Export..."), this);
    actions->addAction(QStringLiteral("resExport"), mActionExport);
    connect(mActionExport, &QAction::triggered, this, &ResourceSelector::exportCalendar);
}

/******************************************************************************
* Display the context menu for the selected calendar.
*/
void ResourceSelector::contextMenuRequested(const QPoint& viewportPos)
{
    mContextMenu = mMainWindow->resourceContextMenu();
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
        case CalEvent::ACTIVE:   text = i18nc("@action", "Use as Default for Active Alarms");  break;
        case CalEvent::ARCHIVED: text = i18nc("@action", "Use as Default for Archived Alarms");  break;
        case CalEvent::TEMPLATE: text = i18nc("@action", "Use as Default for Alarm Templates");  break;
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
        const Resource resource = Resources::getStandard(CalEvent::ARCHIVED, true);
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
    KAlarm::importAlarms(resource, this);
}

/******************************************************************************
* Called from the context menu to copy the selected resource's alarms to an
* external calendar.
*/
void ResourceSelector::exportCalendar()
{
    const Resource resource = currentResource();
    if (resource.isValid())
        KAlarm::exportAlarms(ResourcesCalendar::events(resource), this);
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

/******************************************************************************
* Called when the size hint for the list view has changed.
* Resize the list view and this widget to fit the new list view size hint.
*/
void ResourceSelector::resizeToList()
{
    if (mResizeToList)
    {
        const int change = mListView->sizeHint().height() - mListView->height();
        mListView->resize(mListView->width(), mListView->height() + change);
        resize(width(), height() + change);
    }
}

void ResourceSelector::resizeEvent(QResizeEvent* re)
{
    Q_EMIT resized(re->oldSize(), re->size());
}

#include "moc_resourceselector.cpp"

// vim: et sw=4:
