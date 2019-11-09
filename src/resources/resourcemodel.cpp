/*
 *  resourcemodel.cpp  -  models containing flat list of resources
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

#include "resourcemodel.h"

#include "calendardatamodel.h"
#include "messagebox.h"
#include "preferences.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QApplication>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>

/*============================================================================*/

ResourceFilterModel::ResourceFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void ResourceFilterModel::setEventTypeFilter(CalEvent::Type type)
{
    if (type != mAlarmType)
    {
        mAlarmType = type;
        invalidateFilter();
    }
}

void ResourceFilterModel::setFilterWritable(bool writable)
{
    if (writable != mWritableOnly)
    {
        mWritableOnly = writable;
        invalidateFilter();
    }
}

void ResourceFilterModel::setFilterEnabled(bool enabled)
{
    if (enabled != mEnabledOnly)
    {
        Q_EMIT layoutAboutToBeChanged();
        mEnabledOnly = enabled;
        invalidateFilter();
        Q_EMIT layoutChanged();
    }
}

void ResourceFilterModel::setFilterText(const QString& text)
{
    if (text != mFilterText)
    {
        Q_EMIT layoutAboutToBeChanged();
        mFilterText = text;
        invalidateFilter();
        Q_EMIT layoutChanged();
    }
}

QModelIndex ResourceFilterModel::resourceIndex(const Resource& resource) const
{
    if (!mResourceIndexFunction)
        return QModelIndex();
    return mapFromSource((*mResourceIndexFunction)(resource));
}

bool ResourceFilterModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

bool ResourceFilterModel::canFetchMore(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return false;
}

QModelIndexList ResourceFilterModel::match(const QModelIndex& start, int role, const QVariant& value, int hits, Qt::MatchFlags flags) const
{
    if (role < Qt::UserRole)
        return QSortFilterProxyModel::match(start, role, value, hits, flags);

    QModelIndexList matches;
    const QModelIndexList indexes = sourceModel()->match(mapToSource(start), role, value, hits, flags);
    for (const QModelIndex& ix : indexes)
    {
        QModelIndex proxyIndex = mapFromSource(ix);
        if (proxyIndex.isValid())
            matches += proxyIndex;
    }
    return matches;
}

int ResourceFilterModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 1;
}

bool ResourceFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex ix = sourceModel()->index(sourceRow, 0, sourceParent);
    const ResourceId id = sourceModel()->data(ix, CalendarDataModel::ResourceIdRole).toLongLong();
    if (id < 0)
        return false;   // this row doesn't contain a resource
    const Resource resource = Resources::resource(id);
    if (!resource.isValid())
        return false;   // invalidly configured resource
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
    if (!mFilterText.isEmpty()  &&  !resource.displayName().contains(mFilterText, Qt::CaseInsensitive))
        return false;
    return true;
}

bool ResourceFilterModel::filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const
{
    if (sourceColumn > 0)
        return false;
    return QSortFilterProxyModel::filterAcceptsColumn(sourceColumn, sourceParent);
}


/*============================================================================*/

ResourceListModel::ResourceListModel(QObject* parent)
    : KDescendantsProxyModel(parent)
{
    setDisplayAncestorData(false);
}

/******************************************************************************
* Return the resource for a given row.
*/
Resource ResourceListModel::resource(int row) const
{
    const ResourceId id = data(index(row, 0), CalendarDataModel::ResourceIdRole).toLongLong();
    return Resources::resource(id);
}

Resource ResourceListModel::resource(const QModelIndex& index) const
{
    const ResourceId id = data(index, CalendarDataModel::ResourceIdRole).toLongLong();
    return Resources::resource(id);
}

QModelIndex ResourceListModel::resourceIndex(const Resource& resource) const
{
    return mapFromSource(static_cast<ResourceFilterModel*>(sourceModel())->resourceIndex(resource));
}

void ResourceListModel::setEventTypeFilter(CalEvent::Type type)
{
    static_cast<ResourceFilterModel*>(sourceModel())->setEventTypeFilter(type);
}

void ResourceListModel::setFilterWritable(bool writable)
{
    static_cast<ResourceFilterModel*>(sourceModel())->setFilterWritable(writable);
}

void ResourceListModel::setFilterEnabled(bool enabled)
{
    static_cast<ResourceFilterModel*>(sourceModel())->setFilterEnabled(enabled);
}

void ResourceListModel::setFilterText(const QString& text)
{
    static_cast<ResourceFilterModel*>(sourceModel())->setFilterText(text);
}

bool ResourceListModel::isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const
{
    Q_UNUSED(descendant);
    return !ancestor.isValid();
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant ResourceListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::BackgroundRole:
            if (!mUseResourceColour)
                role = CalendarDataModel::BaseColourRole;
            break;
        default:
            break;
    }
    return KDescendantsProxyModel::data(index, role);
}


/*=============================================================================
= Class: ResourceCheckListModel
= Proxy model providing a checkable list of all Resources. A Resource's
= checked status is equivalent to whether it is selected or not.
= An alarm type is specified, whereby Resources which are enabled for that
= alarm type are checked; Resources which do not contain that alarm type, or
= which are disabled for that alarm type, are unchecked.
=============================================================================*/

ResourceListModel* ResourceCheckListModel::mModel = nullptr;
int                ResourceCheckListModel::mInstanceCount = 0;

ResourceCheckListModel::ResourceCheckListModel(CalEvent::Type type, QObject* parent)
    : KCheckableProxyModel(parent)
    , mAlarmType(type)
{
    ++mInstanceCount;
}

void ResourceCheckListModel::init()
{
    setSourceModel(mModel);    // the source model is NOT filtered by alarm type
    mSelectionModel = new QItemSelectionModel(mModel);
    setSelectionModel(mSelectionModel);
    connect(mSelectionModel, &QItemSelectionModel::selectionChanged,
                       this, &ResourceCheckListModel::selectionChanged);
    connect(mModel, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, &QAbstractItemModel::rowsInserted,
              this, &ResourceCheckListModel::slotRowsInserted);
    // This is probably needed to make ResourceFilterCheckListModel update
    // (similarly to when rows are inserted).
    connect(mModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), SIGNAL(layoutAboutToBeChanged()));
    connect(mModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), SIGNAL(layoutChanged()));

    connect(Resources::instance(), &Resources::settingsChanged,
                             this, &ResourceCheckListModel::resourceSettingsChanged);

    // Initialise checked status for all resources.
    // Note that this is only necessary if the model is recreated after
    // being deleted.
    for (int row = 0, count = mModel->rowCount();  row < count;  ++row)
        setSelectionStatus(mModel->resource(row), mModel->index(row, 0));
}

ResourceCheckListModel::~ResourceCheckListModel()
{
    if (--mInstanceCount <= 0)
    {
        delete mModel;
        mModel = nullptr;
    }
}


/******************************************************************************
* Return the resource for a given row.
*/
Resource ResourceCheckListModel::resource(int row) const
{
    return mModel->resource(mapToSource(index(row, 0)));
}

Resource ResourceCheckListModel::resource(const QModelIndex& index) const
{
    return mModel->resource(mapToSource(index));
}

/******************************************************************************
* Return model data for one index.
*/
QVariant ResourceCheckListModel::data(const QModelIndex& index, int role) const
{
    const Resource resource = mModel->resource(index);
    if (resource.isValid())
    {
        // This is a Resource row
        switch (role)
        {
            case Qt::ForegroundRole:
                if (resource.alarmTypes() & mAlarmType)
                    return resource.foregroundColour(mAlarmType);
                break;
            case Qt::FontRole:
                if (Resources::isStandard(resource, mAlarmType))
                {
                    // It's the standard resource for an alarm type
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
* If the change is to disable a resource, check for eligibility and prevent
* the change if necessary.
*/
bool ResourceCheckListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole  &&  static_cast<Qt::CheckState>(value.toInt()) != Qt::Checked)
    {
        // A resource is to be disabled.
        const Resource resource = mModel->resource(index);
        if (resource.isEnabled(mAlarmType))
        {
            QString errmsg;
            QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
            if (Resources::isStandard(resource, mAlarmType))
            {
                // It's the standard resource for some alarm type.
                if (mAlarmType == CalEvent::ACTIVE)
                {
                    errmsg = i18nc("@info", "You cannot disable your default active alarm calendar.");
                }
                else if (mAlarmType == CalEvent::ARCHIVED  &&  Preferences::archivedKeepDays())
                {
                    // Only allow the archived alarms standard resource to be disabled if
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
void ResourceCheckListModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    Q_EMIT layoutAboutToBeChanged();
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = mapToSource(index(row, 0, parent));
        const Resource resource = mModel->resource(ix);
        if (resource.isValid())
            setSelectionStatus(resource, ix);
    }
    Q_EMIT layoutChanged();   // this is needed to make ResourceFilterCheckListModel update
}

/******************************************************************************
* Called when the user has ticked/unticked a resource to enable/disable it
* (or when the selection changes for any other reason).
*/
void ResourceCheckListModel::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const QModelIndexList sel = selected.indexes();
    for (const QModelIndex& ix : sel)
    {
        // Try to enable the resource, but untick it if not possible
        Resource resource = mModel->resource(ix);
        resource.setEnabled(mAlarmType, true);
        if (!resource.isEnabled(mAlarmType))
            mSelectionModel->select(ix, QItemSelectionModel::Deselect);
    }
    const QModelIndexList desel = deselected.indexes();
    for (const QModelIndex& ix : desel)
        mModel->resource(ix).setEnabled(mAlarmType, false);
}

/******************************************************************************
* Called when a resource parameter or status has changed.
* If the resource's alarm types have been reconfigured, ensure that the
* model views are updated to reflect this.
*/
void ResourceCheckListModel::resourceSettingsChanged(Resource& res, ResourceType::Changes change)
{
    if (!res.isValid()  ||  !(res.alarmTypes() & mAlarmType))
        return;     // resource invalid, or its alarm type is not the one for this model
    if (!(change & (ResourceType::Enabled | ResourceType::AlarmTypes)))
        return;

    if (change & ResourceType::Enabled)
        qCDebug(KALARM_LOG) << debugType("resourceSettingsChanged").constData() << "Enabled" << res.id();
    if (change & ResourceType::AlarmTypes)
        qCDebug(KALARM_LOG) << debugType("resourceSettingsChanged").constData() << "AlarmTypes" << res.id();

    const QModelIndex ix = mModel->resourceIndex(res);
    if (ix.isValid())
        setSelectionStatus(res, ix);
    if (change & ResourceType::AlarmTypes)
        Q_EMIT resourceTypeChange(this);
}

/******************************************************************************
* Select or deselect an index according to its enabled status.
*/
void ResourceCheckListModel::setSelectionStatus(const Resource& resource, const QModelIndex& sourceIndex)
{
    const QItemSelectionModel::SelectionFlags sel = resource.isEnabled(mAlarmType)
                                                  ? QItemSelectionModel::Select : QItemSelectionModel::Deselect;
    mSelectionModel->select(sourceIndex, sel);
}

/******************************************************************************
* Return the instance's alarm type, as a string.
*/
QByteArray ResourceCheckListModel::debugType(const char* func) const
{
    const char* type;
    switch (mAlarmType)
    {
        case CalEvent::ACTIVE:    type = "ResourceCheckListModel[Act]::";  break;
        case CalEvent::ARCHIVED:  type = "ResourceCheckListModel[Arch]::";  break;
        case CalEvent::TEMPLATE:  type = "ResourceCheckListModel[Tmpl]::";  break;
        default:                  type = "ResourceCheckListModel::";  break;
    }
    return QByteArray(type) + func + ':';
}


/*=============================================================================
= Class: ResourceFilterCheckListModel
= Proxy model providing a checkable resource list. The model contains all
= alarm types, but returns only one type at any given time. The selected alarm
= type may be changed as desired.
=============================================================================*/
ResourceFilterCheckListModel::ResourceFilterCheckListModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void ResourceFilterCheckListModel::init()
{
    connect(  mActiveModel, &ResourceCheckListModel::resourceTypeChange,
                      this, &ResourceFilterCheckListModel::resourceTypeChanged);
    connect(mArchivedModel, &ResourceCheckListModel::resourceTypeChange,
                      this, &ResourceFilterCheckListModel::resourceTypeChanged);
    connect(mTemplateModel, &ResourceCheckListModel::resourceTypeChange,
                      this, &ResourceFilterCheckListModel::resourceTypeChanged);
}

void ResourceFilterCheckListModel::setEventTypeFilter(CalEvent::Type type)
{
    if (type != mAlarmType)
    {
        ResourceCheckListModel* newModel;
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
* Return the resource for a given row.
*/
Resource ResourceFilterCheckListModel::resource(int row) const
{
    return static_cast<ResourceCheckListModel*>(sourceModel())->resource(mapToSource(index(row, 0)));
}

Resource ResourceFilterCheckListModel::resource(const QModelIndex& index) const
{
    return static_cast<ResourceCheckListModel*>(sourceModel())->resource(mapToSource(index));
}

QVariant ResourceFilterCheckListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::ToolTipRole:
        {
            const Resource res = resource(index);
            if (res.isValid())
            {
                if (mTooltipFunction)
                    return (*mTooltipFunction)(res, mAlarmType);
            }
            break;
        }
        default:
            break;
    }
    return QSortFilterProxyModel::data(index, role);
}

bool ResourceFilterCheckListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (mAlarmType == CalEvent::EMPTY)
        return true;
    const ResourceCheckListModel* model = static_cast<ResourceCheckListModel*>(sourceModel());
    const Resource resource = model->resource(model->index(sourceRow, 0, sourceParent));
    return resource.alarmTypes() & mAlarmType;
}

/******************************************************************************
* Called when a resource alarm type has changed.
* Ensure that the resource is removed from or added to the current model view.
*/
void ResourceFilterCheckListModel::resourceTypeChanged(ResourceCheckListModel* model)
{
    if (model == sourceModel())
        invalidateFilter();
}


/*=============================================================================
= Class: ResourceView
= View displaying a list of resources.
=============================================================================*/
ResourceView::ResourceView(ResourceFilterCheckListModel* model, QWidget* parent)
    : QListView(parent)
{
    setModel(model);
}

ResourceFilterCheckListModel* ResourceView::resourceModel() const
{
    return static_cast<ResourceFilterCheckListModel*>(model());
}

void ResourceView::setModel(QAbstractItemModel* model)
{
    QListView::setModel(model);
}

/******************************************************************************
* Return the resource for a given row.
*/
Resource ResourceView::resource(int row) const
{
    return static_cast<ResourceFilterCheckListModel*>(model())->resource(row);
}

Resource ResourceView::resource(const QModelIndex& index) const
{
    return static_cast<ResourceFilterCheckListModel*>(model())->resource(index);
}

/******************************************************************************
* Called when a mouse button is released.
* Any currently selected resource is deselected.
*/
void ResourceView::mouseReleaseEvent(QMouseEvent* e)
{
    if (!indexAt(e->pos()).isValid())
        clearSelection();
    QListView::mouseReleaseEvent(e);
}

/******************************************************************************
* Called when a ToolTip or WhatsThis event occurs.
*/
bool ResourceView::viewportEvent(QEvent* e)
{
    if (e->type() == QEvent::ToolTip  &&  isActiveWindow())
    {
        const QHelpEvent* he = static_cast<QHelpEvent*>(e);
        const QModelIndex index = indexAt(he->pos());
        QVariant value = static_cast<ResourceFilterCheckListModel*>(model())->data(index, Qt::ToolTipRole);
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
                    // The whole of the resource name is already displayed,
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

// vim: et sw=4:
