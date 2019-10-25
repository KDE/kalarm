/*
 *  collectionmodel.cpp  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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

#include "collectionmodel.h"

#include "autoqpointer.h"
#include "messagebox.h"
#include "preferences.h"
#include "kalarm_debug.h"

#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/collectionfetchjob.h>
#include <AkonadiCore/entitymimetypefiltermodel.h>
#include <AkonadiWidgets/AgentConfigurationDialog>
#include <AkonadiWidgets/collectiondialog.h>

#include <KLocalizedString>

#include <QUrl>
#include <QApplication>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>
#include <QObject>

using namespace Akonadi;
using namespace KAlarmCal;


/*=============================================================================
= Class: CollectionMimeTypeFilterModel
= Proxy model to filter AkonadiModel to restrict its contents to Collections,
= not Items, containing specified KAlarm content mime types.
= It can optionally be restricted to writable and/or enabled Collections.
=============================================================================*/
class CollectionMimeTypeFilterModel : public Akonadi::EntityMimeTypeFilterModel
{
        Q_OBJECT
    public:
        explicit CollectionMimeTypeFilterModel(QObject* parent = nullptr);
        void setEventTypeFilter(CalEvent::Type);
        void setFilterWritable(bool writable);
        void setFilterEnabled(bool enabled);
        Collection collection(int row) const;
        Collection collection(const QModelIndex&) const;
        QModelIndex resourceIndex(const Resource&) const;

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    private:
        CalEvent::Type mAlarmType{CalEvent::EMPTY};  // collection content type contained in this model
        bool    mWritableOnly{false}; // only include writable collections in this model
        bool    mEnabledOnly{false};  // only include enabled collections in this model
};

CollectionMimeTypeFilterModel::CollectionMimeTypeFilterModel(QObject* parent)
    : EntityMimeTypeFilterModel(parent)
{
    addMimeTypeInclusionFilter(Collection::mimeType());   // select collections, not items
    setHeaderGroup(EntityTreeModel::CollectionTreeHeaders);
    setSourceModel(AkonadiModel::instance());
}

void CollectionMimeTypeFilterModel::setEventTypeFilter(CalEvent::Type type)
{
    if (type != mAlarmType)
    {
        mAlarmType = type;
        invalidateFilter();
    }
}

void CollectionMimeTypeFilterModel::setFilterWritable(bool writable)
{
    if (writable != mWritableOnly)
    {
        mWritableOnly = writable;
        invalidateFilter();
    }
}

void CollectionMimeTypeFilterModel::setFilterEnabled(bool enabled)
{
    if (enabled != mEnabledOnly)
    {
        Q_EMIT layoutAboutToBeChanged();
        mEnabledOnly = enabled;
        invalidateFilter();
        Q_EMIT layoutChanged();
    }
}

bool CollectionMimeTypeFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EntityMimeTypeFilterModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    AkonadiModel* model = AkonadiModel::instance();
    const QModelIndex ix = model->index(sourceRow, 0, sourceParent);
    const Collection collection = model->data(ix, AkonadiModel::CollectionRole).value<Collection>();
    const Resource resource = model->resource(collection.id());
    if (collection.remoteId().isEmpty())
        return false;   // invalidly configured resource
    if (!AgentManager::self()->instance(collection.resource()).isValid())
        return false;
    if (!mWritableOnly  &&  mAlarmType == CalEvent::EMPTY)
        return true;
    if (mWritableOnly  &&  !resource.isWritable())
        return false;
    if (mAlarmType != CalEvent::EMPTY  &&  !(resource.alarmTypes() & mAlarmType))
        return false;
    if (mWritableOnly  &&  !resource.isCompatible())
        return false;
    if (mEnabledOnly  &&  !resource.isEnabled(mAlarmType))
        return false;
    return true;
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionMimeTypeFilterModel::collection(int row) const
{
    return static_cast<AkonadiModel*>(sourceModel())->data(mapToSource(index(row, 0)), EntityTreeModel::CollectionRole).value<Collection>();
}

Collection CollectionMimeTypeFilterModel::collection(const QModelIndex& index) const
{
    return static_cast<AkonadiModel*>(sourceModel())->data(mapToSource(index), EntityTreeModel::CollectionRole).value<Collection>();
}

QModelIndex CollectionMimeTypeFilterModel::resourceIndex(const Resource& resource) const
{
    return mapFromSource(static_cast<AkonadiModel*>(sourceModel())->resourceIndex(resource));
}


/*=============================================================================
= Class: CollectionListModel
= Proxy model converting the AkonadiModel collection tree into a flat list.
= The model may be restricted to specified content mime types.
= It can optionally be restricted to writable and/or enabled Collections.
=============================================================================*/

CollectionListModel::CollectionListModel(QObject* parent)
    : KDescendantsProxyModel(parent)
{
    setSourceModel(new CollectionMimeTypeFilterModel(this));
    setDisplayAncestorData(false);
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionListModel::collection(int row) const
{
    return data(index(row, 0), EntityTreeModel::CollectionRole).value<Collection>();
}

Collection CollectionListModel::collection(const QModelIndex& index) const
{
    return data(index, EntityTreeModel::CollectionRole).value<Collection>();
}

/******************************************************************************
* Return the resource for a given row.
*/
Resource CollectionListModel::resource(int row) const
{
    const Collection::Id id = data(index(row, 0), EntityTreeModel::CollectionIdRole).toLongLong();
    return AkonadiModel::instance()->resource(id);
}

Resource CollectionListModel::resource(const QModelIndex& index) const
{
    const Collection::Id id = data(index, EntityTreeModel::CollectionIdRole).toLongLong();
    return AkonadiModel::instance()->resource(id);
}

QModelIndex CollectionListModel::resourceIndex(const Resource& resource) const
{
    return mapFromSource(static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->resourceIndex(resource));
}

void CollectionListModel::setEventTypeFilter(CalEvent::Type type)
{
    static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->setEventTypeFilter(type);
}

void CollectionListModel::setFilterWritable(bool writable)
{
    static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->setFilterWritable(writable);
}

void CollectionListModel::setFilterEnabled(bool enabled)
{
    static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->setFilterEnabled(enabled);
}

bool CollectionListModel::isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const
{
    Q_UNUSED(descendant);
    return !ancestor.isValid();
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant CollectionListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::BackgroundRole:
            if (!mUseCollectionColour)
                role = AkonadiModel::BaseColourRole;
            break;
        default:
            break;
    }
    return KDescendantsProxyModel::data(index, role);
}


/*=============================================================================
= Class: CollectionCheckListModel
= Proxy model providing a checkable list of all Collections. A Collection's
= checked status is equivalent to whether it is selected or not.
= An alarm type is specified, whereby Collections which are enabled for that
= alarm type are checked; Collections which do not contain that alarm type, or
= which are disabled for that alarm type, are unchecked.
=============================================================================*/

CollectionListModel* CollectionCheckListModel::mModel = nullptr;
int                  CollectionCheckListModel::mInstanceCount = 0;

CollectionCheckListModel::CollectionCheckListModel(CalEvent::Type type, QObject* parent)
    : KCheckableProxyModel(parent)
    , mAlarmType(type)
{
    ++mInstanceCount;
    if (!mModel)
        mModel = new CollectionListModel(nullptr);
    setSourceModel(mModel);    // the source model is NOT filtered by alarm type
    mSelectionModel = new QItemSelectionModel(mModel);
    setSelectionModel(mSelectionModel);
    connect(mSelectionModel, &QItemSelectionModel::selectionChanged,
                       this, &CollectionCheckListModel::selectionChanged);
    connect(mModel, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, &QAbstractItemModel::rowsInserted,
              this, &CollectionCheckListModel::slotRowsInserted);
    // This is probably needed to make CollectionFilterCheckListModel update
    // (similarly to when rows are inserted).
    connect(mModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), SIGNAL(layoutChanged()));

    connect(AkonadiModel::instance(), &AkonadiModel::resourceStatusChanged,
                                this, &CollectionCheckListModel::resourceStatusChanged);

    // Initialise checked status for all collections.
    // Note that this is only necessary if the model is recreated after
    // being deleted.
    for (int row = 0, count = mModel->rowCount();  row < count;  ++row)
        setSelectionStatus(mModel->resource(row), mModel->index(row, 0));
}

CollectionCheckListModel::~CollectionCheckListModel()
{
    if (--mInstanceCount <= 0)
    {
        delete mModel;
        mModel = nullptr;
    }
}


/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionCheckListModel::collection(int row) const
{
    return mModel->collection(mapToSource(index(row, 0)));
}

Collection CollectionCheckListModel::collection(const QModelIndex& index) const
{
    return mModel->collection(mapToSource(index));
}

/******************************************************************************
* Return the resource for a given row.
*/
Resource CollectionCheckListModel::resource(int row) const
{
    return mModel->resource(mapToSource(index(row, 0)));
}

Resource CollectionCheckListModel::resource(const QModelIndex& index) const
{
    return mModel->resource(mapToSource(index));
}

/******************************************************************************
* Return model data for one index.
*/
QVariant CollectionCheckListModel::data(const QModelIndex& index, int role) const
{
    const Resource resource = mModel->resource(index);
    if (resource.isValid())
    {
        // This is a Collection row
        switch (role)
        {
            case Qt::ForegroundRole:
                return resource.foregroundColour(mAlarmType);
            case Qt::FontRole:
                if (!resource.isCompatible()
                ||  resource.enabledTypes() == CalEvent::EMPTY)
                    break;
                if (resource.configIsStandard(mAlarmType)  &&  (resource.alarmTypes() & mAlarmType))
                {
                    // It's the standard collection for a mime type
                    QFont font = qvariant_cast<QFont>(KCheckableProxyModel::data(index, role));
                    font.setBold(true);
                    return font;
                }
                break;
            default:
                break;
        }
    }
    return KCheckableProxyModel::data(index, role);
}

/******************************************************************************
* Set model data for one index.
* If the change is to disable a collection, check for eligibility and prevent
* the change if necessary.
*/
bool CollectionCheckListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole  &&  static_cast<Qt::CheckState>(value.toInt()) != Qt::Checked)
    {
        // A collection is to be disabled.
        const Resource resource = mModel->resource(index);
        if (resource.isValid()  &&  resource.isEnabled(mAlarmType))
        {
            QString errmsg;
            QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
            if (resource.isCompatible()  &&  resource.configIsStandard(mAlarmType))
            {
                // It's the standard collection for some alarm type.
                if (mAlarmType == CalEvent::ACTIVE)
                {
                    errmsg = i18nc("@info", "You cannot disable your default active alarm calendar.");
                }
                else if (mAlarmType == CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
                {
                    // Only allow the archived alarms standard collection to be disabled if
                    // we're not saving expired alarms.
                    errmsg = i18nc("@info", "You cannot disable your default archived alarm calendar "
                                            "while expired alarms are configured to be kept.");
                }
                else if (KAMessageBox::warningContinueCancel(messageParent,
                                                       i18nc("@info", "Do you really want to disable your default calendar?"))
                           == KMessageBox::Cancel)
                    return false;
            }
            if (!errmsg.isEmpty())
            {
                KAMessageBox::sorry(messageParent, errmsg);
                return false;
            }
        }
    }
    return KCheckableProxyModel::setData(index, value, role);
}

/******************************************************************************
* Called when rows have been inserted into the model.
* Select or deselect them according to their enabled status.
*/
void CollectionCheckListModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    Q_EMIT layoutAboutToBeChanged();
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = mapToSource(index(row, 0, parent));
        const Resource resource = mModel->resource(ix);
        if (resource.isValid())
            setSelectionStatus(resource, ix);
    }
    Q_EMIT layoutChanged();   // this is needed to make CollectionFilterCheckListModel update
}

/******************************************************************************
* Called when the user has ticked/unticked a collection to enable/disable it
* (or when the selection changes for any other reason).
*/
void CollectionCheckListModel::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const QModelIndexList sel = selected.indexes();
    for (const QModelIndex& ix : sel)
    {
        // Try to enable the collection, but untick it if not possible
        mModel->resource(ix).setEnabled(mAlarmType, true);
    }
    const QModelIndexList desel = deselected.indexes();
    for (const QModelIndex& ix : desel)
        mModel->resource(ix).setEnabled(mAlarmType, false);
}

/******************************************************************************
* Called when a collection parameter or status has changed.
* If the collection's alarm types have been reconfigured, ensure that the
* model views are updated to reflect this.
*/
void CollectionCheckListModel::resourceStatusChanged(const Resource& resource, AkonadiModel::Change change, const QVariant&, bool inserted)
{
    if (inserted  ||  !resource.isValid())
        return;
    switch (change)
    {
        case AkonadiModel::Enabled:
            qCDebug(KALARM_LOG) << debugType("resourceStatusChanged").constData() << "Enabled" << resource.id();
            break;
        case AkonadiModel::AlarmTypes:
            qCDebug(KALARM_LOG) << debugType("resourceStatusChanged").constData() << "AlarmTypes" << resource.id();
            break;
        default:
            return;
    }
    const QModelIndex ix = mModel->resourceIndex(resource);
    if (ix.isValid())
        setSelectionStatus(resource, ix);
    if (change == AkonadiModel::AlarmTypes)
        Q_EMIT collectionTypeChange(this);
}

/******************************************************************************
* Select or deselect an index according to its enabled status.
*/
void CollectionCheckListModel::setSelectionStatus(const Resource& resource, const QModelIndex& sourceIndex)
{
    const QItemSelectionModel::SelectionFlags sel = resource.isEnabled(mAlarmType)
                                                  ? QItemSelectionModel::Select : QItemSelectionModel::Deselect;
    mSelectionModel->select(sourceIndex, sel);
}

/******************************************************************************
* Return the instance's alarm type, as a string.
*/
QByteArray CollectionCheckListModel::debugType(const char* func) const
{
    const char* type;
    switch (mAlarmType)
    {
        case CalEvent::ACTIVE:    type = "CollectionCheckListModel[Act]::";  break;
        case CalEvent::ARCHIVED:  type = "CollectionCheckListModel[Arch]::";  break;
        case CalEvent::TEMPLATE:  type = "CollectionCheckListModel[Tmpl]::";  break;
        default:                  type = "CollectionCheckListModel::";  break;
    }
    return QByteArray(type) + func + ':';
}


/*=============================================================================
= Class: CollectionFilterCheckListModel
= Proxy model providing a checkable collection list. The model contains all
= alarm types, but returns only one type at any given time. The selected alarm
= type may be changed as desired.
=============================================================================*/
CollectionFilterCheckListModel::CollectionFilterCheckListModel(QObject* parent)
    : QSortFilterProxyModel(parent)
    , mActiveModel(new CollectionCheckListModel(CalEvent::ACTIVE, this))
    , mArchivedModel(new CollectionCheckListModel(CalEvent::ARCHIVED, this))
    , mTemplateModel(new CollectionCheckListModel(CalEvent::TEMPLATE, this))
{
    setDynamicSortFilter(true);
    connect(mActiveModel, &CollectionCheckListModel::collectionTypeChange,
                    this, &CollectionFilterCheckListModel::collectionTypeChanged);
    connect(mArchivedModel, &CollectionCheckListModel::collectionTypeChange,
                      this, &CollectionFilterCheckListModel::collectionTypeChanged);
    connect(mTemplateModel, &CollectionCheckListModel::collectionTypeChange,
                      this, &CollectionFilterCheckListModel::collectionTypeChanged);
}

void CollectionFilterCheckListModel::setEventTypeFilter(CalEvent::Type type)
{
    if (type != mAlarmType)
    {
        CollectionCheckListModel* newModel;
        switch (type)
        {
            case CalEvent::ACTIVE:    newModel = mActiveModel;  break;
            case CalEvent::ARCHIVED:  newModel = mArchivedModel;  break;
            case CalEvent::TEMPLATE:  newModel = mTemplateModel;  break;
            default:
                return;
        }
        mAlarmType = type;
        setSourceModel(newModel);
        invalidate();
    }
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionFilterCheckListModel::collection(int row) const
{
    return static_cast<CollectionCheckListModel*>(sourceModel())->collection(mapToSource(index(row, 0)));
}

Collection CollectionFilterCheckListModel::collection(const QModelIndex& index) const
{
    return static_cast<CollectionCheckListModel*>(sourceModel())->collection(mapToSource(index));
}

/******************************************************************************
* Return the resource for a given row.
*/
Resource CollectionFilterCheckListModel::resource(int row) const
{
    return static_cast<CollectionCheckListModel*>(sourceModel())->resource(mapToSource(index(row, 0)));
}

Resource CollectionFilterCheckListModel::resource(const QModelIndex& index) const
{
    return static_cast<CollectionCheckListModel*>(sourceModel())->resource(mapToSource(index));
}

QVariant CollectionFilterCheckListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::ToolTipRole:
        {
            const Resource res = resource(index);
            if (res.isValid())
                return AkonadiModel::instance()->tooltip(res, mAlarmType);
            break;
        }
        default:
            break;
    }
    return QSortFilterProxyModel::data(index, role);
}

bool CollectionFilterCheckListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (mAlarmType == CalEvent::EMPTY)
        return true;
    const CollectionCheckListModel* model = static_cast<CollectionCheckListModel*>(sourceModel());
    const Collection collection = model->collection(model->index(sourceRow, 0, sourceParent));
    return collection.contentMimeTypes().contains(CalEvent::mimeType(mAlarmType));
}

/******************************************************************************
* Called when a collection alarm type has changed.
* Ensure that the collection is removed from or added to the current model view.
*/
void CollectionFilterCheckListModel::collectionTypeChanged(CollectionCheckListModel* model)
{
    if (model == sourceModel())
        invalidateFilter();
}


/*=============================================================================
= Class: CollectionView
= View displaying a list of collections.
=============================================================================*/
CollectionView::CollectionView(CollectionFilterCheckListModel* model, QWidget* parent)
    : QListView(parent)
{
    setModel(model);
}

void CollectionView::setModel(QAbstractItemModel* model)
{
    QListView::setModel(model);
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionView::collection(int row) const
{
    return static_cast<CollectionFilterCheckListModel*>(model())->collection(row);
}

Collection CollectionView::collection(const QModelIndex& index) const
{
    return static_cast<CollectionFilterCheckListModel*>(model())->collection(index);
}

/******************************************************************************
* Return the resource for a given row.
*/
Resource CollectionView::resource(int row) const
{
    return static_cast<CollectionFilterCheckListModel*>(model())->resource(row);
}

Resource CollectionView::resource(const QModelIndex& index) const
{
    return static_cast<CollectionFilterCheckListModel*>(model())->resource(index);
}

/******************************************************************************
* Called when a mouse button is released.
* Any currently selected collection is deselected.
*/
void CollectionView::mouseReleaseEvent(QMouseEvent* e)
{
    if (!indexAt(e->pos()).isValid())
        clearSelection();
    QListView::mouseReleaseEvent(e);
}

/******************************************************************************
* Called when a ToolTip or WhatsThis event occurs.
*/
bool CollectionView::viewportEvent(QEvent* e)
{
    if (e->type() == QEvent::ToolTip  &&  isActiveWindow())
    {
        const QHelpEvent* he = static_cast<QHelpEvent*>(e);
        const QModelIndex index = indexAt(he->pos());
        QVariant value = static_cast<CollectionFilterCheckListModel*>(model())->data(index, Qt::ToolTipRole);
        if (value.canConvert<QString>())
        {
            QString toolTip = value.toString();
            int i = toolTip.indexOf(QLatin1Char('@'));
            if (i > 0)
            {
                const int j = toolTip.indexOf(QRegExp(QLatin1String("<(nl|br)"), Qt::CaseInsensitive), i + 1);
                const int k = toolTip.indexOf(QLatin1Char('@'), j);
                const QString name = toolTip.mid(i + 1, j - i - 1);
                value = model()->data(index, Qt::FontRole);
                const QFontMetrics fm(qvariant_cast<QFont>(value).resolve(viewOptions().font));
                int textWidth = fm.boundingRect(name).width() + 1;
                const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
                QStyleOptionButton opt;
                opt.QStyleOption::operator=(viewOptions());
                opt.rect = rectForIndex(index);
                const int checkWidth = QApplication::style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt).width();
                const int left = spacing() + 3*margin + checkWidth + viewOptions().decorationSize.width();   // left offset of text
                const int right = left + textWidth;
                if (left >= horizontalOffset() + spacing()
                &&  right <= horizontalOffset() + width() - spacing() - 2*frameWidth())
                {
                    // The whole of the collection name is already displayed,
                    // so omit it from the tooltip.
                    if (k > 0)
                        toolTip.remove(i, k + 1 - i);
                }
                else
                {
                    toolTip.remove(k, 1);
                    toolTip.remove(i, 1);
                }
            }
            QToolTip::showText(he->globalPos(), toolTip, this);
            return true;
        }
    }
    return QListView::viewportEvent(e);
}


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select which Collections will be enabled. Disabled Collections
= are not populated or monitored; their contents are ignored. The set of
= enabled Collections is stored in the config file's "Collections" group.
= Note that this model is not used directly for displaying - its purpose is to
= allow collections to be disabled, which will remove them from the other
= collection models.
=============================================================================*/

CollectionControlModel* CollectionControlModel::mInstance = nullptr;
QHash<QString, CollectionControlModel::ResourceCol> CollectionControlModel::mAgentPaths;

static QRegularExpression matchMimeType(QStringLiteral("^application/x-vnd\\.kde\\.alarm.*"),
                                        QRegularExpression::DotMatchesEverythingOption);

CollectionControlModel* CollectionControlModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionControlModel(qApp);
    return mInstance;
}

CollectionControlModel::CollectionControlModel(QObject* parent)
    : FavoriteCollectionsModel(AkonadiModel::instance(), KConfigGroup(KSharedConfig::openConfig(), "Collections"), parent)
{
    // Initialise the list of enabled collections
    EntityMimeTypeFilterModel* filter = new EntityMimeTypeFilterModel(this);
    filter->addMimeTypeInclusionFilter(Collection::mimeType());
    filter->setSourceModel(AkonadiModel::instance());

    QList<Collection::Id> collectionIds;
    findEnabledCollections(filter, QModelIndex(), collectionIds);
    setCollections(Collection::List());
    for (Collection::Id id : qAsConst(collectionIds))
        addCollection(Collection(id));

    connect(AkonadiModel::instance(), &AkonadiModel::resourceStatusChanged,
                                this, &CollectionControlModel::resourceStatusChanged);
    connect(AkonadiModel::instance(), &EntityTreeModel::collectionTreeFetched,
                                this, &CollectionControlModel::collectionPopulated);
    connect(AkonadiModel::instance(), &EntityTreeModel::collectionPopulated,
                                this, &CollectionControlModel::collectionPopulated);
    connect(AkonadiModel::instance(), &AkonadiModel::serverStopped,
                                this, &CollectionControlModel::reset);
}

/******************************************************************************
* Recursive function to check all collections' enabled status, and to compile a
* list of all collections which have any alarm types enabled.
* Collections which duplicate the same backend storage are filtered out, to
* avoid crashes due to duplicate events in different resources.
*/
void CollectionControlModel::findEnabledCollections(const EntityMimeTypeFilterModel* filter, const QModelIndex& parent, QList<Collection::Id>& collectionIds) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = filter->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = filter->index(row, 0, parent);
        const Collection collection = model->data(filter->mapToSource(ix), AkonadiModel::CollectionRole).value<Collection>();
        if (!AgentManager::self()->instance(collection.resource()).isValid())
            continue;    // the collection doesn't belong to a resource, so omit it
        Resource resource = AkonadiModel::instance()->resource(collection.id());
        const CalEvent::Types enabled = resource.enabledTypes();
        const CalEvent::Types canEnable = checkTypesToEnable(resource, collectionIds, enabled);
        if (canEnable != enabled)
        {
            // There is another collection which uses the same backend
            // storage. Disable alarm types enabled in the other collection.
            if (!resource.isBeingDeleted())
                resource.setEnabled(canEnable);
        }
        if (canEnable)
            collectionIds += collection.id();
        if (filter->rowCount(ix) > 0)
            findEnabledCollections(filter, ix, collectionIds);
    }
}

/******************************************************************************
* Change the collection's enabled status.
* Add or remove the collection to/from the enabled list.
* Reply = alarm types which can be enabled
*/
CalEvent::Types CollectionControlModel::setEnabledStatus(Resource& resource, CalEvent::Types types, bool inserted)
{
    qCDebug(KALARM_LOG) << "CollectionControlModel::setEnabledStatus:" << resource.id() << ", types=" << types;
    CalEvent::Types disallowedStdTypes{};
    CalEvent::Types stdTypes{};

    // Prevent the enabling of duplicate alarm types if another collection
    // uses the same backend storage.
    const QList<Collection::Id> colIds = collectionIds();
    const CalEvent::Types canEnable = checkTypesToEnable(resource, colIds, types);

    // Update the list of enabled collections
    if (canEnable)
    {
        if (!colIds.contains(resource.id()))
        {
            // It's a new collection.
            // Prevent duplicate standard collections being created for any alarm type.
            stdTypes = resource.configStandardTypes();
            if (stdTypes)
            {
                for (const Collection::Id& id : colIds)
                {
                    const Resource res = AkonadiModel::instance()->resource(id);
                    if (res.isValid())
                    {
                        const CalEvent::Types t = stdTypes & res.alarmTypes();
                        if (t  &&  resource.isCompatible())
                        {
                            disallowedStdTypes |= res.configStandardTypes() & t;
                            if (disallowedStdTypes == stdTypes)
                                break;
                        }
                    }
                }
            }
            addCollection(Collection(resource.id()));
        }
    }
    else
        removeCollection(Collection(resource.id()));

    if (disallowedStdTypes  ||  !inserted  ||  canEnable != types)
    {
        // Update the collection's status
        if (!resource.isBeingDeleted())
        {
            if (!inserted  ||  canEnable != types)
                resource.setEnabled(canEnable);
            if (disallowedStdTypes)
                resource.configSetStandard(stdTypes & ~disallowedStdTypes);
        }
    }
    return canEnable;
}

/******************************************************************************
* Called when a collection parameter or status has changed.
* If it's the enabled status, add or remove the collection to/from the enabled
* list.
*/
void CollectionControlModel::resourceStatusChanged(Resource& resource, AkonadiModel::Change change, const QVariant& value, bool inserted)
{
    if (!resource.isValid())
        return;

    switch (change)
    {
        case AkonadiModel::Enabled:
        {
            const CalEvent::Types enabled = static_cast<CalEvent::Types>(value.toInt());
            qCDebug(KALARM_LOG) << "CollectionControlModel::resourceStatusChanged:" << resource.id() << ", enabled=" << enabled << ", inserted=" << inserted;
            setEnabledStatus(resource, enabled, inserted);
            break;
        }
        case AkonadiModel::ReadOnly:
        {
            bool readOnly = value.toBool();
            qCDebug(KALARM_LOG) << "CollectionControlModel::resourceStatusChanged:" << resource.id() << ", readOnly=" << readOnly;
            if (readOnly)
            {
                // A read-only resource can't be the default for any alarm type
                const CalEvent::Types std = standardTypes(resource, false);
                if (std != CalEvent::EMPTY)
                {
                    Resource res = AkonadiModel::instance()->resource(resource.id());
                    setStandard(res, CalEvent::Types(CalEvent::EMPTY));
                    QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
                    bool singleType = true;
                    QString msg;
                    switch (std)
                    {
                        case CalEvent::ACTIVE:
                            msg = xi18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for active alarms.",
                                        resource.displayName());
                            break;
                        case CalEvent::ARCHIVED:
                            msg = xi18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for archived alarms.",
                                        resource.displayName());
                            break;
                        case CalEvent::TEMPLATE:
                            msg = xi18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for alarm templates.",
                                        resource.displayName());
                            break;
                        default:
                            msg = xi18nc("@info", "<para>The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for:%2</para>"
                                                 "<para>Please select new default calendars.</para>",
                                        resource.displayName(), CalendarDataModel::typeListForDisplay(std));
                            singleType = false;
                            break;
                    }
                    if (singleType)
                        msg = xi18nc("@info", "<para>%1</para><para>Please select a new default calendar.</para>", msg);
                    KAMessageBox::information(messageParent, msg);
                }
            }
            break;
        }
        default:
            break;
    }
}

/******************************************************************************
* Check which alarm types can be enabled for a specified collection.
* If the collection uses the same backend storage as another collection, any
* alarm types already enabled in the other collection must be disabled in this
* collection. This is to avoid duplicating events between different resources,
* which causes user confusion and annoyance, and causes crashes.
* Parameters:
*   collectionIds = list of collection IDs to search for duplicates.
*   types         = alarm types to be enabled for the collection.
* Reply = alarm types which can be enabled without duplicating other collections.
*/
CalEvent::Types CollectionControlModel::checkTypesToEnable(const Resource& resource, const QList<Collection::Id>& collectionIds, CalEvent::Types types)
{
    types &= (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    if (types)
    {
        // At least one alarm type is to be enabled
        const QUrl location = resource.location();
        for (const Collection::Id& id : collectionIds)
        {
            const Resource res = AkonadiModel::instance()->resource(id);
            const QUrl cLocation = res.location();
            if (id != resource.id()  &&  cLocation == location)
            {
                // The collection duplicates the backend storage
                // used by another enabled collection.
                // N.B. don't refresh this collection - assume no change.
                qCDebug(KALARM_LOG) << "CollectionControlModel::checkTypesToEnable:" << id << "duplicates backend for" << resource.id();
                types &= ~res.enabledTypes();
                if (!types)
                    break;
            }
        }
    }
    return types;
}

/******************************************************************************
* Return the standard collection for a specified mime type.
* If the mime type is 'archived' and there is no standard collection, the only
* writable archived collection is set to be the standard.
*/
Resource CollectionControlModel::getStandard(CalEvent::Type type)
{
    AkonadiModel* model = AkonadiModel::instance();
    int defaultArch = -1;
    const QList<Collection::Id> colIds = instance()->collectionIds();
    QList<Resource> resources;
    for (int i = 0, count = colIds.count();  i < count;  ++i)
    {
        resources.append(model->resource(colIds[i]));
        Resource& res = resources.last();
        if (res.alarmTypes() & type)
        {
            if ((res.configStandardTypes() & type)
            &&  res.isCompatible())
                return res;
            if (type == CalEvent::ARCHIVED  &&  res.isWritable(type))
                defaultArch = (defaultArch == -1) ? i : -2;
        }
    }

    if (defaultArch >= 0)
    {
        // There is no standard collection for archived alarms, but there is
        // only one writable collection for the type. Set the collection to be
        // the standard.
        setStandard(resources[defaultArch], type, true);
        return resources[defaultArch];
    }
    return Resource();
}

/******************************************************************************
* Return whether a collection is the standard collection for a specified
* mime type.
*/
bool CollectionControlModel::isStandard(const Resource& resource, CalEvent::Type type)
{
    // If it's for archived alarms, set the standard resource if necessary.
    if (type == CalEvent::ARCHIVED)
        return getStandard(type) == resource;

    if (!instance()->collectionIds().contains(resource.id())
    ||  !resource.isCompatible())
        return false;
    return resource.configIsStandard(type);
}

/******************************************************************************
* Return the alarm type(s) for which a collection is the standard collection.
*/
CalEvent::Types CollectionControlModel::standardTypes(const Resource& resource, bool useDefault)
{
    if (!instance()->collectionIds().contains(resource.id())
    ||  !resource.isCompatible())
        return CalEvent::EMPTY;
    CalEvent::Types stdTypes = resource.configStandardTypes();
    if (useDefault)
    {
        // Also return alarm types for which this is the only collection.
        CalEvent::Types wantedTypes = resource.alarmTypes() & ~stdTypes;
        const QList<Collection::Id> colIds = instance()->collectionIds();
        for (int i = 0, count = colIds.count();  wantedTypes && i < count;  ++i)
        {
            if (colIds[i] == resource.id())
                continue;
            Resource res = AkonadiModel::instance()->resource(colIds[i]);
            if (res.isValid())
                wantedTypes &= ~res.alarmTypes();
        }
        stdTypes |= wantedTypes;
    }
    return stdTypes;
}

/******************************************************************************
* Set or clear a collection as the standard collection for a specified mime
* type. If it is being set as standard, the standard status for the mime type
* is cleared for all other collections.
*/
void CollectionControlModel::setStandard(Resource& resource, CalEvent::Type type, bool standard)
{
    AkonadiModel* model = AkonadiModel::instance();
    if (!resource.isCompatible())
        standard = false;   // the collection isn't writable
    if (standard)
    {
        // The collection is being set as standard.
        // Clear the 'standard' status for all other collections.
        const QList<Collection::Id> colIds = instance()->collectionIds();
        if (!colIds.contains(resource.id()))
            return;
        const CalEvent::Types ctypes = resource.configStandardTypes();
        if (ctypes & type)
            return;    // it's already the standard collection for this type
        for (Collection::Id colId : colIds)
        {
            CalEvent::Types types;
            Resource res = model->resource(colId);
            if (colId == resource.id())
                types = ctypes | type;
            else
            {
                types = res.configStandardTypes();
                if (!(types & type))
                    continue;
                types &= ~type;
            }
            res.configSetStandard(types);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        const CalEvent::Types types = resource.configStandardTypes();
        if (types & type)
            resource.configSetStandard(types & ~type);
    }
}

/******************************************************************************
* Set which mime types a collection is the standard collection for.
* If it is being set as standard for any mime types, the standard status for
* those mime types is cleared for all other collections.
*/
void CollectionControlModel::setStandard(Resource& resource, CalEvent::Types types)
{
    AkonadiModel* model = AkonadiModel::instance();
    if (!resource.isCompatible())
        types = CalEvent::EMPTY;   // the collection isn't writable
    if (types)
    {
        // The collection is being set as standard for at least one mime type.
        // Clear the 'standard' status for all other collections.
        const QList<Collection::Id> colIds = instance()->collectionIds();
        if (!colIds.contains(resource.id()))
            return;
        const CalEvent::Types t = resource.configStandardTypes();
        if (t == types)
            return;    // there's no change to the collection's status
        for (Collection::Id colId : colIds)
        {
            CalEvent::Types t;
            Collection c(colId);
            Resource res = model->resource(colId);
            if (colId == resource.id())
                t = types;
            else
            {
                t = res.configStandardTypes();
                if (!(t & types))
                    continue;
                t &= ~types;
            }
            res.configSetStandard(t);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        if (resource.configStandardTypes())
            resource.configSetStandard(types);
    }
}

/******************************************************************************
* Get the collection to use for storing an alarm.
* Optionally, the standard collection for the alarm type is returned. If more
* than one collection is a candidate, the user is prompted.
*/
Resource CollectionControlModel::destination(CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Resource standard;
    if (type == CalEvent::EMPTY)
        return standard;
    standard = getStandard(type);
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == CalEvent::ARCHIVED  ||  noPrompt
    ||  (!Preferences::askResource()  &&  standard.isValid()))
        return standard;

    // Prompt for which collection to use
    CollectionListModel* model = new CollectionListModel(promptParent);
    model->setFilterWritable(true);
    model->setFilterEnabled(true);
    model->setEventTypeFilter(type);
    model->useCollectionColour(false);
    Collection col;
    switch (model->rowCount())
    {
        case 0:
            break;
        case 1:
            col = model->collection(0);
            break;
        default:
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the dialogue is still open. It prevents double deletion (both on
            // deletion of 'promptParent', and on return from this function).
            AutoQPointer<CollectionDialog> dlg = new CollectionDialog(model, promptParent);
            dlg->setWindowTitle(i18nc("@title:window", "Choose Calendar"));
            dlg->setDefaultCollection(Collection(standard.id()));
            dlg->setMimeTypeFilter(QStringList(CalEvent::mimeType(type)));
            if (dlg->exec())
                col = dlg->selectedCollection();
            if (!col.isValid()  &&  cancelled)
                *cancelled = true;
        }
    }
    return AkonadiModel::instance()->resource(col.id());
}

/******************************************************************************
* Edit a resource's configuration.
*/
void CollectionControlModel::editResource(const Resource& resource, QWidget* parent)
{
    if (resource.isValid())
    {
        AgentInstance instance = AgentManager::self()->instance(resource.configName());
        if (instance.isValid())
        {
            QPointer<AgentConfigurationDialog> dlg = new AgentConfigurationDialog(instance, parent);
            dlg->exec();
            delete dlg;
        }
    }
}

/******************************************************************************
* Remove a collection from Akonadi. The calendar file is not removed.
*/
bool CollectionControlModel::removeResource(Akonadi::Collection::Id id)
{
    Resource resource = AkonadiModel::instance()->resource(id);
    if (!resource.isValid())
        return false;
    qCDebug(KALARM_LOG) << "CollectionControlModel::removeResource:" << id;
    resource.notifyDeletion();
    // Note: Don't use CollectionDeleteJob, since that also deletes the backend storage.
    AgentManager* agentManager = AgentManager::self();
    const AgentInstance instance = agentManager->instance(resource.configName());
    if (instance.isValid())
        agentManager->removeInstance(instance);
    return true;
}

/******************************************************************************
* Return all collections which belong to a resource and which optionally
* contain a specified mime type.
*/
Collection::List CollectionControlModel::allCollections(CalEvent::Type type)
{
    const bool allTypes = (type == CalEvent::EMPTY);
    const QString mimeType = CalEvent::mimeType(type);
    AkonadiModel* model = AkonadiModel::instance();
    Collection::List result;
    const QList<Collection::Id> colIds = instance()->collectionIds();
    for (Collection::Id colId : colIds)
    {
        Resource res = model->resource(colId);
        if (res.isValid()  &&  (allTypes  ||  res.alarmTypes() & type))
        {
            Collection* c = model->collection(colId);
            if (c)
                result += *c;
        }
    }
    return result;
}

/******************************************************************************
* Return the enabled collections which contain a specified mime type.
* If 'writable' is true, only writable collections are included.
*/
Collection::List CollectionControlModel::enabledCollections(CalEvent::Type type, bool writable)
{
    const QList<Collection::Id> colIds = instance()->collectionIds();
    Collection::List result;
    for (Collection::Id colId : colIds)
    {
        Collection c(colId);
        const Resource resource = AkonadiModel::instance()->resource(colId);
        if (writable  &&  !resource.isWritable())
            continue;
        if (resource.alarmTypes() & type)
            result += c;
    }
    return result;
}

/******************************************************************************
* Return the collection ID for a given Akonadi resource ID.
*/
Collection::Id CollectionControlModel::collectionForResourceName(const QString& resourceName)
{
    const QList<Collection::Id> colIds = instance()->collectionIds();
    for (Collection::Id colId : colIds)
    {
        Resource res = AkonadiModel::instance()->resource(colId);
        if (res.configName() == resourceName)
            return res.id();
    }
    return -1;
}

/******************************************************************************
* Wait for one or all enabled collections to be populated.
* Reply = true if successful.
*/
bool CollectionControlModel::waitUntilPopulated(Collection::Id colId, int timeout)
{
    qCDebug(KALARM_LOG) << "CollectionControlModel::waitUntilPopulated";
    int result = 1;
    AkonadiModel* model = AkonadiModel::instance();
    while (!model->isCollectionTreeFetched()
       ||  !isPopulated(colId))
    {
        if (!mPopulatedCheckLoop)
            mPopulatedCheckLoop = new QEventLoop(this);
        if (timeout > 0)
            QTimer::singleShot(timeout * 1000, mPopulatedCheckLoop, &QEventLoop::quit);
        result = mPopulatedCheckLoop->exec();
    }
    delete mPopulatedCheckLoop;
    mPopulatedCheckLoop = nullptr;
    return result;
}

/******************************************************************************
* Return whether one or all enabled collections have been populated.
*/
bool CollectionControlModel::isPopulated(Collection::Id collectionId)
{
    AkonadiModel* model = AkonadiModel::instance();
    const QList<Collection::Id> colIds = instance()->collectionIds();
    for (Collection::Id colId : colIds)
    {
        if (collectionId == -1  ||  collectionId == colId)
        {
            const Resource res = model->resource(colId);
            if (!res.isLoaded()
            &&  res.enabledTypes() != CalEvent::EMPTY)
                return false;
        }
    }
    return true;
}

/******************************************************************************
* Check for, and remove, any Akonadi resources which duplicate use of calendar
* files/directories.
*/
void CollectionControlModel::removeDuplicateResources()
{
    mAgentPaths.clear();
    const AgentInstance::List agents = AgentManager::self()->instances();
    for (const AgentInstance& agent : agents)
    {
        if (agent.type().mimeTypes().indexOf(matchMimeType) >= 0)
        {
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
            job->fetchScope().setResource(agent.identifier());
            connect(job, &CollectionFetchJob::result, instance(), &CollectionControlModel::collectionFetchResult);
        }
    }
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
*/
void CollectionControlModel::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCCritical(KALARM_LOG) << "CollectionControlModel::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        AgentManager* agentManager = AgentManager::self();
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            if (c.contentMimeTypes().indexOf(matchMimeType) >= 0)
            {
                ResourceCol thisRes(job->fetchScope().resource(), c.id());
                auto it = mAgentPaths.constFind(c.remoteId());
                if (it != mAgentPaths.constEnd())
                {
                    // Remove the resource containing the higher numbered Collection
                    // ID, which is likely to be the more recently created.
                    const ResourceCol prevRes = it.value();
                    if (thisRes.collectionId > prevRes.collectionId)
                    {
                        qCWarning(KALARM_LOG) << "CollectionControlModel::collectionFetchResult: Removing duplicate resource" << thisRes.resourceId;
                        agentManager->removeInstance(agentManager->instance(thisRes.resourceId));
                        continue;
                    }
                    qCWarning(KALARM_LOG) << "CollectionControlModel::collectionFetchResult: Removing duplicate resource" << prevRes.resourceId;
                    agentManager->removeInstance(agentManager->instance(prevRes.resourceId));
                }
                mAgentPaths[c.remoteId()] = thisRes;
            }
        }
    }
}

/******************************************************************************
* Called when the Akonadi server has stopped. Reset the model.
*/
void CollectionControlModel::reset()
{
    delete mPopulatedCheckLoop;
    mPopulatedCheckLoop = nullptr;

    // Clear the collections list. This is required because addCollection() or
    // setCollections() don't work if the collections which they specify are
    // already in the list.
    setCollections(Collection::List());
}

/******************************************************************************
* Exit from the populated event loop when a collection has been populated.
*/
void CollectionControlModel::collectionPopulated()
{
    if (mPopulatedCheckLoop)
        mPopulatedCheckLoop->exit(1);
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant CollectionControlModel::data(const QModelIndex& index, int role) const
{
    return sourceModel()->data(mapToSource(index), role);
}

#include "collectionmodel.moc"

// vim: et sw=4:
