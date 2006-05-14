/*
 *  alarmresource.h  -  base class for a KAlarm alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
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
#include <libkcal/resourcecached.h>
#include "kcal.h"

class KConfig;
using KCal::CalendarLocal;


#define KARES_DEBUG AlarmResource::debugArea()


/** Base class for a KAlarm alarm calendar resource. */
class AlarmResource : public KCal::ResourceCached
{
		Q_OBJECT
	public:
		/** Type of alarms held in this calendar resource. */
		enum Type {
			ACTIVE   = 0x01,    // active alarms
			EXPIRED  = 0x02,    // expired alarms
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

		/** Return the location of the resource (URL, file path, etc.) */
		virtual QString location(bool prefix = false) const = 0;

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
		bool     writable(const QString& eventID) const;

		/** Return whether the resource is cached, i.e. whether it is downloaded
		 *  and stored locally in a cache file. */
		virtual bool cached() const              { return false; }

		/** Set the default cache update action when loading the resource.
		 *  For remote resources, if @p update is true, and the resource has a
		 *  reload policy of LoadOnStartup, and the resource has not yet been
		 *  downloaded, a download is initiated.
		 *  @return false if nothing changed
		 */
		virtual bool setLoadUpdateCache(bool update);
		bool loadUpdateCache() const             { return mLoadCacheUpdate; }

		/** Return whether the resource is read-only, either because it's marked as
		 *  read-only, or because it's active but not in the current KAlarm format. */
		virtual bool readOnly() const;
		virtual void setReadOnly(bool);

		/** Start a batch of configuration changes.
		 *  The changes will be stored up until applyReconfig() is called. */
		virtual void startReconfig();

		/** Apply the batch of configuration changes since startReconfig() was called. */
		virtual void applyReconfig();

		/** Load the resource, specifying whether to refresh the cache file first.
		 *  If loading succeeds, the loaded() signal is emitted on completion (but is
		 *  not emitted if false is returned).
		 *  Loading is not performed if the resource is disabled.
		 *  @param refreshCache if the resource is cached, whether to update the cache
		 *                      file before loading from the cache file.
		 *  @return true if loading succeeded at least partially, false if it failed
		 *          completely
		 */
		virtual bool loadCached(bool /*refreshCache*/)  { return load(); }

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
		virtual bool load();

		/** Reload the resource from disc.
		 *  If the resource is cached, just load from the cache file
		 *  without updating the cache file first.
		 */
		bool     refresh();

		/** Return whether the resource has fully loaded. */
		bool     isLoaded() const                { return mLoaded; }

		/** Return whether the resource is in the process of loading. */
		bool     isLoading() const               { return mLoading; }

		/** Set a function to fix the calendar once it has been loaded. */
		void     setFixFunction(KCalendar::Status (*f)(CalendarLocal&, const QString&, AlarmResource*, FixFunc))
		                                         { mFixFunction = f; }
		/** Return whether the resource is in a different format from the
		 *  current KAlarm format, in which case it cannot be written to.
		 *  Note that readOnly() takes account of both incompatible() and
		 *  KCal::ResourceCached::readOnly().
		 */
		KCalendar::Status compatibility() const  { return mCompatibility; }
		KCalendar::Status compatibility(const KCal::Event*) const;

		virtual void showProgress(bool)  {}

		static int debugArea()              { return mDebugArea; }
		static void setDebugArea(int area)  { mDebugArea = area; }

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
		void              setCompatibility(KCalendar::Status c)    { mCompatibility = c; }
		void              checkCompatibility(const QString&);
		KCalendar::Status checkCompatibility(KCal::CalendarLocal&, const QString& filename, FixFunc);
		virtual void      enableResource(bool enable) = 0;
		void              lock(const QString& path);

		KCalendar::Status (*mFixFunction)(CalendarLocal&, const QString&, AlarmResource*, FixFunc);

	private:
		static int  mDebugArea;       // area for kDebug() output

		KABC::Lock* mLock;
		Type        mType;            // type of alarm held in this resource
		bool        mStandard;        // standard resource for this mWriteType
		bool        mLoadCacheUpdate; // whether to update cache when loading the resource
		bool        mNewReadOnly;     // new read-only status (while mReconfiguring = 1)
		bool        mOldReadOnly;     // old read-only status (when startReconfig() called)
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
