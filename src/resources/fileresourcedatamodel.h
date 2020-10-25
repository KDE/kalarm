/*
 *  fileresourcedatamodel.h  -  model containing file system resources and their events
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FILERESOURCEDATAMODEL_H
#define FILERESOURCEDATAMODEL_H

#include "resourcedatamodelbase.h"
#include "fileresourceconfigmanager.h"

#include <KAlarmCal/KAEvent>

#include <QAbstractItemModel>

using namespace KAlarmCal;


/*=============================================================================
= Class: FileResourceDataModel
= Contains all calendar resources accessed through the file system, and their
= events.
=============================================================================*/
class FileResourceDataModel : public QAbstractItemModel, public ResourceDataModelBase
{
    Q_OBJECT
public:
    static FileResourceDataModel* instance(QObject* parent = nullptr);
    ~FileResourceDataModel() override;

    /** Return whether a model row holds a resource or an event.
     *  @return  type of item held at the index, or Error if index invalid.
     */
    Type           type(const QModelIndex& ix) const;

    Resource       resource(ResourceId) const;
    Resource       resource(const QModelIndex&) const;
    QModelIndex    resourceIndex(const Resource&) const;
    QModelIndex    resourceIndex(ResourceId) const;

    KAEvent event(const QString& eventId) const;
    KAEvent event(const QModelIndex&) const;
    using QObject::event;   // prevent warning about hidden virtual method

    QModelIndex    eventIndex(const KAEvent&) const;
    QModelIndex    eventIndex(const QString& eventId) const;

    bool           addEvent(KAEvent&, Resource&);

    bool           haveEvents() const                   { return mHaveEvents; }

    int            rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int            columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex    index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex    parent(const QModelIndex& index) const override;
    QModelIndexList match(const QModelIndex &start, int role, const QVariant &value, int hits = 1, Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const override;
    QVariant       data(const QModelIndex&, int role = Qt::DisplayRole) const override;
    bool           setData(const QModelIndex&, const QVariant& value, int role = Qt::EditRole) override;
    QVariant       headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags  flags(const QModelIndex&) const override;

Q_SIGNALS:
    void haveEventsStatus(bool have);

protected:
    /** Terminate access to the data model, and tidy up. */
    void terminate() override;

    /** Reload all resources' data from storage. */
    void reload() override;

    /** Reload a resource's data from storage. */
    bool reload(Resource&) override;

    /** Check for, and remove, any duplicate resources, i.e. those which use
     *  the same calendar file/directory. This does nothing for file system
     *  resources, since FileResourceConfigManager::createResources() removes
     *  duplicate resources before creating them.
     */
    void removeDuplicateResources() override  {}

    /** Disable the widget if the database engine is not available, and display an
     *  error overlay. This is not applicable to File Resources.
     */
    void widgetNeedsDatabase(QWidget*) override  {}

    /** Create a ResourceCreator instance for the model. */
    ResourceCreator* createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent) override;

    /** Update a resource's backend calendar file to the current KAlarm format. */
    void updateCalendarToCurrentFormat(Resource&, bool ignoreKeepFormat, QObject* parent) override;

    ResourceListModel* createResourceListModel(QObject* parent) override;
    ResourceFilterCheckListModel* createResourceFilterCheckListModel(QObject* parent) override;
    AlarmListModel*    createAlarmListModel(QObject* parent) override;
    AlarmListModel*    allAlarmListModel() override;
    TemplateListModel* createTemplateListModel(QObject* parent) override;
    TemplateListModel* allTemplateListModel() override;

    /** Return the data storage backend type used by this model. */
    Preferences::Backend dataStorageBackend() const override   { return Preferences::FileResources; }

private Q_SLOTS:
    void     slotMigrationCompleted();
    void     slotUpdateTimeTo();
    void     slotUpdateArchivedColour(const QColor&);
    void     slotUpdateDisabledColour(const QColor&);
    void     slotUpdateHolidays();
    void     slotUpdateWorkingHours();
    /** Add a resource and all its events. */
    void     addResource(Resource&);
    void     slotResourceLoaded(Resource&);
    void     slotResourceSettingsChanged(Resource&, ResourceType::Changes);
    void     removeResource(Resource&);
    void     slotEventsAdded(Resource&, const QList<KAEvent>&);
    void     slotEventUpdated(Resource&, const KAEvent&);
    bool     deleteEvents(Resource&, const QList<KAEvent>&);

    /** Called when a resource notifies a message to display to the user. */
    void slotResourceMessage(ResourceType::MessageType, const QString& message, const QString& details);

private:
    // Represents a resource or event within the data model.
    struct Node;

    explicit FileResourceDataModel(QObject* parent = nullptr);
    void     initialise();
    void     signalDataChanged(bool (*checkFunc)(const KAEvent*), int startColumn, int endColumn, const QModelIndex& parent);

    /** Remove a resource's events. */
    void removeResourceEvents(Resource&, bool setHaveEvents = true);

    int  removeResourceEvents(QVector<Node*>& eventNodes);

    void updateHaveEvents(bool have)        { mHaveEvents = have;  Q_EMIT haveEventsStatus(have); }

    static bool mInstanceIsOurs;        // mInstance is a FileResourceDataModel instance
    // Resource nodes for model root [Resource = Resource()], and
    // Event nodes for each resource.
    QHash<Resource, QVector<Node*>> mResourceNodes;
    // Resources in the order in which they are held in the model.
    // This must be the same order as in mResourceNodes[nullptr].
    QVector<Resource>     mResources;
    QHash<QString, Node*> mEventNodes;  // each event ID, mapped to its node.
    bool                  mHaveEvents;  // there are events in this model
};

#endif // FILERESOURCEDATAMODEL_H

// vim: et sw=4:
