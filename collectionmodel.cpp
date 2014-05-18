/*
 *  collectionmodel.cpp  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2007-2012 by David Jarvie <djarvie@kde.org>
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

#include <kalarmcal/collectionattribute.h>
#include <kalarmcal/compatibilityattribute.h>

#include <AkonadiCore/agentmanager.h>
#include <AkonadiWidgets/collectiondialog.h>
#include <AkonadiCore/collectiondeletejob.h>
#include <AkonadiCore/collectionmodifyjob.h>
#include <AkonadiCore/entitymimetypefiltermodel.h>

#include <klocale.h>
#include <KDebug>

#include <QApplication>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>
#include <QObject>
#include <KSharedConfig>

using namespace Akonadi;
using namespace KAlarmCal;

static Collection::Rights writableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;


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
        explicit CollectionMimeTypeFilterModel(QObject* parent = 0);
        void setEventTypeFilter(CalEvent::Type);
        void setFilterWritable(bool writable);
        void setFilterEnabled(bool enabled);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        QModelIndex collectionIndex(const Akonadi::Collection&) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private:
        CalEvent::Type mAlarmType;  // collection content type contained in this model
        bool    mWritableOnly; // only include writable collections in this model
        bool    mEnabledOnly;  // only include enabled collections in this model
};

CollectionMimeTypeFilterModel::CollectionMimeTypeFilterModel(QObject* parent)
    : EntityMimeTypeFilterModel(parent),
      mAlarmType(CalEvent::EMPTY),
      mWritableOnly(false),
      mEnabledOnly(false)
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
        emit layoutAboutToBeChanged();
        mEnabledOnly = enabled;
        invalidateFilter();
        emit layoutChanged();
    }
}

bool CollectionMimeTypeFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EntityMimeTypeFilterModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    AkonadiModel* model = AkonadiModel::instance();
    const QModelIndex ix = model->index(sourceRow, 0, sourceParent);
    const Collection collection = model->data(ix, AkonadiModel::CollectionRole).value<Collection>();
    if (!AgentManager::self()->instance(collection.resource()).isValid())
        return false;
    if (!mWritableOnly  &&  mAlarmType == CalEvent::EMPTY)
        return true;
    if (mWritableOnly  &&  (collection.rights() & writableRights) != writableRights)
        return false;
    if (mAlarmType != CalEvent::EMPTY  &&  !collection.contentMimeTypes().contains(CalEvent::mimeType(mAlarmType)))
        return false;
    if ((mWritableOnly || mEnabledOnly)  &&  !collection.hasAttribute<CollectionAttribute>())
        return false;
    if (mWritableOnly  &&  (!collection.hasAttribute<CompatibilityAttribute>()
                         || collection.attribute<CompatibilityAttribute>()->compatibility() != KACalendar::Current))
        return false;
    if (mEnabledOnly  &&  !collection.attribute<CollectionAttribute>()->isEnabled(mAlarmType))
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

QModelIndex CollectionMimeTypeFilterModel::collectionIndex(const Collection& collection) const
{
    return mapFromSource(static_cast<AkonadiModel*>(sourceModel())->collectionIndex(collection));
}


/*=============================================================================
= Class: CollectionListModel
= Proxy model converting the AkonadiModel collection tree into a flat list.
= The model may be restricted to specified content mime types.
= It can optionally be restricted to writable and/or enabled Collections.
=============================================================================*/

CollectionListModel::CollectionListModel(QObject* parent)
    : KDescendantsProxyModel(parent),
      mUseCollectionColour(true)
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

QModelIndex CollectionListModel::collectionIndex(const Collection& collection) const
{
    return mapFromSource(static_cast<CollectionMimeTypeFilterModel*>(sourceModel())->collectionIndex(collection));
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

CollectionListModel* CollectionCheckListModel::mModel = 0;
int                  CollectionCheckListModel::mInstanceCount = 0;

CollectionCheckListModel::CollectionCheckListModel(CalEvent::Type type, QObject* parent)
    : KCheckableProxyModel(parent),
      mAlarmType(type)
{
    ++mInstanceCount;
    if (!mModel)
        mModel = new CollectionListModel(0);
    setSourceModel(mModel);    // the source model is NOT filtered by alarm type
    mSelectionModel = new QItemSelectionModel(mModel);
    setSelectionModel(mSelectionModel);
    connect(mSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                             SLOT(selectionChanged(QItemSelection,QItemSelection)));
    connect(mModel, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(slotRowsInserted(QModelIndex,int,int)));
    // This is probably needed to make CollectionFilterCheckListModel update
    // (similarly to when rows are inserted).
    connect(mModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), SIGNAL(layoutChanged()));

    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)),
                                      SLOT(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)));

    // Initialise checked status for all collections.
    // Note that this is only necessary if the model is recreated after
    // being deleted.
    for (int row = 0, count = mModel->rowCount();  row < count;  ++row)
        setSelectionStatus(mModel->collection(row), mModel->index(row, 0));
}

CollectionCheckListModel::~CollectionCheckListModel()
{
    if (--mInstanceCount <= 0)
    {
        delete mModel;
        mModel = 0;
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
* Return model data for one index.
*/
QVariant CollectionCheckListModel::data(const QModelIndex& index, int role) const
{
    const Collection collection = mModel->collection(index);
    if (collection.isValid())
    {
        // This is a Collection row
        switch (role)
        {
            case Qt::ForegroundRole:
            {
                const QString mimeType = CalEvent::mimeType(mAlarmType);
                if (collection.contentMimeTypes().contains(mimeType))
                    return AkonadiModel::foregroundColor(collection, QStringList(mimeType));
                break;
            }
            case Qt::FontRole:
            {
                if (!collection.hasAttribute<CollectionAttribute>()
                ||  !AkonadiModel::isCompatible(collection))
                    break;
                const CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
                if (!attr->enabled())
                    break;
                const QStringList mimeTypes = collection.contentMimeTypes();
                if (attr->isStandard(mAlarmType)  &&  mimeTypes.contains(CalEvent::mimeType(mAlarmType)))
                {
                    // It's the standard collection for a mime type
                    QFont font = qvariant_cast<QFont>(KCheckableProxyModel::data(index, role));
                    font.setBold(true);
                    return font;
                }
                break;
            }
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
        const Collection collection = mModel->collection(index);
        if (collection.isValid()  &&  collection.hasAttribute<CollectionAttribute>())
        {
            const CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
            if (attr->isEnabled(mAlarmType))
            {
                QString errmsg;
                QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
                if (attr->isStandard(mAlarmType)
                &&  AkonadiModel::isCompatible(collection))
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
    }
    return KCheckableProxyModel::setData(index, value, role);
}

/******************************************************************************
* Called when rows have been inserted into the model.
* Select or deselect them according to their enabled status.
*/
void CollectionCheckListModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = mapToSource(index(row, 0, parent));
        const Collection collection = mModel->collection(ix);
        if (collection.isValid())
            setSelectionStatus(collection, ix);
    }
    emit layoutChanged();   // this is needed to make CollectionFilterCheckListModel update
}

/******************************************************************************
* Called when the user has ticked/unticked a collection to enable/disable it
* (or when the selection changes for any other reason).
*/
void CollectionCheckListModel::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const QModelIndexList sel = selected.indexes();
    foreach (const QModelIndex& ix, sel)
    {
        // Try to enable the collection, but untick it if not possible
        if (!CollectionControlModel::setEnabled(mModel->collection(ix), mAlarmType, true))
            mSelectionModel->select(ix, QItemSelectionModel::Deselect);
    }
    const QModelIndexList desel = deselected.indexes();
    foreach (const QModelIndex& ix, desel)
        CollectionControlModel::setEnabled(mModel->collection(ix), mAlarmType, false);
}

/******************************************************************************
* Called when a collection parameter or status has changed.
* If the collection's alarm types have been reconfigured, ensure that the
* model views are updated to reflect this.
*/
void CollectionCheckListModel::collectionStatusChanged(const Collection& collection, AkonadiModel::Change change, const QVariant&, bool inserted)
{
    if (inserted  ||  !collection.isValid())
        return;
    switch (change)
    {
        case AkonadiModel::Enabled:
            kDebug() << "Enabled" << collection.id();
            break;
        case AkonadiModel::AlarmTypes:
            kDebug() << "AlarmTypes" << collection.id();
            break;
        default:
            return;
    }
    const QModelIndex ix = mModel->collectionIndex(collection);
    if (ix.isValid())
        setSelectionStatus(collection, ix);
    if (change == AkonadiModel::AlarmTypes)
        emit collectionTypeChange(this);
}

/******************************************************************************
* Select or deselect an index according to its enabled status.
*/
void CollectionCheckListModel::setSelectionStatus(const Collection& collection, const QModelIndex& sourceIndex)
{
    const QItemSelectionModel::SelectionFlags sel = (collection.hasAttribute<CollectionAttribute>()
                                                     &&  collection.attribute<CollectionAttribute>()->isEnabled(mAlarmType))
                                                  ? QItemSelectionModel::Select : QItemSelectionModel::Deselect;
    mSelectionModel->select(sourceIndex, sel);
}


/*=============================================================================
= Class: CollectionFilterCheckListModel
= Proxy model providing a checkable collection list. The model contains all
= alarm types, but returns only one type at any given time. The selected alarm
= type may be changed as desired.
=============================================================================*/
CollectionFilterCheckListModel::CollectionFilterCheckListModel(QObject* parent)
    : QSortFilterProxyModel(parent),
      mActiveModel(new CollectionCheckListModel(CalEvent::ACTIVE, this)),
      mArchivedModel(new CollectionCheckListModel(CalEvent::ARCHIVED, this)),
      mTemplateModel(new CollectionCheckListModel(CalEvent::TEMPLATE, this)),
      mAlarmType(CalEvent::EMPTY)
{
    setDynamicSortFilter(true);
    connect(mActiveModel, SIGNAL(collectionTypeChange(CollectionCheckListModel*)), SLOT(collectionTypeChanged(CollectionCheckListModel*)));
    connect(mArchivedModel, SIGNAL(collectionTypeChange(CollectionCheckListModel*)), SLOT(collectionTypeChanged(CollectionCheckListModel*)));
    connect(mTemplateModel, SIGNAL(collectionTypeChange(CollectionCheckListModel*)), SLOT(collectionTypeChanged(CollectionCheckListModel*)));
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

QVariant CollectionFilterCheckListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::ToolTipRole:
        {
            const Collection col = collection(index);
            if (col.isValid())
                return AkonadiModel::instance()->tooltip(col, mAlarmType);
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
        if (qVariantCanConvert<QString>(value))
        {
            QString toolTip = value.toString();
            int i = toolTip.indexOf(QLatin1Char('@'));
            if (i > 0)
            {
                int j = toolTip.indexOf(QRegExp(QLatin1String("<(nl|br)"), Qt::CaseInsensitive), i + 1);
                int k = toolTip.indexOf(QLatin1Char('@'), j);
                const QString name = toolTip.mid(i + 1, j - i - 1);
                value = model()->data(index, Qt::FontRole);
                const QFontMetrics fm(qvariant_cast<QFont>(value).resolve(viewOptions().font));
                int textWidth = fm.boundingRect(name).width() + 1;
                const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
                QStyleOptionButton opt;
                opt.QStyleOption::operator=(viewOptions());
                opt.rect = rectForIndex(index);
                int checkWidth = QApplication::style()->subElementRect(QStyle::SE_ViewItemCheckIndicator, &opt).width();
                int left = spacing() + 3*margin + checkWidth + viewOptions().decorationSize.width();   // left offset of text
                int right = left + textWidth;
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

CollectionControlModel* CollectionControlModel::mInstance = 0;
bool                    CollectionControlModel::mAskDestination = false;

CollectionControlModel* CollectionControlModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionControlModel(qApp);
    return mInstance;
}

CollectionControlModel::CollectionControlModel(QObject* parent)
    : FavoriteCollectionsModel(AkonadiModel::instance(), KConfigGroup(KSharedConfig::openConfig(), "Collections"), parent),
      mPopulatedCheckLoop(0)
{
    // Initialise the list of enabled collections
    EntityMimeTypeFilterModel* filter = new EntityMimeTypeFilterModel(this);
    filter->addMimeTypeInclusionFilter(Collection::mimeType());
    filter->setSourceModel(AkonadiModel::instance());
    Collection::List collections;
    findEnabledCollections(filter, QModelIndex(), collections);
    setCollections(collections);

    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)),
                                      SLOT(statusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)));
#if KDE_IS_VERSION(4,9,80)
    connect(AkonadiModel::instance(), SIGNAL(collectionTreeFetched(Akonadi::Collection::List)),
                                      SLOT(collectionPopulated()));
    connect(AkonadiModel::instance(), SIGNAL(collectionPopulated(Akonadi::Collection::Id)),
                                      SLOT(collectionPopulated()));
#endif
}

/******************************************************************************
* Recursive function to check all collections' enabled status, and to compile a
* list of all collections which have any alarm types enabled.
* Collections which duplicate the same backend storage are filtered out, to
* avoid crashes due to duplicate events in different resources.
*/
void CollectionControlModel::findEnabledCollections(const EntityMimeTypeFilterModel* filter, const QModelIndex& parent, Collection::List& collections) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = filter->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = filter->index(row, 0, parent);
        const Collection collection = model->data(filter->mapToSource(ix), AkonadiModel::CollectionRole).value<Collection>();
        if (!AgentManager::self()->instance(collection.resource()).isValid())
            continue;    // the collection doesn't belong to a resource, so omit it
        const CalEvent::Types enabled = !collection.hasAttribute<CollectionAttribute>() ? CalEvent::EMPTY
                                           : collection.attribute<CollectionAttribute>()->enabled();
        const CalEvent::Types canEnable = checkTypesToEnable(collection, collections, enabled);
        if (canEnable != enabled)
        {
            // There is another collection which uses the same backend
            // storage. Disable alarm types enabled in the other collection.
            if (!model->isCollectionBeingDeleted(collection.id()))
                model->setData(model->collectionIndex(collection), static_cast<int>(canEnable), AkonadiModel::EnabledTypesRole);
        }
        if (canEnable)
            collections += collection;
        if (filter->rowCount(ix) > 0)
            findEnabledCollections(filter, ix, collections);
    }
}

bool CollectionControlModel::isEnabled(const Collection& collection, CalEvent::Type type)
{
    if (!collection.isValid()  ||  !instance()->collections().contains(collection))
        return false;
    if (!AgentManager::self()->instance(collection.resource()).isValid())
    {
        // The collection doesn't belong to a resource, so it can't be used.
        // Remove it from the list of collections.
        instance()->removeCollection(collection);
        return false;
    }
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    return col.hasAttribute<CollectionAttribute>()
       &&  col.attribute<CollectionAttribute>()->isEnabled(type);
}

/******************************************************************************
* Enable or disable the specified alarm types for a collection.
* Reply = alarm types which can be enabled
*/
CalEvent::Types CollectionControlModel::setEnabled(const Collection& collection, CalEvent::Types types, bool enabled)
{
    kDebug() << "id:" << collection.id() << ", alarm types" << types << "->" << enabled;
    if (!collection.isValid()  ||  (!enabled && !instance()->collections().contains(collection)))
        return CalEvent::EMPTY;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    CalEvent::Types alarmTypes = !col.hasAttribute<CollectionAttribute>() ? CalEvent::EMPTY
                                         : col.attribute<CollectionAttribute>()->enabled();
    if (enabled)
        alarmTypes |= static_cast<CalEvent::Types>(types & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE));
    else
        alarmTypes &= ~types;

    return instance()->setEnabledStatus(collection, alarmTypes, false);
}

/******************************************************************************
* Change the collection's enabled status.
* Add or remove the collection to/from the enabled list.
* Reply = alarm types which can be enabled
*/
CalEvent::Types CollectionControlModel::setEnabledStatus(const Collection& collection, CalEvent::Types types, bool inserted)
{
    kDebug() << "id:" << collection.id() << ", types=" << types;
    CalEvent::Types disallowedStdTypes(0);
    CalEvent::Types stdTypes(0);

    // Prevent the enabling of duplicate alarm types if another collection
    // uses the same backend storage.
    const Collection::List cols = collections();
    const CalEvent::Types canEnable = checkTypesToEnable(collection, cols, types);

    // Update the list of enabled collections
    if (canEnable)
    {
        bool inList = false;
        const Collection::List cols = collections();
        foreach (const Collection& c, cols)
        {
            if (c.id() == collection.id())
            {
                inList = true;
                break;
            }
        }
        if (!inList)
        {
            // It's a new collection.
            // Prevent duplicate standard collections being created for any alarm type.
            stdTypes = collection.hasAttribute<CollectionAttribute>()
                                ? collection.attribute<CollectionAttribute>()->standard()
                                : CalEvent::EMPTY;
            if (stdTypes)
            {
                foreach (const Collection& col, cols)
                {
                    Collection c(col);
                    AkonadiModel::instance()->refresh(c);    // update with latest data
                    if (c.isValid())
                    {
                        const CalEvent::Types t = stdTypes & CalEvent::types(c.contentMimeTypes());
                        if (t)
                        {
                            if (c.hasAttribute<CollectionAttribute>()
                            &&  AkonadiModel::isCompatible(c))
                            {
                                disallowedStdTypes |= c.attribute<CollectionAttribute>()->standard() & t;
                                if (disallowedStdTypes == stdTypes)
                                    break;
                            }
                        }
                    }
                }
            }
            addCollection(collection);
        }
    }
    else
        removeCollection(collection);

    if (disallowedStdTypes  ||  !inserted  ||  canEnable != types)
    {
        // Update the collection's status
        AkonadiModel* model = static_cast<AkonadiModel*>(sourceModel());
        if (!model->isCollectionBeingDeleted(collection.id()))
        {
            const QModelIndex ix = model->collectionIndex(collection);
            if (!inserted  ||  canEnable != types)
                model->setData(ix, static_cast<int>(canEnable), AkonadiModel::EnabledTypesRole);
            if (disallowedStdTypes)
                model->setData(ix, static_cast<int>(stdTypes & ~disallowedStdTypes), AkonadiModel::IsStandardRole);
        }
    }
    return canEnable;
}

/******************************************************************************
* Called when a collection parameter or status has changed.
* If it's the enabled status, add or remove the collection to/from the enabled
* list.
*/
void CollectionControlModel::statusChanged(const Collection& collection, AkonadiModel::Change change, const QVariant& value, bool inserted)
{
    if (!collection.isValid())
        return;

    switch (change)
    {
        case AkonadiModel::Enabled:
        {
            const CalEvent::Types enabled = static_cast<CalEvent::Types>(value.toInt());
            kDebug() << "id:" << collection.id() << ", enabled=" << enabled << ", inserted=" << inserted;
            setEnabledStatus(collection, enabled, inserted);
            break;
        }
        case AkonadiModel::ReadOnly:
        {
            bool readOnly = value.toBool();
            kDebug() << "id:" << collection.id() << ", readOnly=" << readOnly;
            if (readOnly)
            {
                // A read-only collection can't be the default for any alarm type
                const CalEvent::Types std = standardTypes(collection, false);
                if (std != CalEvent::EMPTY)
                {
                    Collection c(collection);
                    setStandard(c, CalEvent::Types(CalEvent::EMPTY));
                    QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
                    bool singleType = true;
                    QString msg;
                    switch (std)
                    {
                        case CalEvent::ACTIVE:
                            msg = i18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for active alarms.",
                                        collection.name());
                            break;
                        case CalEvent::ARCHIVED:
                            msg = i18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for archived alarms.",
                                        collection.name());
                            break;
                        case CalEvent::TEMPLATE:
                            msg = i18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for alarm templates.",
                                        collection.name());
                            break;
                        default:
                            msg = i18nc("@info", "<para>The calendar <resource>%1</resource> has been made read-only. "
                                                 "This was the default calendar for:%2</para>"
                                                 "<para>Please select new default calendars.</para>",
                                        collection.name(), typeListForDisplay(std));
                            singleType = false;
                            break;
                    }
                    if (singleType)
                        msg = i18nc("@info", "<para>%1</para><para>Please select a new default calendar.</para>", msg);
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
*   collection  - must be up to date (using AkonadiModel::refresh() etc.)
*   collections = list of collections to search for duplicates.
*   types       = alarm types to be enabled for the collection.
* Reply = alarm types which can be enabled without duplicating other collections.
*/
CalEvent::Types CollectionControlModel::checkTypesToEnable(const Collection& collection, const Collection::List& collections, CalEvent::Types types)
{
    types &= (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    if (types)
    {
        // At least on alarm type is to be enabled
        const KUrl location(collection.remoteId());
        foreach (const Collection& c, collections)
        {
            if (c.id() != collection.id()  &&  KUrl(c.remoteId()) == location)
            {
                // The collection duplicates the backend storage
                // used by another enabled collection.
                // N.B. don't refresh this collection - assume no change.
                if (c.hasAttribute<CollectionAttribute>())
                {
                    types &= ~c.attribute<CollectionAttribute>()->enabled();
                    if (!types)
                        break;
                }
            }
        }
    }
    return types;
}

/******************************************************************************
* Create a bulleted list of alarm types for insertion into <para>...</para>.
*/
QString CollectionControlModel::typeListForDisplay(CalEvent::Types alarmTypes)
{
    QString list;
    if (alarmTypes & CalEvent::ACTIVE)
        list += QLatin1String("<item>") + i18nc("@info/plain", "Active Alarms") + QLatin1String("</item>");
    if (alarmTypes & CalEvent::ARCHIVED)
        list += QLatin1String("<item>") + i18nc("@info/plain", "Archived Alarms") + QLatin1String("</item>");
    if (alarmTypes & CalEvent::TEMPLATE)
        list += QLatin1String("<item>") + i18nc("@info/plain", "Alarm Templates") + QLatin1String("</item>");
    if (!list.isEmpty())
        list = QLatin1String("<list>") + list + QLatin1String("</list>");
    return list;
}

/******************************************************************************
* Return whether a collection is both enabled and fully writable for a given
* alarm type.
* Optionally, the enabled status can be ignored.
* Reply: 1 = fully enabled and writable,
*        0 = enabled and writable except that backend calendar is in an old KAlarm format,
*       -1 = not enabled, read-only, or incompatible format.
*/
int CollectionControlModel::isWritableEnabled(const Akonadi::Collection& collection, CalEvent::Type type)
{
    KACalendar::Compat format;
    return isWritableEnabled(collection, type, format);
}
int CollectionControlModel::isWritableEnabled(const Akonadi::Collection& collection, CalEvent::Type type, KACalendar::Compat& format)
{
    int writable = AkonadiModel::isWritable(collection, format);
    if (writable == -1)
        return -1;

    // Check the collection's enabled status
    if (!instance()->collections().contains(collection)
    ||  !collection.hasAttribute<CollectionAttribute>())
        return -1;
    if (!collection.attribute<CollectionAttribute>()->isEnabled(type))
        return -1;
    return writable;
}

/******************************************************************************
* Return the standard collection for a specified mime type.
* If 'useDefault' is true and there is no standard collection, the only
* collection for the mime type will be returned as a default.
*/
Collection CollectionControlModel::getStandard(CalEvent::Type type, bool useDefault)
{
    const QString mimeType = CalEvent::mimeType(type);
    int defalt = -1;
    Collection::List cols = instance()->collections();
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        AkonadiModel::instance()->refresh(cols[i]);    // update with latest data
        if (cols[i].isValid()
        &&  cols[i].contentMimeTypes().contains(mimeType))
        {
            if (cols[i].hasAttribute<CollectionAttribute>()
            &&  (cols[i].attribute<CollectionAttribute>()->standard() & type)
            &&  AkonadiModel::isCompatible(cols[i]))
                return cols[i];
            defalt = (defalt == -1) ? i : -2;
        }
    }
    return (useDefault && defalt >= 0) ? cols[defalt] : Collection();
}

/******************************************************************************
* Return whether a collection is the standard collection for a specified
* mime type.
*/
bool CollectionControlModel::isStandard(Akonadi::Collection& collection, CalEvent::Type type)
{
    if (!instance()->collections().contains(collection))
        return false;
    AkonadiModel::instance()->refresh(collection);    // update with latest data
    if (!collection.hasAttribute<CollectionAttribute>()
    ||  !AkonadiModel::isCompatible(collection))
        return false;
    return collection.attribute<CollectionAttribute>()->isStandard(type);
}

/******************************************************************************
* Return the alarm type(s) for which a collection is the standard collection.
*/
CalEvent::Types CollectionControlModel::standardTypes(const Collection& collection, bool useDefault)
{
    if (!instance()->collections().contains(collection))
        return CalEvent::EMPTY;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if (!AkonadiModel::isCompatible(col))
        return CalEvent::EMPTY;
    CalEvent::Types stdTypes = col.hasAttribute<CollectionAttribute>()
                                     ? col.attribute<CollectionAttribute>()->standard()
                                     : CalEvent::EMPTY;
    if (useDefault)
    {
        // Also return alarm types for which this is the only collection.
        CalEvent::Types wantedTypes = AkonadiModel::types(collection) & ~stdTypes;
        Collection::List cols = instance()->collections();
        for (int i = 0, count = cols.count();  wantedTypes && i < count;  ++i)
        {
            if (cols[i] == col)
                continue;
            AkonadiModel::instance()->refresh(cols[i]);    // update with latest data
            if (cols[i].isValid())
                wantedTypes &= ~AkonadiModel::types(cols[i]);
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
void CollectionControlModel::setStandard(Akonadi::Collection& collection, CalEvent::Type type, bool standard)
{
    AkonadiModel* model = AkonadiModel::instance();
    model->refresh(collection);    // update with latest data
    if (!AkonadiModel::isCompatible(collection))
        standard = false;   // the collection isn't writable
    if (standard)
    {
        // The collection is being set as standard.
        // Clear the 'standard' status for all other collections.
        Collection::List cols = instance()->collections();
        if (!cols.contains(collection))
            return;
        const CalEvent::Types ctypes = collection.hasAttribute<CollectionAttribute>()
                                       ? collection.attribute<CollectionAttribute>()->standard() : CalEvent::EMPTY;
        if (ctypes & type)
            return;    // it's already the standard collection for this type
        for (int i = 0, count = cols.count();  i < count;  ++i)
        {
            CalEvent::Types types;
            if (cols[i] == collection)
            {
                cols[i] = collection;    // update with latest data
                types = ctypes | type;
            }
            else
            {
                model->refresh(cols[i]);    // update with latest data
                types = cols[i].hasAttribute<CollectionAttribute>()
                      ? cols[i].attribute<CollectionAttribute>()->standard() : CalEvent::EMPTY;
                if (!(types & type))
                    continue;
                types &= ~type;
            }
            const QModelIndex index = model->collectionIndex(cols[i]);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        CalEvent::Types types = collection.hasAttribute<CollectionAttribute>()
                                ? collection.attribute<CollectionAttribute>()->standard() : CalEvent::EMPTY;
        if (types & type)
        {
            types &= ~type;
            const QModelIndex index = model->collectionIndex(collection);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
}

/******************************************************************************
* Set which mime types a collection is the standard collection for.
* If it is being set as standard for any mime types, the standard status for
* those mime types is cleared for all other collections.
*/
void CollectionControlModel::setStandard(Akonadi::Collection& collection, CalEvent::Types types)
{
    AkonadiModel* model = AkonadiModel::instance();
    model->refresh(collection);    // update with latest data
    if (!AkonadiModel::isCompatible(collection))
        types = CalEvent::EMPTY;   // the collection isn't writable
    if (types)
    {
        // The collection is being set as standard for at least one mime type.
        // Clear the 'standard' status for all other collections.
        Collection::List cols = instance()->collections();
        if (!cols.contains(collection))
            return;
        const CalEvent::Types t = collection.hasAttribute<CollectionAttribute>()
                                  ? collection.attribute<CollectionAttribute>()->standard() : CalEvent::EMPTY;
        if (t == types)
            return;    // there's no change to the collection's status
        for (int i = 0, count = cols.count();  i < count;  ++i)
        {
            CalEvent::Types t;
            if (cols[i] == collection)
            {
                cols[i] = collection;    // update with latest data
                t = types;
            }
            else
            {
                model->refresh(cols[i]);    // update with latest data
                t = cols[i].hasAttribute<CollectionAttribute>()
                  ? cols[i].attribute<CollectionAttribute>()->standard() : CalEvent::EMPTY;
                if (!(t & types))
                    continue;
                t &= ~types;
            }
            const QModelIndex index = model->collectionIndex(cols[i]);
            model->setData(index, static_cast<int>(t), AkonadiModel::IsStandardRole);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        if (collection.hasAttribute<CollectionAttribute>()
        &&  collection.attribute<CollectionAttribute>()->standard())
        {
            const QModelIndex index = model->collectionIndex(collection);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
}

/******************************************************************************
* Get the collection to use for storing an alarm.
* Optionally, the standard collection for the alarm type is returned. If more
* than one collection is a candidate, the user is prompted.
*/
Collection CollectionControlModel::destination(CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Collection standard;
    if (type == CalEvent::EMPTY)
        return standard;
    standard = getStandard(type);
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == CalEvent::ARCHIVED  ||  noPrompt  ||  (!mAskDestination  &&  standard.isValid()))
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
            dlg->setCaption(i18nc("@title:window", "Choose Calendar"));
            dlg->setDefaultCollection(standard);
            dlg->setMimeTypeFilter(QStringList(CalEvent::mimeType(type)));
            if (dlg->exec())
                col = dlg->selectedCollection();
            if (!col.isValid()  &&  cancelled)
                *cancelled = true;
        }
    }
    return col;
}

/******************************************************************************
* Return the enabled collections which contain a specified mime type.
* If 'writable' is true, only writable collections are included.
*/
Collection::List CollectionControlModel::enabledCollections(CalEvent::Type type, bool writable)
{
    const QString mimeType = CalEvent::mimeType(type);
    Collection::List cols = instance()->collections();
    Collection::List result;
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        AkonadiModel::instance()->refresh(cols[i]);    // update with latest data
        if (cols[i].contentMimeTypes().contains(mimeType)
        &&  (!writable || ((cols[i].rights() & writableRights) == writableRights)))
            result += cols[i];
    }
    return result;
}

/******************************************************************************
* Return the collection ID for a given resource ID.
*/
Collection CollectionControlModel::collectionForResource(const QString& resourceId)
{
    const Collection::List cols = instance()->collections();
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        if (cols[i].resource() == resourceId)
            return cols[i];
    }
    return Collection();
}

#if KDE_IS_VERSION(4,9,80)
/******************************************************************************
* Return whether all enabled collections have been populated.
*/
bool CollectionControlModel::isPopulated(Collection::Id colId)
{
    AkonadiModel* model = AkonadiModel::instance();
    Collection::List cols = instance()->collections();
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        if ((colId == -1  ||  colId == cols[i].id())
        &&  !model->data(model->collectionIndex(cols[i].id()), AkonadiModel::IsPopulatedRole).toBool())
        {
            model->refresh(cols[i]);    // update with latest data
            if (!cols[i].hasAttribute<CollectionAttribute>()
            ||  cols[i].attribute<CollectionAttribute>()->enabled() == CalEvent::EMPTY)
                return false;
        }
    }
    return true;
}

/******************************************************************************
* Wait for one or all enabled collections to be populated.
* Reply = true if successful.
*/
bool CollectionControlModel::waitUntilPopulated(Collection::Id colId, int timeout)
{
    kDebug();
    int result = 1;
    AkonadiModel* model = AkonadiModel::instance();
    while (!model->isCollectionTreeFetched()
       ||  !isPopulated(colId))
    {
        if (!mPopulatedCheckLoop)
            mPopulatedCheckLoop = new QEventLoop(this);
        if (timeout > 0)
            QTimer::singleShot(timeout * 1000, mPopulatedCheckLoop, SLOT(quit()));
        result = mPopulatedCheckLoop->exec();
    }
    delete mPopulatedCheckLoop;
    mPopulatedCheckLoop = 0;
    return result;
}
#endif

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
