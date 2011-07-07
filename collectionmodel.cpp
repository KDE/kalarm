/*
 *  collectionmodel.cpp  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2007-2011 by David Jarvie <djarvie@kde.org>
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
#include "collectionattribute.h"
#include "compatibilityattribute.h"
#include "preferences.h"

#include <akonadi/collectiondialog.h>
#include <akonadi/collectiondeletejob.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/entitymimetypefiltermodel.h>

#include <klocale.h>
#include <kmessagebox.h>

#include <QApplication>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QObject>

using namespace Akonadi;
using KAlarm::CollectionAttribute;
using KAlarm::CompatibilityAttribute;

static Collection::Rights writableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;


/*=============================================================================
= Class: CollectionMimeTypeFilterModel
= Proxy model to filter AkonadiModel to restrict its contents to Collections,
= not Items, containing specified content mime types.
= It can optionally be restricted to writable and/or enabled Collections.
=============================================================================*/
class CollectionMimeTypeFilterModel : public Akonadi::EntityMimeTypeFilterModel
{
        Q_OBJECT
    public:
        explicit CollectionMimeTypeFilterModel(QObject* parent = 0);
        void setEventTypeFilter(KAlarm::CalEvent::Type);
        void setFilterWritable(bool writable);
        void setFilterEnabled(bool enabled);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        QModelIndex collectionIndex(const Akonadi::Collection&) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private:
        KAlarm::CalEvent::Type mAlarmType;  // collection content type contained in this model
        bool    mWritableOnly; // only include writable collections in this model
        bool    mEnabledOnly;  // only include enabled collections in this model
};

CollectionMimeTypeFilterModel::CollectionMimeTypeFilterModel(QObject* parent)
    : EntityMimeTypeFilterModel(parent),
      mAlarmType(KAlarm::CalEvent::EMPTY),
      mWritableOnly(false),
      mEnabledOnly(false)
{
    addMimeTypeInclusionFilter(Collection::mimeType());
    setHeaderGroup(EntityTreeModel::CollectionTreeHeaders);
    setSourceModel(AkonadiModel::instance());
}

void CollectionMimeTypeFilterModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
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
    if (!mWritableOnly  &&  mAlarmType == KAlarm::CalEvent::EMPTY)
        return true;
    AkonadiModel* model = AkonadiModel::instance();
    QModelIndex ix = model->index(sourceRow, 0, sourceParent);
    Collection collection = model->data(ix, AkonadiModel::CollectionRole).value<Collection>();
    if (mWritableOnly  &&  (collection.rights() & writableRights) != writableRights)
        return false;
    if (mAlarmType != KAlarm::CalEvent::EMPTY  &&  !collection.contentMimeTypes().contains(KAlarm::CalEvent::mimeType(mAlarmType)))
        return false;
    if (mEnabledOnly)
    {
        if (!collection.hasAttribute<CollectionAttribute>()
        ||  !collection.attribute<CollectionAttribute>()->isEnabled(mAlarmType))
            return false;
    }
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

void CollectionListModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
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

CollectionCheckListModel::CollectionCheckListModel(KAlarm::CalEvent::Type type, QObject* parent)
    : KCheckableProxyModel(parent),
      mAlarmType(type)
{
    if (!mModel)
        mModel = new CollectionListModel(this);
    setSourceModel(mModel);    // the source model is NOT filtered by alarm type
    mSelectionModel = new QItemSelectionModel(mModel);
    setSelectionModel(mSelectionModel);
    connect(mSelectionModel, SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                             SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));
    connect(mModel, SIGNAL(rowsAboutToBeInserted(const QModelIndex&, int, int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, SIGNAL(rowsInserted(const QModelIndex&, int, int)), SLOT(slotRowsInserted(const QModelIndex&, int, int)));
    // This is probably needed to make CollectionFilterCheckListModel update
    // (similarly to when rows are inserted).
    connect(mModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, SIGNAL(rowsRemoved(const QModelIndex&, int, int)), SIGNAL(layoutChanged()));

    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant&, bool)),
                                      SLOT(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant&, bool)));
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
            case Qt::FontRole:
            {
                if (!collection.hasAttribute<CollectionAttribute>()
                ||  !AkonadiModel::isCompatible(collection))
                    break;
                CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
                if (!attr->enabled())
                    break;
                QStringList mimeTypes = collection.contentMimeTypes();
                if (attr->isStandard(mAlarmType)  &&  mimeTypes.contains(KAlarm::CalEvent::mimeType(mAlarmType)))
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
                    if (mAlarmType == KAlarm::CalEvent::ACTIVE)
                    {
                        errmsg = i18nc("@info", "You cannot disable your default active alarm calendar.");
                    }
                    else if (mAlarmType == KAlarm::CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
                    {
                        // Only allow the archived alarms standard collection to be disabled if
                        // we're not saving expired alarms.
                        errmsg = i18nc("@info", "You cannot disable your default archived alarm calendar "
                                                "while expired alarms are configured to be kept.");
                    }
                    else if (KMessageBox::warningContinueCancel(messageParent,
                                                           i18nc("@info", "Do you really want to disable your default calendar?"))
                               == KMessageBox::Cancel)
                        return false;
                }
                if (!errmsg.isEmpty())
                {
                    KMessageBox::sorry(messageParent, errmsg);
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
        CollectionControlModel::setEnabled(mModel->collection(ix), mAlarmType, true);
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
    QModelIndex ix = mModel->collectionIndex(collection);
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
    QItemSelectionModel::SelectionFlags sel = (collection.hasAttribute<CollectionAttribute>()
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
      mActiveModel(new CollectionCheckListModel(KAlarm::CalEvent::ACTIVE, this)),
      mArchivedModel(new CollectionCheckListModel(KAlarm::CalEvent::ARCHIVED, this)),
      mTemplateModel(new CollectionCheckListModel(KAlarm::CalEvent::TEMPLATE, this)),
      mAlarmType(KAlarm::CalEvent::EMPTY)
{
    setDynamicSortFilter(true);
    connect(mActiveModel, SIGNAL(collectionTypeChange(CollectionCheckListModel*)), SLOT(collectionTypeChanged(CollectionCheckListModel*)));
    connect(mArchivedModel, SIGNAL(collectionTypeChange(CollectionCheckListModel*)), SLOT(collectionTypeChanged(CollectionCheckListModel*)));
    connect(mTemplateModel, SIGNAL(collectionTypeChange(CollectionCheckListModel*)), SLOT(collectionTypeChanged(CollectionCheckListModel*)));
}

void CollectionFilterCheckListModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
{
    if (type != mAlarmType)
    {
        CollectionCheckListModel* newModel;
        switch (type)
        {
            case KAlarm::CalEvent::ACTIVE:    newModel = mActiveModel;  break;
            case KAlarm::CalEvent::ARCHIVED:  newModel = mArchivedModel;  break;
            case KAlarm::CalEvent::TEMPLATE:  newModel = mTemplateModel;  break;
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
    if (mAlarmType == KAlarm::CalEvent::EMPTY)
        return true;
    CollectionCheckListModel* model = static_cast<CollectionCheckListModel*>(sourceModel());
    const Collection collection = model->collection(model->index(sourceRow, 0, sourceParent));
    return collection.contentMimeTypes().contains(KAlarm::CalEvent::mimeType(mAlarmType));
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
        QHelpEvent* he = static_cast<QHelpEvent*>(e);
        QModelIndex index = indexAt(he->pos());
        QVariant value = static_cast<CollectionFilterCheckListModel*>(model())->data(index, Qt::ToolTipRole);
        if (qVariantCanConvert<QString>(value))
        {
            QString toolTip = value.toString();
            int i = toolTip.indexOf('@');
            if (i > 0)
            {
                int j = toolTip.indexOf(QRegExp("<(nl|br)", Qt::CaseInsensitive), i + 1);
                int k = toolTip.indexOf('@', j);
                QString name = toolTip.mid(i + 1, j - i - 1);
                value = model()->data(index, Qt::FontRole);
                QFontMetrics fm(qvariant_cast<QFont>(value).resolve(viewOptions().font));
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
    : FavoriteCollectionsModel(AkonadiModel::instance(), KConfigGroup(KGlobal::config(), "Collections"), parent)
{
    // Initialise the list of enabled collections
    EntityMimeTypeFilterModel* filter = new EntityMimeTypeFilterModel(this);
    filter->addMimeTypeInclusionFilter(Collection::mimeType());
    filter->setSourceModel(AkonadiModel::instance());
    Collection::List collections;
    findEnabledCollections(filter, QModelIndex(), collections);
    setCollections(collections);

    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant&, bool)),
                                      SLOT(statusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant&, bool)));
}

/******************************************************************************
* Recursive function to check all collections' enabled status, and to compile a
* list of all collections which have any alarm types enabled.
*/
void CollectionControlModel::findEnabledCollections(const EntityMimeTypeFilterModel* filter, const QModelIndex& parent, Collection::List& collections) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = filter->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = filter->index(row, 0, parent);
        const Collection collection = model->data(filter->mapToSource(ix), AkonadiModel::CollectionRole).value<Collection>();
        if (collection.hasAttribute<CollectionAttribute>()
        &&  collection.attribute<CollectionAttribute>()->enabled())
            collections += collection;
        if (filter->rowCount(ix) > 0)
            findEnabledCollections(filter, ix, collections);
    }
}

bool CollectionControlModel::isEnabled(const Collection& collection, KAlarm::CalEvent::Type type)
{
    if (!collection.isValid()  ||  !instance()->collections().contains(collection))
        return false;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    return col.hasAttribute<CollectionAttribute>()
       &&  col.attribute<CollectionAttribute>()->isEnabled(type);
}

void CollectionControlModel::setEnabled(const Collection& collection, KAlarm::CalEvent::Types types, bool enabled)
{
    kDebug() << "id:" << collection.id() << ", alarm types" << types << "->" << enabled;
    if (!collection.isValid()  ||  (!enabled && !instance()->collections().contains(collection)))
        return;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    KAlarm::CalEvent::Types alarmTypes = !col.hasAttribute<CollectionAttribute>() ? KAlarm::CalEvent::EMPTY
                                         : col.attribute<CollectionAttribute>()->enabled();
    if (enabled)
        alarmTypes |= static_cast<KAlarm::CalEvent::Types>(types & KAlarm::CalEvent::ALL);
    else
        alarmTypes &= ~types;
    instance()->statusChanged(collection, AkonadiModel::Enabled, static_cast<int>(alarmTypes), false);
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

    if (change == AkonadiModel::Enabled)
    {
        KAlarm::CalEvent::Types enabled = static_cast<KAlarm::CalEvent::Types>(value.toInt());
        kDebug() << "id:" << collection.id() << ", enabled=" << enabled;

        // Update the list of enabled collections
        if (enabled)
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
                addCollection(collection);
        }
        else
            removeCollection(collection);

        if (!inserted)
        {
            // Update the collection's status
            AkonadiModel* model = static_cast<AkonadiModel*>(sourceModel());
            if (!model->isCollectionBeingDeleted(collection.id()))
                model->setData(model->collectionIndex(collection), static_cast<int>(enabled), AkonadiModel::EnabledRole);
        }
    }
}

/******************************************************************************
* Return whether a collection is both enabled and fully writable for a given
* alarm type.
* Optionally, the enabled status can be ignored.
*/
bool CollectionControlModel::isWritable(const Akonadi::Collection& collection, KAlarm::CalEvent::Type type, bool ignoreEnabledStatus)
{
    KAlarm::Calendar::Compat format;
    return isWritable(collection, type, format, ignoreEnabledStatus);
}
bool CollectionControlModel::isWritable(const Akonadi::Collection& collection, KAlarm::CalEvent::Type type, KAlarm::Calendar::Compat& format, bool ignoreEnabledStatus)
{
    format = KAlarm::Calendar::Current;
    if (!collection.isValid())
        return false;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if ((col.rights() & writableRights) != writableRights)
        return false;
    if (!col.hasAttribute<CompatibilityAttribute>())
    {
        format = KAlarm::Calendar::Incompatible;
        return false;
    }
    format = col.attribute<CompatibilityAttribute>()->compatibility();
    if (format != KAlarm::Calendar::Current)
        return false;
    if (ignoreEnabledStatus)
        return true;

    // Check the collection's enabled status
    if (!instance()->collections().contains(col)
    ||  !col.hasAttribute<CollectionAttribute>())
        return false;
    return col.attribute<CollectionAttribute>()->isEnabled(type);
}

/******************************************************************************
* Return the standard collection for a specified mime type.
* If 'useDefault' is true and there is no standard collection, the only
* collection for the mime type will be returned as a default.
*/
Collection CollectionControlModel::getStandard(KAlarm::CalEvent::Type type, bool useDefault)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
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
bool CollectionControlModel::isStandard(Akonadi::Collection& collection, KAlarm::CalEvent::Type type)
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
KAlarm::CalEvent::Types CollectionControlModel::standardTypes(const Collection& collection, bool useDefault)
{
    if (!instance()->collections().contains(collection))
        return KAlarm::CalEvent::EMPTY;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if (!AkonadiModel::isCompatible(col))
        return KAlarm::CalEvent::EMPTY;
    KAlarm::CalEvent::Types stdTypes = col.hasAttribute<CollectionAttribute>()
                                     ? col.attribute<CollectionAttribute>()->standard()
                                     : KAlarm::CalEvent::EMPTY;
    if (useDefault)
    {
        // Also return alarm types for which this is the only collection.
        KAlarm::CalEvent::Types wantedTypes = AkonadiModel::types(collection) & ~stdTypes;
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
void CollectionControlModel::setStandard(Akonadi::Collection& collection, KAlarm::CalEvent::Type type, bool standard)
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
        KAlarm::CalEvent::Types ctypes = collection.hasAttribute<CollectionAttribute>()
                                 ? collection.attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
        if (ctypes & type)
            return;    // it's already the standard collection for this type
        for (int i = 0, count = cols.count();  i < count;  ++i)
        {
            KAlarm::CalEvent::Types types;
            if (cols[i] == collection)
            {
                cols[i] = collection;    // update with latest data
                types = ctypes | type;
            }
            else
            {
                model->refresh(cols[i]);    // update with latest data
                types = cols[i].hasAttribute<CollectionAttribute>()
                      ? cols[i].attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
                if (!(types & type))
                    continue;
                types &= ~type;
            }
            QModelIndex index = model->collectionIndex(cols[i]);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
    else
    {
        // The 'standard' status is being cleared for the collection.
        // The collection doesn't have to be in this model's list of collections.
        KAlarm::CalEvent::Types types = collection.hasAttribute<CollectionAttribute>()
                                ? collection.attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
        if (types & type)
        {
            types &= ~type;
            QModelIndex index = model->collectionIndex(collection);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
}

/******************************************************************************
* Set which mime types a collection is the standard collection for.
* If it is being set as standard for any mime types, the standard status for
* those mime types is cleared for all other collections.
*/
void CollectionControlModel::setStandard(Akonadi::Collection& collection, KAlarm::CalEvent::Types types)
{
    AkonadiModel* model = AkonadiModel::instance();
    model->refresh(collection);    // update with latest data
    if (!AkonadiModel::isCompatible(collection))
        types = KAlarm::CalEvent::EMPTY;   // the collection isn't writable
    if (types)
    {
        // The collection is being set as standard for at least one mime type.
        // Clear the 'standard' status for all other collections.
        Collection::List cols = instance()->collections();
        if (!cols.contains(collection))
            return;
        KAlarm::CalEvent::Types t = collection.hasAttribute<CollectionAttribute>()
                            ? collection.attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
        if (t == types)
            return;    // there's no change to the collection's status
        for (int i = 0, count = cols.count();  i < count;  ++i)
        {
            KAlarm::CalEvent::Types t;
            if (cols[i] == collection)
            {
                cols[i] = collection;    // update with latest data
                t = types;
            }
            else
            {
                model->refresh(cols[i]);    // update with latest data
                t = cols[i].hasAttribute<CollectionAttribute>()
                  ? cols[i].attribute<CollectionAttribute>()->standard() : KAlarm::CalEvent::EMPTY;
                if (!(t & types))
                    continue;
                t &= ~types;
            }
            QModelIndex index = model->collectionIndex(cols[i]);
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
            QModelIndex index = model->collectionIndex(collection);
            model->setData(index, static_cast<int>(types), AkonadiModel::IsStandardRole);
        }
    }
}

/******************************************************************************
* Get the collection to use for storing an alarm.
* Optionally, the standard collection for the alarm type is returned. If more
* than one collection is a candidate, the user is prompted.
*/
Collection CollectionControlModel::destination(KAlarm::CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Collection standard;
    if (type == KAlarm::CalEvent::EMPTY)
        return standard;
    standard = getStandard(type);
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == KAlarm::CalEvent::ARCHIVED  ||  noPrompt  ||  (!mAskDestination  &&  standard.isValid()))
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
            dlg->setMimeTypeFilter(QStringList(KAlarm::CalEvent::mimeType(type)));
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
Collection::List CollectionControlModel::enabledCollections(KAlarm::CalEvent::Type type, bool writable)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
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
* Return the data for a given role, for a specified item.
*/
QVariant CollectionControlModel::data(const QModelIndex& index, int role) const
{
    return sourceModel()->data(mapToSource(index), role);
}

#include "collectionmodel.moc"
#include "moc_collectionmodel.cpp"

// vim: et sw=4:
