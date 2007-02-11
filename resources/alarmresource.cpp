/*
 *  alarmresource.cpp  -  base class for a KAlarm alarm calendar resource
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

#include "kalarm.h"

#include <QFile>

#include <kconfig.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kabc/lock.h>
#include <kabc/locknull.h>

#include "alarmresource.moc"
using namespace KCal;
#include "alarmresources.h"


void              (*AlarmResource::mCalIDFunction)(CalendarLocal&) = 0;
KCalendar::Status (*AlarmResource::mFixFunction)(CalendarLocal&, const QString&, AlarmResource*, FixFunc) = 0;
int                 AlarmResource::mDebugArea = KARES_DEBUG;
bool                AlarmResource::mNoGui = false;


AlarmResource::AlarmResource(const KConfig* config)
	: ResourceCached(config),
	  mLock(0),
	  mType(static_cast<Type>(0)),    // invalid
	  mStandard(false),
	  mCloseAfterSave(false),
	  mCompatibility(KCalendar::Incompatible),
	  mReconfiguring(0),
	  mLoaded(false),
	  mLoading(false)
{
	// Prevent individual events being set read-only when loading a read-only resource
	setNoReadOnlyOnLoad(true);

	if (config)
	{
		ResourceCached::readConfig(config);
		int type = config->readEntry("AlarmType", static_cast<int>(ACTIVE));
		switch (type)
		{
			case ACTIVE:
			case ARCHIVED:
			case TEMPLATE:
				mType = static_cast<Type>(type);
				mStandard = config->readEntry("Standard", true);
				break;
			default:
				break;
		}
	}
	enableChangeNotification();
}

AlarmResource::AlarmResource(Type type)
	: ResourceCached(0),
	  mLock(0),
	  mType(type),
	  mStandard(false),
	  mCloseAfterSave(false),
	  mCompatibility(KCalendar::Incompatible),
	  mReconfiguring(0),
	  mLoaded(false),
	  mLoading(false)
{
	enableChangeNotification();
}

AlarmResource::~AlarmResource()
{
	delete mLock;
}

void AlarmResource::writeConfig(KConfig* config)
{
	config->writeEntry("AlarmType", static_cast<int>(mType));
	config->writeEntry("Standard", mStandard);
	ResourceCached::writeConfig(config);
	ResourceCalendar::writeConfig(config);
}

void AlarmResource::startReconfig()
{
	mOldReadOnly = ResourceCached::readOnly();
	mNewReadOnly = mOldReadOnly;
	mReconfiguring = 1;
}

void AlarmResource::applyReconfig()
{
	if (!mReconfiguring)
		return;
	if (mReconfiguring == 1)
	{
		// Called before derived classes do their stuff
		ResourceCached::setReadOnly(mNewReadOnly);
		mReconfiguring = 2;
	}
	else
	{
		// Called when derived classes have done their stuff
		setReadOnly(mNewReadOnly);
		mReconfiguring = 0;
	}
}

/******************************************************************************
* If a function is defined to convert alarms to the current format, call it.
* Set the resource to read-only if it isn't the current format version, or if
* its format is unknown.
*/
void AlarmResource::checkCompatibility(const QString& filename)
{
	mCompatibility = KCalendar::Incompatible;   // assume the worst
	if (!mFixFunction)
		return;
	// Check whether the version is compatible (and convert it if desired)
	mCompatibility = (*mFixFunction)(mCalendar, filename, this, PROMPT);
	if (mCompatibility == KCalendar::Converted)
	{
		// Set mCompatibility first to ensure that readOnly() returns
		// the correct value and that save() therefore works.
		mCompatibility = KCalendar::Current;
		save();
	}
	if (mCompatibility != KCalendar::Current  &&  mCompatibility != KCalendar::ByEvent)
	{
		// It's not in the current KAlarm format, so it will be read-only to prevent incompatible updates
		kDebug(KARES_DEBUG) << "AlarmResource::checkCompatibility(" << resourceName() << "): opened read-only (not current KAlarm format)" << endl;
	}
}

/******************************************************************************
* If a function is defined to convert alarms to the current format, call it to
* convert an individual file within the overall resource.
*/
KCalendar::Status AlarmResource::checkCompatibility(CalendarLocal& calendar, const QString& filename, FixFunc conv)
{
	KCalendar::Status compat = KCalendar::Incompatible;   // assume the worst
	if (mFixFunction)
	{
		// Check whether the version is compatible (and convert it if desired)
		compat = (*mFixFunction)(calendar, filename, this, conv);
		if (compat == KCalendar::Converted)
			calendar.save(filename);
	}
	return compat;
}

KCalendar::Status AlarmResource::compatibility(const Event* event) const
{
	if (mCompatibility != KCalendar::ByEvent)
		return mCompatibility;
	CompatibilityMap::ConstIterator it = mCompatibilityMap.find(event);
	if (it == mCompatibilityMap.end())
		return KCalendar::Incompatible;    // event not found!?! - assume the worst
	return it.value();
}

bool AlarmResource::writable() const
{
	return isActive()  &&  !KCal::ResourceCached::readOnly()
	   &&  (mCompatibility == KCalendar::Current || mCompatibility == KCalendar::ByEvent);
}

bool AlarmResource::writable(const Event* event) const
{
	return isActive()  &&  !KCal::ResourceCached::readOnly()
	   &&  compatibility(event) == KCalendar::Current;
}

bool AlarmResource::readOnly() const
{
	return KCal::ResourceCached::readOnly()
	   ||  (isActive()  &&  mCompatibility != KCalendar::Current && mCompatibility != KCalendar::ByEvent);
}

void AlarmResource::setReadOnly(bool ronly)
{
	if (mReconfiguring == 1)
	{
		mNewReadOnly = ronly;
		return;
	}
	kDebug(KARES_DEBUG) << "AlarmResource::setReadOnly(" << ronly << ")\n";
	bool oldRCronly = (mReconfiguring == 2) ? mOldReadOnly : ResourceCached::readOnly();
	bool oldronly = (oldRCronly || (mCompatibility != KCalendar::Current && mCompatibility != KCalendar::ByEvent));
	if (!ronly  &&  isActive())
	{
		// Trying to change the resource to read-write.
		// Only allow this if it is in, or can be converted to, the current KAlarm format.
		switch (mCompatibility)
		{
			case KCalendar::Incompatible:
				emit notWritable(this);    // allow an error message to be output
				return;
			case KCalendar::Convertible:
				if (mReconfiguring <= 2)
				{
					if (!isOpen())
						return;
					load(NoSyncCache);   // give user the option of converting it
				}
				if (mCompatibility != KCalendar::Current)
					return;    // not converted, so keep as read-only
				break;
			case KCalendar::Current:
			case KCalendar::ByEvent:
			case KCalendar::Converted:   // shouldn't ever happen
				break;
		}
	}
	if (ronly != oldRCronly)
		ResourceCached::setReadOnly(ronly);
	if ((ronly || mCompatibility != KCalendar::Current && mCompatibility != KCalendar::ByEvent) != oldronly)
		emit readOnlyChanged(this);   // the effective read-only status has changed
}

void AlarmResource::setEnabled(bool enable)
{
	if (isActive() != enable)
	{
		setActive(enable);
		enableResource(enable);
		emit enabledChanged(this);
	}
}

bool AlarmResource::load(CacheAction action)
{
	if (!ResourceCached::load(action))
		return false;
	emit resLoaded(this);    // special signal to AlarmResources
	return true;
}

bool AlarmResource::saveAndClose(CacheAction action, Incidence* incidence)
{
	bool result = save(action, incidence);
	if (isSaving())
		mCloseAfterSave = true;   // ensure it's closed if saving is asynchronous
	else
		close();
	return result;
}

void AlarmResource::doClose()
{
	KCal::ResourceCached::doClose();
	mLoaded = mLoading = false;
	mCompatibilityMap.clear();
}

QString AlarmResource::infoText() const
{
	QString txt = "<b>" + resourceName() + "</b>";
	KRES::Factory* factory = KRES::Factory::self("alarms");
	txt += "<br>" + i18n("Resource type: %1", factory->typeName(type()));
	QString type;
	switch (mType)
	{
		case ACTIVE:    type = i18n("Active alarms");  break;
		case ARCHIVED:  type = i18n("Archived alarms");  break;
		case TEMPLATE:  type = i18n("Alarm templates");  break;
		default:        break;
	}
	txt += "<br>" + i18nc("Content type (active alarms, etc)", "Contents: %1", type);
	txt += "<br>" + displayLocation(true);
	type = readOnly() ? i18n("Read-only") : i18n("Read-write");
	txt += "<br>" + i18nc("Access permissions (read-only, etc)", "Permissions: %1", type);
	type = isActive() ? i18n("Enabled") : i18n("Disabled");
	txt += "<br>" + i18nc("Enabled/disabled status", "Status: %1", type);
	type = (AlarmResources::instance()->getStandardResource(mType) == this) ? i18n("Yes") : i18n("No");
	txt += "<br>" + i18n("Default resource: %1", type);
	return txt;
}

void AlarmResource::lock(const QString& path)
{
	delete mLock;
	if (path.isNull())
		mLock = 0;
	else if (path.isEmpty())
		mLock = new KABC::LockNull(true);
	else
		mLock = new KABC::Lock(path);
}

KCalEvent::Status AlarmResource::kcalEventType() const
{
	switch (mType)
	{
		case ACTIVE:    return KCalEvent::ACTIVE;
		case ARCHIVED:  return KCalEvent::ARCHIVED;
		case TEMPLATE:  return KCalEvent::TEMPLATE;
		default:        return KCalEvent::EMPTY;
	}
}

/*
void AlarmResource::kaCheckCalendar(CalendarLocal& cal)
{
	mTypes = EMPTY;
	Event::List events = cal.rawEvents();
	for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
	{
		switch (KCalEvent::status(*it))
		{
			case KCalEvent::ACTIVE:    mTypes = static_cast<Type>(mTypes | ACTIVE);  break;
			case KCalEvent::ARCHIVED:  mTypes = static_cast<Type>(mTypes | ARCHIVED);  break;
			case KCalEvent::TEMPLATE:  mTypes = static_cast<Type>(mTypes | TEMPLATE);  break;
			default:   break;
		}
		if (mTypes == (ACTIVE | ARCHIVED | TEMPLATE))
			break;
	}
}
*/

#ifndef NDEBUG
QByteArray AlarmResource::typeName() const
{
	switch (mType)
	{
		case ACTIVE:    return "Active";
		case ARCHIVED:  return "Archived";
		case TEMPLATE:  return "Template";
		default:        return "Empty";
	}
}
#endif
