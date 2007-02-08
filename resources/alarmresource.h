/*
 *  alarmresource.h  -  base class for a KAlarm alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006,2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ALARMRESOURCE_H
#define ALARMRESOURCE_H

/* @file alarmresource.h - base class for a KAlarm alarm calendar resource */

#include <sys/stat.h>

#include <QMap>
#include <QByteArray>
#include <kcal/resourcecached.h>
#include "kcalendar.h"

class KConfig;
using KCal::CalendarLocal;


#define KARES_DEBUG AlarmResource::debugArea()


/** Base class for a KAlarm alarm calendar resource. */
class KDE_EXPORT AlarmResource : public KCal::ResourceCached
{
		Q_OBJECT
	public:
		/** Type of alarms held in this calendar resource. */
		enum Type {
			ACTIVE   = 0x01,    // active alarms
			ARCHIVED = 0x02,    // archived alarms
			TEMPLATE = 0x04     // alarm templates
		};
		/** Whether the fix function should convert old format KAlarm calendars. */
		enum FixFunc { PROMPT, PROMPT_PART, CONVERT, NO_CONVERT };

		explicit AlarmResource(const KConfig*);
		explicit AlarmResource(Type);
		~AlarmResource();
		virtual void writeConfig(KConfig*);
		virtual QString infoText() const;
		KABC::Lock*  lock()                      { return mLock; }

		/** Return which type of alarms the resource can contain. */
		Type     alarmType() const               { return mType; }

		/** Return which type of alarms the resource can contain. */
		KCalEvent::Status kcalEventType() const;

		/** Set the type of alarms which the resource can contain. */
		void     setAlarmType(Type type)         { mType = type; }

		/** Set whether the application has a GUI. This determines whether error or
		 *  progress messages are displayed. */
		static void setNoGui(bool noGui)         { mNoGui = noGui; }
		static bool hasGui()                     { return !mNoGui; }

		/** Return the location(s) of the resource (URL, file path, etc.) */
		virtual QStringList location() const = 0;

		/** Return the location of the resource (URL, file path, etc.)
		 *  for display purposes. */
		virtual QString displayLocation(bool prefix = false) const = 0;

		/** Change the resource's location. The resource will be reloaded if active. */
		virtual bool setLocation(const QString& locn, const QString& locn2 = QString()) = 0;
		/** Return whether the resource is the standard resource for its alarm type. */
		bool     standardResource() const        { return mStandard; }

		/** Set or clear the resource as the standard resource for its alarm type. */
		void     setStandardResource(bool std)   { mStandard = std; }

		void     setEnabled(bool enable);
		bool     isEnabled() const               { return isActive(); }

		/** Return whether the resource can be written to now,
		 *  i.e. it's active, read-write and in the current KAlarm format. */
		bool     writable() const;

		/** Return whether the event can be written to now, i.e. the resource is
		 *  active and read-write, and the event is in the current KAlarm format. */
		bool     writable(const KCal::Event*) const;

		/** Return whether the resource is cached, i.e. whether it is downloaded
		 *  and stored locally in a cache file. */
		virtual bool cached() const              { return false; }

		/** Return whether the resource is read-only, either because it's marked as
		 *  read-only, or because it's active but not in the current KAlarm format. */
		virtual bool readOnly() const;
		virtual void setReadOnly(bool);

		/** Start a batch of configuration changes.
		 *  The changes will be stored up until applyReconfig() is called. */
		virtual void startReconfig();

		/** Apply the batch of configuration changes since startReconfig() was called. */
		virtual void applyReconfig();

		/** Load the resource.
		 *  If it's a cached resource, load() uses the default action to either refresh
		 *  the cache file first or not.
		 *  If loading succeeds, the loaded() signal is emitted on completion (but is
		 *  not emitted if false is returned). This allows AlarmResources to process
		 *  the load.
		 *  Loading is not performed if the resource is disabled.
		 *  @return true if loading succeeded at least partially, false if it failed
		 *          completely
		 */
		virtual bool load(CacheAction);

		/** Return whether the resource has fully loaded. */
		bool     isLoaded() const                { return mLoaded; }

		/** Return whether the resource is in the process of loading. */
		bool     isLoading() const               { return mLoading; }

		/** Save the resource and then close it.
		 *  It will be closed even if saving fails. */
		bool saveAndClose(CacheAction, KCal::Incidence* = 0);
		bool saveAndClose(KCal::Incidence* incidence = 0)  { return saveAndClose(DefaultCache, incidence); }

		/** Set a function to write the application ID into a calendar. */
		static void setCalIDFunction(void (*f)(CalendarLocal&))    { mCalIDFunction = f; }
		/** Set a function to fix the calendar once it has been loaded. */
		static void setFixFunction(KCalendar::Status (*f)(CalendarLocal&, const QString&, AlarmResource*, FixFunc))
		                                         { mFixFunction = f; }
		/** Return whether the resource is in a different format from the
		 *  current KAlarm format, in which case it cannot be written to.
		 *  Note that readOnly() takes account of both incompatible() and
		 *  KCal::ResourceCached::readOnly().
		 */
		KCalendar::Status compatibility() const  { return mCompatibility; }
		KCalendar::Status compatibility(const KCal::Event*) const;

		virtual void showProgress(bool)  {}

		static int debugArea()               { return mDebugArea; }
		static void setDebugArea(int area)   { mDebugArea = area; }

#ifndef NDEBUG
		QByteArray typeName() const;
#endif

	public slots:
		virtual void cancelDownload(bool /*disable*/ = false)  {}

	signals:
		/** Signal that loading of the resource has completed, whether
		 *  successfully or not.
		 *  This signal is always emitted after a resource is loaded. */
		void loaded(AlarmResource*);
		/** Signal that loading of the resource been successfully initiated
		 *  (successfully completed in the case of local resources). */
		void resLoaded(AlarmResource*);
		/** Emitted after attempting to save the resource, whether successfully or not.
		 *  Not emitted if no attempt was made to save it (e.g. if the resource
		 *  is closed or read-only or there is nothing to save, or if save()
		 *  returned false).
		 */
		void resourceSaved(AlarmResource*);
		/** Emitted during download for remote resources. */
		void downloading(AlarmResource*, unsigned long percent);
		/** Signal that a remote resource download has completed, and the cache file has been updated. */
		void cacheDownloaded(AlarmResource*);
		/** Signal that the resource's read-only status has changed. */
		void readOnlyChanged(AlarmResource*);
		/** Signal that the resource's active status has changed. */
		void enabledChanged(AlarmResource*);
		/** Signal that the resource's location has changed. */
		void locationChanged(AlarmResource*);
		/** Signal that the resource cannot be set read-write since its format is incompatible. */
		void notWritable(AlarmResource*);

	protected:
		virtual void      doClose();
		bool              closeAfterSave() const    { bool ret = mCloseAfterSave; mCloseAfterSave = false; return ret; }
		void              setCompatibility(KCalendar::Status c)    { mCompatibility = c; }
		void              checkCompatibility(const QString&);
		KCalendar::Status checkCompatibility(KCal::CalendarLocal&, const QString& filename, FixFunc);
		virtual void      enableResource(bool enable) = 0;
		void              lock(const QString& path);

		static void              (*mCalIDFunction)(CalendarLocal&);
		static KCalendar::Status (*mFixFunction)(CalendarLocal&, const QString&, AlarmResource*, FixFunc);

	private:
		static int  mDebugArea;       // area for kDebug() output
		static bool mNoGui;           // application has no GUI, so don't display messages

		KABC::Lock* mLock;
		Type        mType;            // type of alarm held in this resource
		bool        mStandard;        // this is the standard resource for this mWriteType
		bool        mNewReadOnly;     // new read-only status (while mReconfiguring = 1)
		bool        mOldReadOnly;     // old read-only status (when startReconfig() called)
		mutable bool mCloseAfterSave;  // resource is to be closed once save() is complete
		KCalendar::Status mCompatibility; // whether resource is in compatible format
	protected:
		typedef QMap<const KCal::Event*, KCalendar::Status>  CompatibilityMap;
		CompatibilityMap  mCompatibilityMap;   // whether individual events are in compatible format
		short       mReconfiguring;   // a batch of config changes is in progress
		bool        mLoaded;          // true if resource has finished loading
		bool        mLoading;         // true if resource is currently loading
};

typedef KRES::Manager<AlarmResource> AlarmResourceManager;

#endif
