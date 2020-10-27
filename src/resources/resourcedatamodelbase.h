/*
 *  resourcedatamodelbase.h  -  base for models containing calendars and events
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RESOURCEDATAMODELBASE_H
#define RESOURCEDATAMODELBASE_H

#include "resourcetype.h"

#include "preferences.h"

#include <KAlarmCal/KACalendar>

#include <QSize>

class Resource;
class ResourceListModel;
class ResourceFilterCheckListModel;
class AlarmListModel;
class TemplateListModel;
class ResourceCreator;
class QPixmap;

namespace KAlarmCal { class KAEvent; }

using namespace KAlarmCal;


/*=============================================================================
= Class: ResourceDataModelBase
= Base class for models containing all calendars and events.
=============================================================================*/
class ResourceDataModelBase
{
public:
    /** Data column numbers. */
    enum
    {
        // Item columns
        TimeColumn = 0, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, NameColumn, TextColumn,
        TemplateNameColumn,
        ColumnCount
    };
    /** Additional model data roles. */
    enum
    {
        UserRole = Qt::UserRole + 500,   // copied from Akonadi::EntityTreeModel
        ItemTypeRole = UserRole,   // item's type: calendar or event
        // Calendar roles
        ResourceIdRole,            // the resource ID
        BaseColourRole,            // background colour ignoring collection colour
        // Event roles
        EventIdRole,               // the event ID
        ParentResourceIdRole,      // the parent resource ID of the event
        EnabledRole,               // true for enabled alarm, false for disabled
        StatusRole,                // KAEvent::ACTIVE/ARCHIVED/TEMPLATE
        AlarmActionsRole,          // KAEvent::Actions
        AlarmSubActionRole,        // KAEvent::Action
        ValueRole,                 // numeric value
        SortRole,                  // the value to use for sorting
        TimeDisplayRole,           // time column value with '~' representing omitted leading zeroes
        ColumnTitleRole,           // column titles (whether displayed or not)
        CommandErrorRole           // last command execution error for alarm (per user)
    };
    /** The type of a model row. */
    enum class Type { Error = 0, Event, Resource };

    virtual ~ResourceDataModelBase();

    static QSize iconSize()       { return mIconSize; }

    /** Return a bulleted list of alarm types for inclusion in an i18n message. */
    static QString typeListForDisplay(CalEvent::Types);

    /** Get the tooltip for a resource. The resource's enabled status is
     *  evaluated for specified alarm types. */
    QString tooltip(const Resource&, CalEvent::Types) const;

    /** Return the read-only status tooltip for a resource.
     * A null string is returned if the resource is fully writable. */
    static QString readOnlyTooltip(const Resource&);

    /** Return offset to add to headerData() role, for item models. */
    virtual int headerDataEventRoleOffset() const  { return 0; }

protected:
    ResourceDataModelBase();

    /** Terminate access to the data model, and tidy up. */
    virtual void terminate() = 0;

    /** Reload all resources' data from storage.
     *  @note In the case of Akonadi, this does not reload from the backend storage.
     */
    virtual void reload() = 0;

    /** Reload a resource's data from storage.
     *  @note In the case of Akonadi, this does not reload from the backend storage.
     */
    virtual bool reload(Resource&) = 0;

    /** Check for, and remove, any duplicate resources, i.e. those which use
     *  the same calendar file/directory.
     */
    virtual void removeDuplicateResources() = 0;

    /** Disable the widget if the database engine is not available, and display
     *  an error overlay.
     */
    virtual void widgetNeedsDatabase(QWidget*) = 0;

    /** Create a ResourceCreator instance for the model. */
    virtual ResourceCreator* createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent) = 0;

    /** Update a resource's backend calendar file to the current KAlarm format. */
    virtual void updateCalendarToCurrentFormat(Resource&, bool ignoreKeepFormat, QObject* parent) = 0;

    virtual ResourceListModel* createResourceListModel(QObject* parent) = 0;
    virtual ResourceFilterCheckListModel* createResourceFilterCheckListModel(QObject* parent) = 0;
    virtual AlarmListModel*    createAlarmListModel(QObject* parent) = 0;
    virtual AlarmListModel*    allAlarmListModel() = 0;
    virtual TemplateListModel* createTemplateListModel(QObject* parent) = 0;
    virtual TemplateListModel* allTemplateListModel() = 0;

    /** Return the data storage backend type used by this model. */
    virtual Preferences::Backend dataStorageBackend() const = 0;

    static QVariant headerData(int section, Qt::Orientation, int role, bool eventHeaders, bool& handled);

    /** Return whether resourceData() and/or eventData() handle a role. */
    bool roleHandled(int role) const;

    /** Return the model data for a resource.
     *  @param role     may be updated for calling the base model.
     *  @param handled  updated to true if the reply is valid, else set to false.
     */
    QVariant resourceData(int& role, const Resource&, bool& handled) const;

    /** Return the model data for an event.
     *  @param handled  updated to true if the reply is valid, else set to false.
     */
    QVariant eventData(int role, int column, const KAEvent& event, const Resource&, bool& handled) const;

    /** Called when a resource notifies a message to display to the user. */
    void handleResourceMessage(ResourceType::MessageType, const QString& message, const QString& details);

    /** Return whether calendar migration/creation at initialisation has completed. */
    bool isMigrationComplete() const;

    /** Return whether calendar migration is currently in progress. */
    bool isMigrating() const;

    /** To be called when calendar migration has been initiated (or reset). */
    void setMigrationInitiated(bool started = true);

    /** To be called when calendar migration has been initiated (or reset). */
    void setMigrationComplete();

    /** To be called when all previously configured calendars have been created. */
    void setCalendarsCreated();

    static QString  repeatText(const KAEvent&);
    static QString  repeatOrder(const KAEvent&);
    static QString  whatsThisText(int column);
    static QPixmap* eventIcon(const KAEvent&);

    static ResourceDataModelBase* mInstance;

private:
    static QPixmap* mTextIcon;
    static QPixmap* mFileIcon;
    static QPixmap* mCommandIcon;
    static QPixmap* mEmailIcon;
    static QPixmap* mAudioIcon;
    static QSize    mIconSize;      // maximum size of any icon

    int  mMigrationStatus {-1};     // migration status, -1 = no, 0 = initiated, 1 = complete
    bool mCreationStatus {false};   // previously configured calendar creation status

friend class DataModel;
};

#endif // RESOURCEDATAMODELBASE_H

// vim: et sw=4:
