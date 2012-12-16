/*
 *  alarmresources.h  -  alarm calendar resources
 *  Program:  kalarm
 *  Copyright © 2006-2011 by David Jarvie <djarvie@kde.org>
 *  Based on calendarresources.h in libkcal,
 *  Copyright (c) 2003 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
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

#ifndef ALARMRESOURCES_H
#define ALARMRESOURCES_H

#include "alarmresource.h"
#include "kalarm_resources_export.h"

#include <kalarmcal/kacalendar.h>

#include <kresources/manager.h>
#include <kdatetime.h>

class KConfigGroup;
namespace KCal { class Event; }
using KCal::CalendarLocal;
using KCal::ResourceCalendar;
using namespace KAlarmCal;


/** Provides access to all alarm calendar resources. */
class KALARM_RESOURCES_EXPORT AlarmResources : public KCal::Calendar, public KRES::ManagerObserver<AlarmResource>
{
        Q_OBJECT
    public:
        enum Change { Added, Deleted, Invalidated, Enabled, ReadOnly, WrongType, Location, Colour };
        // Return code when the user can cancel an operation
        enum Result { Success, Cancelled, Failed };

        class Ticket
        {
            friend class AlarmResources;
          public:
            AlarmResource* resource() const  { return mResource; }
          private:
            explicit Ticket(AlarmResource* r) : mResource(r) { }
            AlarmResource* mResource;
        };

        /** Create the alarm calendar resources instance.
         *  If the alarm calendar resources instance has already been created,
         *  the existing instance is returned regardless of parameters passed.
         *  @return The alarm calendar resources instance, or 0 if a reserved
         *          file name was used (in which case, use creationError() to
         *          fetch the error message).
         */
        static AlarmResources* create(const KDateTime::Spec& timeSpec, bool activeOnly = false, bool passiveClient = false);
        static QString creationError()   { return mConstructionError; }
        virtual ~AlarmResources();
        /** Return the alarm calendar resources instance.
         *  @return The alarm calendar resources instance, or 0 if not already created.
         */
        static AlarmResources* instance()   { return mInstance; }
        /** Set a reserved local calendar file path which can't be used by this class. */
        static void setReservedFile(const QString& path)     { mReservedFile = path; }

        /** Set whether the application has a GUI. This determines whether error or
         *  progress messages are displayed. */
        void setNoGui(bool);

        /** Specify that the client is passive, i.e. never makes changes to the
         *  resource configuration. This will prevent any configuration changes
         *  from being saved. */
        void   setPassiveClient(bool p)  { mPassiveClient = p; }
        bool   passiveClient()           { return mPassiveClient; }

        /** Return the standard resource for the given alarm type.
         *  @return 0 if no standard resource is set.
         */
        AlarmResource* getStandardResource(CalEvent::Type);
        /** Set the specified resource to be the standard resource for its alarm type,
         *  replacing any existing standard resource.
         */
        void setStandardResource(AlarmResource*);

        /** Add the standard KAlarm default resource for the given alarm type. */
        AlarmResource* addDefaultResource(CalEvent::Type);

        /** Return the number of active resources for a given alarm type. */
        int activeCount(CalEvent::Type, bool writable);

        void writeConfig();
        /** Set a function to write the application ID into a calendar. */
        void setCalIDFunction(void (*f)(CalendarLocal&))
                                      { AlarmResource::setCalIDFunction(f); }
        /** Set a function to create KAlarm event instances.
         *  When the function is called, the CalendarLocal parameter is
         *  set to null to indicate that the resource is about to be reloaded. */
        void setCustomEventFunction(void (*f)(AlarmResource*, CalendarLocal*))
                                      { AlarmResource::setCustomEventFunction(f); }
        /** Set a function to fix the calendar once it has been loaded. */
        void setFixFunction(KACalendar::Compat (*f)(CalendarLocal&, const QString&, AlarmResource*, AlarmResource::FixFunc, bool* wrongType))
                                      { AlarmResource::setFixFunction(f); }

        /** Add an event to the resource calendar.
         *  The resource calendar takes ownership of the event.
         *  @return Success if success; otherwise the event is deleted, and
         *          Cancelled or Failed is returned.
         */
        Result addEvent(KCal::Event*, CalEvent::Type, QWidget* promptParent = 0, bool noPrompt = false);

        /** Return whether all, some or none of the active resources are loaded.
         *  @return 0 if no resources are loaded,
         *          1 if some but not all active resources are loaded,
         *          2 if all active resources are loaded.
         */
        int loadedState(CalEvent::Type) const;
        bool isLoading(CalEvent::Type) const;
        void showProgress(bool);

        /** Loads all incidences from the resources.  The resources must be added
         *  first using either readConfig(KConfig*), which adds the system
         *  resources, or manually using resourceAdded(AlarmResource*).
        */
        void load(KCal::ResourceCached::CacheAction = KCal::ResourceCached::DefaultCache);
        bool load(AlarmResource*, KCal::ResourceCached::CacheAction = KCal::ResourceCached::DefaultCache);

    /**
     * Reloads all incidences from all resources.
     * @return success or failure
     */
    virtual bool reload();

    /**
       Clear out the current Calendar, freeing all used memory etc.
    */
    virtual void close();

    /**
       Sync changes in memory to persistant storage.
    */
    virtual bool save();

    /**
       Determine if the Calendar is currently being saved.

       @return true if the Calendar is currently being saved; false otherwise.
    */
    virtual bool isSaving();

    /**
       Get the CalendarResourceManager used by this calendar.

       @return a pointer to the CalendarResourceManage.
    */
    AlarmResourceManager* resourceManager() const    { return mManager; }

    /**
       Get the resource with the specified resource ID.

       @param resourceID the ID of the resource
       @return pointer to the resource, or null if not found
    */
    AlarmResource* resourceWithId(const QString& resourceID) const;

    /**
       Get the resource associated with a specified incidence.

       @param incidence is a pointer to an incidence whose resource
       is to be located.
       @return a pointer to the resource containing the incidence.
    */
    AlarmResource* resource(const KCal::Incidence*) const;

    /**
       Get the resource associated with a specified incidence ID.

       @param incidenceID the ID of the incidence whose resource is to be located.
       @return a pointer to the resource containing the incidence.
    */
    AlarmResource* resourceForIncidence(const QString& incidenceID);

    /**
       Read the Resources settings from a config file.

       @param config The KConfig object which points to the config file.
       If no object is given (null pointer) the standard config file is used.

       @note Call this method <em>before</em> load().
    */
    void readConfig(KConfig* config = 0);

    /**
       Set the destination policy such that Incidences are added to a
       Resource which is queried.
       @param ask if true, prompt for which resource to add to, if
       false, add to standard resource.
    */
    void setAskDestinationPolicy(bool ask)  { mAskDestination = ask; }

        /** Find the resource to be used to store an event of a given type.
         *  @param cancelled If non-null: set to true if the user cancelled
         *             the prompt dialogue; set to false if any other error.
         */
        AlarmResource* destination(CalEvent::Type, QWidget* promptParent = 0, bool noPrompt = false, bool* cancelled = 0);

        /**
           Called when a resource is added to the managed collection.
           Overrides KRES::ManagerObserver<AlarmResource>::resourceAdded().
           This method must be public, because in-process added resources
           do not emit the corresponding signal, so this methodd has to be
           called manually!

           @param resource is a pointer to the AlarmResource to add.
        */
        virtual void resourceAdded(AlarmResource* resource);

// Incidence Specific Methods //

        // Event Specific Methods //

        /**
           Insert an Event into the Calendar.

           @param event is a pointer to the Event to insert.
           @return true if the Event was successfully inserted; false otherwise.
        */
        virtual bool addEvent(KCal::Event* event)   { return addEvent(event, (QWidget*)0); }
        /** @return Success if success; otherwise the event is deleted, and
         *          Cancelled or Failed is returned.
         */
        Result addEvent(KCal::Event* event, QWidget* promptParent);

        /**
           Insert an Event into a Calendar Resource.

           @param event is a pointer to the Event to insert.
           @param resource is a pointer to the AlarmResource to be added to.
           @return true if the Event was successfully inserted; false otherwise.
        */
        bool addEvent(KCal::Event* event, AlarmResource* resource);

        /**
           Remove an Event from the Calendar.

           @param event is a pointer to the Event to remove.
           @return true if the Event was successfully removed; false otherwise.

           @note In most cases use deleteIncidence(Incidence*) instead.
        */
        virtual bool deleteEvent(KCal::Event* event);

        /**
           Removes all Events from the calendar.
        */
        virtual void deleteAllEvents() {}

        /**
           Return a sorted, unfiltered list of all Events.

           @param sortField specifies the EventSortField.
           @param sortDirection specifies the SortDirection.
           @return the list of all unfiltered Events sorted as specified.
        */
        virtual KCal::Event::List rawEvents(
          KCal::EventSortField sortField = KCal::EventSortUnsorted,
          KCal::SortDirection sortDirection = KCal::SortDirectionAscending);

        /**
           Return a sorted, unfiltered list of all Events for one resource.
           Call this in preference to AlarmResource::rawEvents(), to ensure
           that the overall events index is updated.

           @param resource the AlarmResource to retrieve events for
           @param sortField specifies the EventSortField.
           @param sortDirection specifies the SortDirection.
           @return the list of all unfiltered Events sorted as specified.
        */
        virtual KCal::Event::List rawEvents(AlarmResource* resource,
          KCal::EventSortField sortField = KCal::EventSortUnsorted,
          KCal::SortDirection sortDirection = KCal::SortDirectionAscending);

        /**
           Return an unfiltered list of all Events which occur on the given
           timestamp.

           @param dt request unfiltered Event list for this KDateTime only.
           @return the list of unfiltered Events occurring on the specified
           timestamp.
        */
        virtual KCal::Event::List rawEventsForDate(const KDateTime& dt);

        /**
           Return an unfiltered list of all Events occurring within a date range.

           @param start is the starting date.
           @param end is the ending date.
           @param timespec the time spec to use to interpret @p start and @p end
           @param inclusive if true only Events which are completely included
           within the date range are returned.
           @return the list of unfiltered Events occurring within the specified
           date range.
        */
        virtual KCal::Event::List rawEvents(const QDate& start, const QDate& end,
                               const KDateTime::Spec& timespec = KDateTime::Spec(),
                       bool inclusive = false);

        /**
           Return a sorted, unfiltered list of all Events which occur on the given
           date.  The Events are sorted according to @a sortField and
           @a sortDirection.

           @param date request unfiltered Event list for this QDate only.
           @param timespec the time spec to use to interpret @p date
           @param sortField specifies the EventSortField.
           @param sortDirection specifies the SortDirection.
           @return the list of sorted, unfiltered Events occurring on @a date.
        */
        virtual KCal::Event::List rawEventsForDate(
          const QDate& date,
          const KDateTime::Spec& timespec = KDateTime::Spec(),
          KCal::EventSortField sortField = KCal::EventSortUnsorted,
          KCal::SortDirection sortDirection = KCal::SortDirectionAscending);

        /**
           Returns the Event associated with the given unique identifier.

           @param uid is a unique identifier string.
           @return a pointer to the Event.
           A null pointer is returned if no such Event exists.
        */
        virtual KCal::Event* event(const QString& uid);

        // Alarm Specific Methods //

        /**
           Return a list of Alarms within a time range for this Calendar.

           @param from is the starting timestamp.
           @param to is the ending timestamp.
           @return the list of Alarms for the for the specified time range.
        */
        virtual KCal::Alarm::List alarms(const KDateTime& from, const KDateTime& to);

        /**
           Return a list of Alarms that occur before the specified timestamp.

           @param to is the ending timestamp.
           @return the list of Alarms occurring before the specified KDateTime.
        */
        KCal::Alarm::List alarmsTo(const KDateTime& to);

        static void setDebugArea(int area)   { AlarmResource::setDebugArea(area); }

        using QObject::event;   // prevent warning about hidden virtual method

  signals:
    /** Signal that the Resource has been modified. */
    void signalResourceModified(AlarmResource*);

    /** Signal an error message. */
    void signalErrorMessage(const QString& err);

        /** Signal that a different standard resource has been set for the given alarm type. */
        void standardResourceChange(CalEvent::Type);

        void resourceSaved(AlarmResource*);
        /** Signal that a remote resource's cache has completed downloading. */
        void cacheDownloaded(AlarmResource*);
        /** Signal that one resource has completed loading. */
        void resourceLoaded(AlarmResource*, bool success);
        /** Emitted at start of download only if mShowProgress is true. */
        void downloading(AlarmResource*, unsigned long percent);
        /** Signal that a resource has been added or deleted, or a resource's
         *  enabled or read-only status has changed. */
        void resourceStatusChanged(AlarmResource*, AlarmResources::Change);

    protected:
        virtual void connectResource(AlarmResource*);
        virtual void resourceModified(AlarmResource*)  {}
        virtual void resourceDeleted(AlarmResource*);

        /**
           Let CalendarResource subclasses set the time specification
           (time zone, etc.)
        */
        virtual void doSetTimeSpec(const KDateTime::Spec& timeSpec);

    /**
       Increment the number of times this Resource has been changed.

       @param resource is a pointer to the AlarmResource to be counted.
       @return the new number of times this Resource has been changed.
    */
    int incrementChangeCount(AlarmResource* resource);

    /**
       Decrement the number of times this Resource has been changed.

       @param resource is a pointer to the AlarmResource to be counted.
       @return the new number of times this Resource has been changed.
    */
    int decrementChangeCount(AlarmResource* resource);

    protected slots:
        void slotLoadError(ResourceCalendar*, const QString& err);
        void slotSaveError(ResourceCalendar*, const QString& err);

    private:
        // Override unused virtual functions
        virtual bool addTodo(KCal::Todo*) { return true; }
        virtual bool deleteTodo(KCal::Todo*) { return true; }
        virtual void deleteAllTodos() {}
        virtual KCal::Todo::List rawTodos(KCal::TodoSortField = KCal::TodoSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending) { return KCal::Todo::List(); }
        virtual KCal::Todo::List rawTodosForDate(const QDate&) { return KCal::Todo::List(); }
        virtual KCal::Todo* todo(const QString&) { return 0; }
        virtual bool addJournal(KCal::Journal*) { return true; }
        virtual bool deleteJournal(KCal::Journal*) { return true; }
        virtual void deleteAllJournals() {}
        virtual KCal::Journal::List rawJournals(KCal::JournalSortField = KCal::JournalSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending) { return KCal::Journal::List(); }
        virtual KCal::Journal::List rawJournalsForDate(const QDate&) { return KCal::Journal::List(); }
        virtual KCal::Journal* journal(const QString&) { return 0; }

    private slots:
        void slotActiveChanged(AlarmResource* r)      { slotResourceStatusChanged(r, Enabled); }
        void slotReadOnlyChanged(AlarmResource* r)    { slotResourceStatusChanged(r, ReadOnly); }
        void slotWrongTypeChanged(AlarmResource* r)   { slotResourceStatusChanged(r, WrongType); }
        void slotLocationChanged(AlarmResource* r)    { slotResourceStatusChanged(r, Location); }
        void slotColourChanged(AlarmResource* r)      { slotResourceStatusChanged(r, Colour); }
        void slotResourceLoaded(AlarmResource*);
        void slotResourceSaved(AlarmResource*);
        void slotCacheDownloaded(AlarmResource*);
        void slotResourceChanged(ResourceCalendar*);
        void slotResourceInvalidated(AlarmResource*);

    private:
        AlarmResources(const KDateTime::Spec& timeSpec, bool activeOnly, bool passiveClient);
        AlarmResource* addDefaultResource(const KConfigGroup&, CalEvent::Type);
        AlarmResource* destination(KCal::Incidence*, QWidget* promptParent, bool* cancelled = 0);
        void  appendEvents(KCal::Event::List& result, const KCal::Event::List& events, AlarmResource*);
        void  slotResourceStatusChanged(AlarmResource*, Change);
        void  remap(AlarmResource*);

        static AlarmResources* mInstance;
        static QString         mReservedFile;        // disallowed file path
        static QString         mConstructionError;   // error string if an error occurred in creating instance

        KRES::Manager<AlarmResource>* mManager;
        typedef QMap<KCal::Incidence*, AlarmResource*> ResourceMap;
        ResourceMap                   mResourceMap;
        QMap<AlarmResource*, Ticket*> mTickets;
        QMap<AlarmResource*, int>     mChangeCounts;
        bool        mActiveOnly;      // only resource calendars containing ACTIVE alarms are to be opened
        bool        mPassiveClient;   // true if client never initiates changes
        bool        mNoGui;           // application has no GUI, so don't display messages
        bool        mAskDestination;  // true to prompt user which resource to store new events in
        bool        mShowProgress;    // emit download progress signals
        bool        mOpen;
        bool        mClosing;         // close() is in progress
};

#endif

// vim: et sw=4:
