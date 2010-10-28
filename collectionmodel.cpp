/*
 *  collectionmodel.cpp  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2007-2010 by David Jarvie <djarvie@kde.org>
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

static Collection::Rights writableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;


/*=============================================================================
= Class: CollectionMimeTypeFilterModel
= Proxy model to restrict its contents to Collections, not Items, containing
= specified content mime types.
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

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private:
        QString mMimeType;     // collection content type contained in this model
        bool    mWritableOnly; // only include writable collections in this model
        bool    mEnabledOnly;  // only include enabled collections in this model
};

CollectionMimeTypeFilterModel::CollectionMimeTypeFilterModel(QObject* parent)
    : EntityMimeTypeFilterModel(parent),
      mMimeType(),
      mWritableOnly(false),
      mEnabledOnly(false)
{
    addMimeTypeInclusionFilter(Collection::mimeType());
    setHeaderGroup(EntityTreeModel::CollectionTreeHeaders);
    setSourceModel(AkonadiModel::instance());
}

void CollectionMimeTypeFilterModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    if (mimeType != mMimeType)
    {
        mMimeType = mimeType;
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
    if (!mWritableOnly  &&  mMimeType.isEmpty())
        return true;
    AkonadiModel* model = AkonadiModel::instance();
    QModelIndex ix = model->index(sourceRow, 0, sourceParent);
    Collection collection = model->data(ix, AkonadiModel::CollectionRole).value<Collection>();
    if (mWritableOnly  &&  collection.rights() == Collection::ReadOnly)
        return false;
    if (!mMimeType.isEmpty()  &&  !collection.contentMimeTypes().contains(mMimeType))
        return false;
    if (mEnabledOnly)
    {
        if (!collection.hasAttribute<CollectionAttribute>()
        ||  !collection.attribute<CollectionAttribute>()->isEnabled())
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


/*=============================================================================
= Class: CollectionListModel
= Proxy model converting the collection tree into a flat list.
= The model may be restricted to specified content mime types.
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
//    return AkonadiModel::instance()->data(mapToSource(index(row, 0)), EntityTreeModel::CollectionRole).value<Collection>();
    return data(index(row, 0), EntityTreeModel::CollectionRole).value<Collection>();
}

Collection CollectionListModel::collection(const QModelIndex& index) const
{
//    return AkonadiModel::instance()->data(mapToSource(index), EntityTreeModel::CollectionRole).value<Collection>();
    return data(index, EntityTreeModel::CollectionRole).value<Collection>();
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
= Proxy model providing a checkable collection list.
=============================================================================*/

CollectionCheckListModel* CollectionCheckListModel::mInstance = 0;

CollectionCheckListModel* CollectionCheckListModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionCheckListModel(qApp);
    return mInstance;
}

CollectionCheckListModel::CollectionCheckListModel(QObject* parent)
    : Future::KCheckableProxyModel(parent)
{
    CollectionListModel* model = new CollectionListModel(this);
    setSourceModel(model);
    mSelectionModel = new QItemSelectionModel(model);
    setSelectionModel(mSelectionModel);
    connect(mSelectionModel, SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                             SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));
    connect(model, SIGNAL(rowsInserted(const QModelIndex&, int, int)), SLOT(slotRowsInserted(const QModelIndex&, int, int)));
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionCheckListModel::collection(int row) const
{
    return static_cast<CollectionListModel*>(sourceModel())->collection(mapToSource(index(row, 0)));
}

Collection CollectionCheckListModel::collection(const QModelIndex& index) const
{
    return static_cast<CollectionListModel*>(sourceModel())->collection(mapToSource(index));
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
        const Collection collection = static_cast<CollectionListModel*>(sourceModel())->collection(index);
        if (collection.isValid()  &&  collection.hasAttribute<CollectionAttribute>())
        {
            const CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
            if (attr->isEnabled())
            {
                QString errmsg;
                QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
                if (attr->standard() != KAlarm::CalEvent::EMPTY)
                {
                    // It's the standard collection for some alarm type.
                    if (attr->isStandard(KAlarm::CalEvent::ACTIVE))
                    {
                        errmsg = i18nc("@info", "You cannot disable your default active alarm calendar.");
                    }
                    else if (attr->isStandard(KAlarm::CalEvent::ARCHIVED)  &&  Preferences::archivedKeepDays())
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
    return Future::KCheckableProxyModel::setData(index, value, role);
}

/******************************************************************************
* Called when rows have been inserted into the model.
* Select or deselect them according to their enabled status.
*/
void CollectionCheckListModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    CollectionListModel* model = static_cast<CollectionListModel*>(sourceModel());
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = mapToSource(index(row, 0, parent));
        const Collection collection = model->collection(ix);
        if (collection.isValid())
        {
            QItemSelectionModel::SelectionFlags sel = (collection.hasAttribute<CollectionAttribute>()
                                                       &&  collection.attribute<CollectionAttribute>()->isEnabled())
                                                    ? QItemSelectionModel::Select : QItemSelectionModel::Deselect;
            mSelectionModel->select(ix, sel);
        }
    }
}

/******************************************************************************
* Called when the user has ticked/unticked a collection to enable/disable it.
*/
void CollectionCheckListModel::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const QModelIndexList sel = selected.indexes();
    foreach (const QModelIndex& ix, sel)
        CollectionControlModel::setEnabled(static_cast<CollectionListModel*>(sourceModel())->collection(ix), true);
    const QModelIndexList desel = deselected.indexes();
    foreach (const QModelIndex& ix, desel)
        CollectionControlModel::setEnabled(static_cast<CollectionListModel*>(sourceModel())->collection(ix), false);
}


/*=============================================================================
= Class: CollectionFilterCheckListModel
= Proxy model providing a checkable collection list, filtered by mime type.
=============================================================================*/
CollectionFilterCheckListModel::CollectionFilterCheckListModel(QObject* parent)
    : QSortFilterProxyModel(parent),
      mMimeType()
{
    setSourceModel(CollectionCheckListModel::instance());
}

void CollectionFilterCheckListModel::setEventTypeFilter(KAlarm::CalEvent::Type type)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    if (mimeType != mMimeType)
    {
        mMimeType = mimeType;
        invalidateFilter();
    }
}

/******************************************************************************
* Return the collection for a given row.
*/
Collection CollectionFilterCheckListModel::collection(int row) const
{
    return CollectionCheckListModel::instance()->collection(mapToSource(index(row, 0)));
}

Collection CollectionFilterCheckListModel::collection(const QModelIndex& index) const
{
    return CollectionCheckListModel::instance()->collection(mapToSource(index));
}

bool CollectionFilterCheckListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (mMimeType.isEmpty())
        return true;
    CollectionCheckListModel* model = CollectionCheckListModel::instance();
    const Collection collection = model->collection(model->index(sourceRow, 0, sourceParent));
    return collection.contentMimeTypes().contains(mMimeType);
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
    model->setData(QModelIndex(), viewOptions().font, Qt::FontRole);
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
        QVariant value = model()->data(index, Qt::ToolTipRole);
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

    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool)),
                                      SLOT(statusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool)));
}

/******************************************************************************
* Recursive function to check all collections' enabled status.
*/
void CollectionControlModel::findEnabledCollections(const EntityMimeTypeFilterModel* filter, const QModelIndex& parent, Collection::List& collections) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = filter->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = filter->index(row, 0, parent);
        const Collection collection = model->data(filter->mapToSource(ix), AkonadiModel::CollectionRole).value<Collection>();
        if (collection.hasAttribute<CollectionAttribute>()
        &&  collection.attribute<CollectionAttribute>()->isEnabled())
            collections += collection;
        if (filter->rowCount(ix) > 0)
            findEnabledCollections(filter, ix, collections);
    }
}

bool CollectionControlModel::isEnabled(const Collection& collection)
{
    return collection.isValid()  &&  instance()->collections().contains(collection);
}

void CollectionControlModel::setEnabled(const Collection& collection, bool enabled)
{
    instance()->statusChanged(collection, AkonadiModel::Enabled, enabled);
}

void CollectionControlModel::statusChanged(const Collection& collection, AkonadiModel::Change change, bool value)
{
    if (change == AkonadiModel::Enabled)
    {
        if (collection.isValid())
        {
            if (value)
            {
                const Collection::List cols = collections();
                foreach (const Collection& c, cols)
                {
                    if (c.id() == collection.id())
                        return;
                }
                addCollection(collection);
            }
            else
                removeCollection(collection);
            AkonadiModel* model = static_cast<AkonadiModel*>(sourceModel());
            model->setData(model->collectionIndex(collection), value, AkonadiModel::EnabledRole);
        }
    }
}

/******************************************************************************
* Return whether a collection is both enabled and fully writable.
* Optionally, the enabled status can be ignored.
*/
bool CollectionControlModel::isWritable(const Akonadi::Collection& collection, bool ignoreEnabledStatus)
{
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if (!col.hasAttribute<CollectionAttribute>()
    ||  col.attribute<CollectionAttribute>()->compatibility() != KAlarm::Calendar::Current)
        return false;
    return (ignoreEnabledStatus || isEnabled(col))
       &&  (col.rights() & writableRights) == writableRights;
}

/******************************************************************************
* Return the standard collection for a specified mime type.
*/
Collection CollectionControlModel::getStandard(KAlarm::CalEvent::Type type)
{
    QString mimeType = KAlarm::CalEvent::mimeType(type);
    Collection::List cols = instance()->collections();
    for (int i = 0, count = cols.count();  i < count;  ++i)
    {
        AkonadiModel::instance()->refresh(cols[i]);    // update with latest data
        if (cols[i].isValid()
        &&  cols[i].contentMimeTypes().contains(mimeType)
        &&  cols[i].hasAttribute<CollectionAttribute>()
        &&  (cols[i].attribute<CollectionAttribute>()->standard() & type))
            return cols[i];
    }
    return Collection();
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
    if (!collection.hasAttribute<CollectionAttribute>())
        return false;
    return collection.attribute<CollectionAttribute>()->isStandard(type);
}

/******************************************************************************
* Return the alarm type(s) for which a collection is the standard collection.
*/
KAlarm::CalEvent::Types CollectionControlModel::standardTypes(const Collection& collection)
{
    if (!instance()->collections().contains(collection))
        return KAlarm::CalEvent::EMPTY;
    Collection col = collection;
    AkonadiModel::instance()->refresh(col);    // update with latest data
    if (!col.hasAttribute<CollectionAttribute>())
        return KAlarm::CalEvent::EMPTY;
    return col.attribute<CollectionAttribute>()->standard();
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
