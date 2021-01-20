/*
 *  akonadiresource.h  -  class for an Akonadi alarm calendar resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AKONADIRESOURCE_H
#define AKONADIRESOURCE_H

#include "resource.h"

#include "kalarm_debug.h"

#include <KAlarmCal/CollectionAttribute>

#include <AkonadiCore/AgentInstance>
#include <AkonadiCore/Collection>
#include <AkonadiCore/Item>
#include <AkonadiCore/ServerManager>

#include <QObject>
#include <QDBusConnection>

class KJob;
class DuplicateResourceObject;

using namespace KAlarmCal;

/** Class for an alarm calendar resource accessed through Akonadi.
 *  Public access to this class is normally via the Resource class.
 */
class AkonadiResource : public ResourceType
{
    Q_OBJECT
public:
    /** Construct a new AkonadiResource.
     *  The supplied collection must be up to date.
     */
    static Resource create(const Akonadi::Collection&);

protected:
    /** Constructor.
     *  The supplied collection must be up to date.
     */
    explicit AkonadiResource(const Akonadi::Collection&);

public:
    ~AkonadiResource() override;

    static Resource nullResource();

    /** Return whether the resource has a valid configuration. */
    bool isValid() const override;

    /** Return the resource's collection. */
    Akonadi::Collection collection() const;

    /** Return the type of storage used by the resource. */
    StorageType storageType() const override;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    QString storageTypeString(bool description) const override;

    /** Return the location(s) of the resource (URL, file path, etc.) */
    QUrl location() const override;

    /** Return the location of the resource (URL, file path, etc.)
     *  for display purposes. */
    QString displayLocation() const override;

    /** Return the resource's display name. */
    QString displayName() const override;

    /** Return the resource's configuration identifier, which for this class is
     *  the Akonadi resource identifier. This is not the name normally
     *  displayed to the user.
     */
    QString configName() const override;

    /** Return which types of alarms the resource can contain. */
    CalEvent::Types alarmTypes() const override;

    /** Return which alarm types (active, archived or template) the
     *  resource is enabled for. */
    CalEvent::Types enabledTypes() const override;

    /** Set the enabled/disabled state of the resource and its alarms,
     *  for a specified alarm type (active, archived or template). The
     *  enabled/disabled state for other alarm types is not affected.
     *  The alarms of that type in a disabled resource are ignored, and
     *  not displayed in the alarm list. The standard status for that type
     *  for a disabled resource is automatically cleared.
     *  @param type     alarm type
     *  @param enabled  true to set enabled, false to set disabled.
     */
    void setEnabled(CalEvent::Type type, bool enabled) override;

    /** Set which alarm types (active, archived or template) the resource
     *  is enabled for.
     *  @param types  alarm types
     */
    void setEnabled(CalEvent::Types types) override;

    /** Return whether the resource is configured as read-only or is
     *  read-only on disc.
     */
    bool readOnly() const override;

    /** Return whether the resource is both enabled and fully writable for a
     *  given alarm type, i.e. not read-only, and compatible with the current
     *  KAlarm calendar format.
     *
     *  @param type  alarm type to check for, or EMPTY to check for any type.
     *  @return 1 = fully enabled and writable,
     *          0 = enabled and writable except that backend calendar is in an
     *              old KAlarm format,
     *         -1 = read-only, disabled or incompatible format.
     */
    int writableStatus(CalEvent::Type type = CalEvent::EMPTY) const override;

    using ResourceType::isWritable;

    /** Return whether the user has chosen not to update the resource's
     *  calendar storage format. */
    bool keepFormat() const override;

    /** Set or clear whether the user has chosen not to update the resource's
     *  calendar storage format. */
    void setKeepFormat(bool keep) override;

    /** Return the background colour used to display alarms belonging to
     *  this resource.
     *  @return display colour, or invalid if none specified */
    QColor backgroundColour() const override;

    /** Set the background colour used to display alarms belonging to this
     *  resource.
     *  @param colour display colour, or invalid to use the default colour */
    void setBackgroundColour(const QColor& colour) override;

    /** Return whether the resource is set in its Akonadi attribute to be the
     *  standard resource for a specified alarm type (active, archived or
     *  template). There is no check for whether the resource is enabled, is
     *  writable, or is the only resource set as standard.
     *
     *  @note To determine whether the resource is actually the standard
     *        resource, call Resources::isStandard().
     *
     *  @param type  alarm type
     */
    bool configIsStandard(CalEvent::Type type) const override;

    /** Return which alarm types (active, archived or template) the resource
     *  is standard for, as set in its Akonadi attribute. This is restricted to
     *  the alarm types which the resource can contain (@see alarmTypes()).
     *  There is no check for whether the resource is enabled, is writable, or
     *  is the only resource set as standard.
     *
     *  @note To determine what alarm types the resource is actually the standard
     *        resource for, call Resources::standardTypes().
     *
     *  @return alarm types.
     */
    CalEvent::Types configStandardTypes() const override;

    /** Set or clear the resource as the standard resource for a specified alarm
     *  type (active, archived or template), storing the setting in its Akonadi
     *  attribute. There is no check for whether the resource is eligible to be
     *  set as standard, or to ensure that it is the only standard resource for
     *  the type.
     *
     *  @note To set the resource's standard status and ensure that it is
     *        eligible and the only standard resource for the type, call
     *        Resources::setStandard().
     *
     *  @param type      alarm type
     *  @param standard  true to set as standard, false to clear standard status.
     */
    void configSetStandard(CalEvent::Type type, bool standard) override;

    /** Set which alarm types (active, archived or template) the resource is
     *  the standard resource for, storing the setting in its Akonadi attribute.
     *  There is no check for whether the resource is eligible to be set as
     *  standard, or to ensure that it is the only standard resource for the
     *  types.
     *
     *  @note To set the resource's standard status and ensure that it is
     *        eligible and the only standard resource for the types, call
     *        Resources::setStandard().
     *
     *  @param types  alarm types.to set as standard
     */
    void configSetStandard(CalEvent::Types types) override;

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     *  @param versionString  Receives calendar's KAlarm version as a string.
     */
    KACalendar::Compat compatibilityVersion(QString& versionString) const override;

    /** Edit the resource's configuration. */
    void editResource(QWidget* dialogParent) override;

    /** Remove the resource. The calendar file is not removed.
     *  @note The instance will be invalid once it has been removed.
     *  @return true if the resource has been removed or a removal job has been scheduled.
     */
    bool removeResource() override;

    /** Load the resource from storage, and fetch all events.
     *  Not applicable to AkonadiResource, since the Akonadi resource handles
     *  loading automatically.
     *  @return true.
     */
    bool load(bool readThroughCache = true) override;

    /** Reload the resource. Any cached data is first discarded.
     *  Not applicable to AkonadiResource, since Akonadi provides no means to
     *  reload from the backend.
     *  @return false
     */
    bool reload(bool discardMods = false) override;

    /** Return whether the resource has fully loaded. */
    bool isPopulated() const override;

    /** Save the resource.
     *  Not applicable to AkonadiResource, since AkonadiDataModel handles saving
     *  automatically.
     *  @return true.
     */
    bool save(QString* errorMessage = nullptr, bool writeThroughCache = true, bool force = false) override;

    /** Add an event to the resource, and add it to Akonadi. */
    bool addEvent(const KAEvent&) override;

    /** Update an event in the resource, and update it in Akonadi.
     *  Its UID must be unchanged.
     */
    bool updateEvent(const KAEvent&, bool saveIfReadOnly = true) override;

    /** Delete an event from the resource, and from Akonadi. */
    bool deleteEvent(const KAEvent&) override;

    /** Called to notify the resource that an event's command error has changed. */
    void handleCommandErrorChange(const KAEvent&) override;

    /*-----------------------------------------------------------------------------
    * The methods below are all particular to the AkonadiResource class, and in
    * order to be accessible to clients are defined as 'static'.
    *----------------------------------------------------------------------------*/

    /******************************************************************************
    * Return a reference to the Collection held by an Akonadi resource.
    * @reply Reference to the Collection, which belongs to AkonadiDataModel and
    *        whose ID must not be changed.
    */
    static Akonadi::Collection& collection(Resource&);
    static const Akonadi::Collection& collection(const Resource&);
//    Akonadi::Collection& collection()   { return mCollection; }

    /** Return the event for an Akonadi Item belonging to a resource. */
    static KAEvent event(Resource&, const Akonadi::Item&);
    using QObject::event;   // prevent warning about hidden virtual method

    /** Check for, and remove, Akonadi resources which duplicate use of
     *  calendar files/directories.
     *  @param completed  Function to call on completion of de-duplication, if
     *                    KAlarm Akonadi resources exist.
     *  @return  true if de-duplication initiated;
     *           false if no KAlarm Akonadi resources found, in which case @p
     *           completed will not be called.
     */
    static bool removeDuplicateResources(void (*completed)() = nullptr);

    /** Called to notify that a resource's Collection has been populated.
     *  @param events  The full list of events in the Collection.
     */
    static void notifyCollectionLoaded(ResourceId, const QList<KAEvent>& events);

    /** Called to notify that a resource's Collection has changed. */
    static void notifyCollectionChanged(Resource&, const Akonadi::Collection&, bool checkCompatibility);

    /** Called to notify that events have been added or updated in Akonadi. */
    static void notifyEventsChanged(Resource&, const QList<KAEvent>&);

    /** Called to notify that events are to be deleted from Akonadi. */
    static void notifyEventsToBeDeleted(Resource&, const QList<KAEvent>&);

    /** Called to notify that an Akonadi Item belonging to a resource has
     *  changed or been created.
     *  @note  notifyEventChanged() should also be called to signal the
     *         new or changed event to interested parties.
     *  @param created  true if the item has been created; false if changed.
     */
    static void notifyItemChanged(Resource&, const Akonadi::Item&, bool created);

    /** Create a D-Bus interface to an Akonadi resource.
     *  @return  If success: interface;
     *           If error: null, with 'errorMessage' containing the error message.
     */
    template <class Interface>
    static Interface* getAgentInterface(const Akonadi::AgentInstance& agent, QString& errorMessage, QObject* parent);

    /** The resource type for a single file Akonadi resource. */
    static const QString KALARM_RESOURCE;
    /** The resource type for a directory Akonadi resource. */
    static const QString KALARM_DIR_RESOURCE;

private Q_SLOTS:
    void slotCollectionRemoved(const Akonadi::Collection&);
    void itemJobDone(KJob*);
    void modifyCollectionAttrJobDone(KJob*);

private:
    void queueItemModifyJob(const Akonadi::Item&);
    void checkQueuedItemModifyJob(const Akonadi::Item&);
    void fetchCollectionAttribute(bool refresh) const;
    void modifyCollectionAttribute();

    static DuplicateResourceObject* mDuplicateResourceObject;  // QObject used by removeDuplicateResources()
    mutable Akonadi::Collection mCollection;           // the Akonadi Collection represented by this resource
    mutable CollectionAttribute mCollectionAttribute;  // current set value of CollectionAttribute
    QHash<KJob*, Akonadi::Item::Id> mPendingItemJobs;  // pending item creation/deletion jobs, with event ID
    QHash<Akonadi::Item::Id, Akonadi::Item> mItemModifyJobQueue; // pending item modification jobs, invalid item = queue empty but job active
    QList<Akonadi::Item::Id>    mItemsBeingCreated;    // new items not fully initialised yet
    bool                        mValid;                // whether the collection is valid and belongs to an Akonadi resource
    mutable bool                mHaveCollectionAttribute {false};  // whether the collection has a CollectionAttribute
    bool                        mHaveCompatibilityAttribute {false};  // whether the collection has a CompatibilityAttribute
    CalEvent::Types             mLastEnabled {CalEvent::EMPTY};  // last known enabled status
    mutable bool                mNewEnabled {false};
    bool                        mCollectionAttrChecked {false};  // CollectionAttribute has been processed first time
};


/*=============================================================================
* Template definitions.
*============================================================================*/

/******************************************************************************
* Create a D-Bus interface to an Akonadi resource.
* Reply = interface if success
*       = 0 if error: 'errorMessage' contains the error message.
*/
template <class Interface> Interface* AkonadiResource::getAgentInterface(const Akonadi::AgentInstance& agent, QString& errorMessage, QObject* parent)
{
    const auto service = Akonadi::ServerManager::agentServiceName(Akonadi::ServerManager::Resource, agent.identifier());
    Interface* iface = new Interface(service, QStringLiteral("/Settings"), QDBusConnection::sessionBus(), parent);
    if (!iface->isValid())
    {
        errorMessage = iface->lastError().message();
        qCDebug(KALARM_LOG) << "AkonadiResource::getAgentInterface: D-Bus error accessing resource:" << errorMessage;
        delete iface;
        return nullptr;
    }
    return iface;
}

#endif // AKONADIRESOURCE_H

// vim: et sw=4:
