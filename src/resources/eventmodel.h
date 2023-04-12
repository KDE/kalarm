/*
 *  eventmodel.h  -  model containing flat list of events
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2010-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "resource.h"
#include "kalarmcalendar/kacalendar.h"

#include <KDescendantsProxyModel>

#include <QSortFilterProxyModel>

using namespace KAlarmCal;

/*=============================================================================
= Class: EventListModel
= Proxy model to filter a resource data model to restrict its contents to
= events, not resources, containing specified alarm types in enabled resources.
=============================================================================*/
class EventListModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    /** Constructs a new instance.
     *  @param  types      The alarm types (active/archived/template) included in this model
     *  @param  parent     The parent object
     *  @tparam DataModel  The data model class to use as the source model. It must
     *                     have the following methods:
     *                       static Model* instance(); - returns the unique instance.
     *                       KAEvent     event(const QModelIndex&) const;
     *                       QModelIndex eventIndex(const QString&) const;
     *                       int         headerDataEventRoleOffset() const;
     */
    template <class DataModel>
    static EventListModel* create(CalEvent::Types types, QObject* parent = nullptr);

    /** Return the alarm types included in the model. */
    CalEvent::Types alarmTypes() const   { return mAlarmTypes; }

    KAEvent      event(int row) const;
    KAEvent      event(const QModelIndex&) const;
    using QObject::event;   // prevent warning about hidden virtual method
    QModelIndex  eventIndex(const QString& eventId) const;

    /** Determine whether the model contains any items. */
    bool haveEvents() const;

    static int iconWidth();

    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits = 1, Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

Q_SIGNALS:
    /** Signal emitted when either the first item is added to the model,
     *  or when the last item is deleted from the model.
     */
    void haveEventsStatus(bool have);

protected:
    /** Constructor. Note that initialise() must be called to complete the construction. */
    EventListModel(CalEvent::Types types, QObject* parent);

    /** To be called after construction as a base class.
     *  @tparam DataModel  The data model class to use as the source model. It must
     *                     have the following methods:
     *                       static Model* instance(); - returns the unique instance.
     *                       KAEvent     event(const QModelIndex&) const;
     *                       QModelIndex eventIndex(const QString&) const;
     *                       int         headerDataEventRoleOffset() const;
     */
    template <class DataModel> void initialise();

    /** Return the event for a given source model row. */
    KAEvent eventForSourceRow(int sourceRow) const;

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const override;

private Q_SLOTS:
    void slotResourcePopulated(Resource&);
    void slotRowsInserted(Resource&);
    void slotRowsRemoved(Resource&);
    void resourceSettingsChanged(Resource&, ResourceType::Changes);

private:
    KAEvent      (*mEventFunction)(const QModelIndex&) {nullptr};  // function to fetch event from data model
    QModelIndex  (*mEventIndexFunction)(const QString&) {nullptr};  // function to fetch event index from data model
    CalEvent::Types mAlarmTypes {CalEvent::EMPTY};  // only include events with these alarm types
    int             mHeaderDataRoleOffset {0};  // offset for base class to add to headerData() role
    bool            mHaveEvents {false};        // there are events in this model
};


/*=============================================================================
= Class: AlarmListModel
= Filter proxy model containing all alarms of specified alarm types in enabled
= resources.
=============================================================================*/
class AlarmListModel : public EventListModel
{
    Q_OBJECT
public:
    enum   // data columns
    {
        TimeColumn = 0, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, NameColumn, TextColumn,
        ColumnCount
    };

    template <class DataModel>
    static AlarmListModel* create(QObject* parent = nullptr);

    ~AlarmListModel() override;

    /** Return the model containing all active and archived alarms. */
    template <class DataModel>
    static AlarmListModel* all();

    /** Set a filter to restrict the event types to a subset of those
     *  specified in the constructor.
     *  @param types the event types to be included in the model
     */
    void setEventTypeFilter(CalEvent::Types types);

    /** Return the filter set by setEventTypeFilter().
     *  @return all event types included in the model
     */
    CalEvent::Types eventTypeFilter() const   { return mFilterTypes; }

    /** Set a filter to include only alarms which are due on specified dates.
     *  @param dates  Dates for inclusion, in date order, or empty to remove filter.
     */
    void setDateFilter(const QList<QDate>& dates);

    /** Set whether to replace a blank alarm name with the alarm text. */
    void setReplaceBlankName(bool replace)   { mReplaceBlankName = replace; }

    int  columnCount(const QModelIndex& = QModelIndex()) const  override { return ColumnCount; }
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

protected:
    explicit AlarmListModel(QObject* parent);

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const override;
    QVariant data(const QModelIndex&, int role) const override;

private Q_SLOTS:
    void slotResourceSettingsChanged(Resource&, ResourceType::Changes);
    void slotResourceRemoved(KAlarmCal::ResourceId);
    void slotEventUpdated(Resource&, const KAlarmCal::KAEvent&);
    void slotEventsRemoved(Resource&, const QList<KAlarmCal::KAEvent>&);

private:
    static AlarmListModel* mAllInstance;
    CalEvent::Types mFilterTypes;    // types of events contained in this model
    QList<std::pair<KADateTime, KADateTime>> mFilterDates; // date/time ranges to include in filter
    mutable QHash<ResourceId, QHash<QString, KADateTime>> mDateFilterCache;  // if date filter, whether events are included in filter
    bool mReplaceBlankName {false};  // replace Name with Text for Qt::DisplayRole if Name is blank
};


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates, optionally for specified
= alarm action types (display, email, etc.) in enabled resources.
=============================================================================*/
class TemplateListModel : public EventListModel
{
    Q_OBJECT
public:
    enum {   // data columns
        TypeColumn, TemplateNameColumn,
        ColumnCount
    };

    template <class DataModel>
    static TemplateListModel* create(QObject* parent = nullptr);

    ~TemplateListModel() override;

    /** Return the model containing all alarm templates. */
    template <class DataModel>
    static TemplateListModel* all();

    /** Set which alarm action types should be included in the model. */
    void setAlarmActionFilter(KAEvent::Actions);

    /** Return which alarm action types are included in the model. */
    KAEvent::Actions alarmActionFilter() const  { return mActionsFilter; }

    /** Set which alarm types should be shown as disabled in the model. */
    void setAlarmActionsEnabled(KAEvent::Actions);

    /** Set which alarm types should be shown as disabled in the model. */
    KAEvent::Actions setAlarmActionsEnabled() const  { return mActionsEnabled; }

    int  columnCount(const QModelIndex& = QModelIndex()) const override  { return ColumnCount; }
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex&) const override;

protected:
    explicit TemplateListModel(QObject* parent);

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const override;

private:
    static TemplateListModel* mAllInstance;

    KAEvent::Actions mActionsEnabled;  // disable types not in this mask
    KAEvent::Actions mActionsFilter;   // hide types not in this mask
};


/*=============================================================================
* Template definitions.
*============================================================================*/

template <class DataModel>
EventListModel* EventListModel::create(CalEvent::Types types, QObject* parent)
{
    auto model = new EventListModel(types, parent);
    model->initialise<DataModel>();
    return model;
}

template <class DataModel> void EventListModel::initialise()
{
    static_cast<KDescendantsProxyModel*>(sourceModel())->setSourceModel(DataModel::instance());
    mHeaderDataRoleOffset = DataModel::instance()->headerDataEventRoleOffset();
    mEventFunction      = [](const QModelIndex& ix) { return DataModel::instance()->event(ix); };
    mEventIndexFunction = [](const QString& id) { return DataModel::instance()->eventIndex(id); };
}

template <class DataModel>
AlarmListModel* AlarmListModel::create(QObject* parent)
{
    auto model = new AlarmListModel(parent);
    model->EventListModel::initialise<DataModel>();
    return model;
}

template <class DataModel>
AlarmListModel* AlarmListModel::all()
{
    if (!mAllInstance)
    {
        mAllInstance = create<DataModel>(DataModel::instance());
        mAllInstance->sort(TimeColumn, Qt::AscendingOrder);
    }
    return mAllInstance;
}

template <class DataModel>
TemplateListModel* TemplateListModel::create(QObject* parent)
{
    auto model = new TemplateListModel(parent);
    model->EventListModel::initialise<DataModel>();
    return model;
}

template <class DataModel>
TemplateListModel* TemplateListModel::all()
{
    if (!mAllInstance)
    {
        mAllInstance = create<DataModel>(DataModel::instance());
        mAllInstance->sort(TemplateNameColumn, Qt::AscendingOrder);
    }
    return mAllInstance;
}

// vim: et sw=4:
