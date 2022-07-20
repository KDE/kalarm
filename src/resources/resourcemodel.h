/*
 *  resourcemodel.h  -  models containing flat list of resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2010-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "resource.h"
#include "kalarmcalendar/kacalendar.h"

#include <KDescendantsProxyModel>
#include <KCheckableProxyModel>

#include <QSortFilterProxyModel>
#include <QListView>

using namespace KAlarmCal;

/*=============================================================================
= Class: ResourceFilterModel
= Proxy model to filter a resource data model to restrict its contents to
= resources, not events, containing specified alarm types.
= It can optionally be restricted to writable and/or enabled resources.
=============================================================================*/
class ResourceFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    /** Constructs a new instance.
     *  @tparam DataModel  The data model class to use as the source model. It must
     *                     have the following methods:
     *                     static Model* instance(); - returns the unique instance.
     *                     QModelIndex resourceIndex(const Resource&) const;
     */
    template <class DataModel>
    static ResourceFilterModel* create(QObject* parent = nullptr);

    /** Set the alarm type to include in the model.
     *  @param type  If EMPTY, include all alarm types;
     *               otherwise, include only resources with the specified alarm type.
     */
    void setEventTypeFilter(CalEvent::Type);

    /** Filter on resources' writable status.
     *  @param enabled  If true, only include writable resources;
     *                  if false, ignore writable status.
     */
    void setFilterWritable(bool writable);

    /** Filter on resources' enabled status.
     *  @param enabled  If true, only include enabled resources;
     *                  if false, ignore enabled status.
     */
    void setFilterEnabled(bool enabled);

    /** Filter on resources' display names, using a simple text search. */
    void setFilterText(const QString& text);

    QModelIndex resourceIndex(const Resource&) const;

    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    QModelIndexList match(const QModelIndex &start, int role, const QVariant &value, int hits = 1, Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool filterAcceptsColumn(int sourceColumn, const QModelIndex &sourceParent) const override;

private:
    explicit ResourceFilterModel(QObject* parent);

    QModelIndex  (*mResourceIndexFunction)(const Resource&) {nullptr};  // function to fetch resource index from data model
    QString        mFilterText;           // only include resources whose display names include this
    CalEvent::Type mAlarmType {CalEvent::EMPTY};  // only include resources with this alarm type
    bool           mWritableOnly {false}; // only include writable resources
    bool           mEnabledOnly {false};  // only include enabled resources
};

/*=============================================================================
= Class: ResourceListModel
= Proxy model converting the resource tree into a flat list.
= The model may be restricted to specified alarm types.
= It can optionally be restricted to writable and/or enabled resources.
=============================================================================*/
class ResourceListModel : public KDescendantsProxyModel
{
    Q_OBJECT
public:
    /** Constructs a new instance.
     *  @tparam DataModel  The data model class to use as the source model. It must
     *                     have the following methods:
     *                     static DataModel* instance(); - returns the unique instance.
     *                     QModelIndex resourceIndex(const Resource&) const;
     */
    template <class DataModel>
    static ResourceListModel* create(QObject* parent = nullptr);

    void         setEventTypeFilter(CalEvent::Type);
    void         setFilterWritable(bool writable);
    void         setFilterEnabled(bool enabled);
    void         setFilterText(const QString& text);
    void         useResourceColour(bool use)   { mUseResourceColour = use; }
    Resource     resource(int row) const;
    Resource     resource(const QModelIndex&) const;
    QModelIndex  resourceIndex(const Resource&) const;
    virtual bool isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const;
    QVariant     data(const QModelIndex&, int role = Qt::DisplayRole) const override;

private:
    explicit ResourceListModel(QObject* parent);

    bool mUseResourceColour {true};
};


/*=============================================================================
= Class: ResourceCheckListModel
= Proxy model providing a checkable list of all Resources.
= An alarm type is specified, whereby Resources which are enabled for that
= alarm type are checked; Resources which do not contain that alarm type, or
= which are disabled for that alarm type, are unchecked.
=============================================================================*/
class ResourceCheckListModel : public KCheckableProxyModel
{
    Q_OBJECT
public:
    /** Constructs a new instance.
     *  @tparam DataModel  The data model class to use as the source model. It must
     *                     have the following methods:
     *                     static DataModel* instance(); - returns the unique instance.
     *                     QModelIndex resourceIndex(const Resource&) const;
     */
    template <class DataModel>
    static ResourceCheckListModel* create(CalEvent::Type, QObject* parent = nullptr);

    ~ResourceCheckListModel() override;
    Resource resource(int row) const;
    Resource resource(const QModelIndex&) const;
    void disable()    { mDisabled = true; }
    QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex&, const QVariant& value, int role) override;

Q_SIGNALS:
    void resourceTypeChange(ResourceCheckListModel*);

private Q_SLOTS:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void slotRowsInsertedRemoved();
    void resourceSettingsChanged(Resource&, ResourceType::Changes);

private:
    ResourceCheckListModel(CalEvent::Type, QObject* parent);
    void init();
    void setSelectionStatus(const Resource&, const QModelIndex&);
    QByteArray debugType(const char* func) const;

    static ResourceListModel* mModel;
    static int                mInstanceCount;
    CalEvent::Type            mAlarmType;     // alarm type contained in this model
    QItemSelectionModel*      mSelectionModel;
    bool                      mResetting {false};   // currently handling rows inserted/removed
    bool                      mDisabled {false};    // resources are being deleted on program exit
};


/*=============================================================================
= Class: ResourceFilterCheckListModel
= Proxy model providing a checkable resource list, filtered to contain only one
= alarm type. The selected alarm type may be changed as desired.
=============================================================================*/
class ResourceFilterCheckListModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    /** Constructs an instance.
     *  @tparam DataModel  The data model class to use as the source model. It must
     *                     have the following methods:
     *                     static DataModel* instance(); - returns the unique instance.
     *                     QModelIndex resourceIndex(const Resource&) const;
     *                     QString tooltip(const Resource&, CalEvent::Types) const;
     */
    template <class DataModel>
    static ResourceFilterCheckListModel* create(QObject* parent = nullptr);

    ~ResourceFilterCheckListModel() override;
    void setEventTypeFilter(CalEvent::Type);
    Resource resource(int row) const;
    Resource resource(const QModelIndex&) const;
    static void disable();
    QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private Q_SLOTS:
    void resourceTypeChanged(ResourceCheckListModel*);
    void slotRowsAboutToBeInserted(const QModelIndex& parent, int start, int end);
    void slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
    void slotRowsInserted();
    void slotRowsRemoved();

private:
    explicit ResourceFilterCheckListModel(QObject* parent);
    void init();

    static QVector<ResourceFilterCheckListModel*> mInstances;
    ResourceCheckListModel* mActiveModel {nullptr};
    ResourceCheckListModel* mArchivedModel {nullptr};
    ResourceCheckListModel* mTemplateModel {nullptr};
    CalEvent::Type          mAlarmType {CalEvent::EMPTY};  // alarm type contained in this model
    QString               (*mTooltipFunction)(const Resource&, CalEvent::Types) {nullptr};  // function to fetch tooltip from data model
};


/*=============================================================================
= Class: ResourceView
= View for a ResourceFilterCheckListModel.
=============================================================================*/
class ResourceView : public QListView
{
    Q_OBJECT
public:
    explicit ResourceView(ResourceFilterCheckListModel*, QWidget* parent = nullptr);
    ResourceFilterCheckListModel* resourceModel() const;
    Resource  resource(int row) const;
    Resource  resource(const QModelIndex&) const;
    QSize     sizeHint() const override;

Q_SIGNALS:
    void rowCountChanged();

protected:
    void mouseReleaseEvent(QMouseEvent*) override;
    bool viewportEvent(QEvent*) override;
    QStyleOptionViewItem listViewOptions() const;
};


/*=============================================================================
* Template definitions.
*============================================================================*/

template <class DataModel>
ResourceFilterModel* ResourceFilterModel::create(QObject* parent)
{
    auto model = new ResourceFilterModel(parent);
    model->setSourceModel(DataModel::instance());
    model->mResourceIndexFunction = [](const Resource& r) { return DataModel::instance()->resourceIndex(r); };
    return model;
}

template <class DataModel>
ResourceListModel* ResourceListModel::create(QObject* parent)
{
    auto model = new ResourceListModel(parent);
    model->setSourceModel(ResourceFilterModel::create<DataModel>(model));
    return model;
}

template <class DataModel>
ResourceCheckListModel* ResourceCheckListModel::create(CalEvent::Type type, QObject* parent)
{
    auto model = new ResourceCheckListModel(type, parent);
    if (!mModel)
        mModel = ResourceListModel::create<DataModel>(nullptr);
    model->init();
    return model;
}

template <class DataModel>
ResourceFilterCheckListModel* ResourceFilterCheckListModel::create(QObject* parent)
{
    ResourceFilterCheckListModel* instance = new ResourceFilterCheckListModel(parent);
    mInstances.append(instance);
    instance->mActiveModel   = ResourceCheckListModel::create<DataModel>(CalEvent::ACTIVE, instance);
    instance->mArchivedModel = ResourceCheckListModel::create<DataModel>(CalEvent::ARCHIVED, instance);
    instance->mTemplateModel = ResourceCheckListModel::create<DataModel>(CalEvent::TEMPLATE, instance);
    instance->mTooltipFunction = [](const Resource& r, CalEvent::Types t) { return DataModel::instance()->tooltip(r, t); };
    instance->init();
    return instance;
}

// vim: et sw=4:
