/*
 *  alarmresource.cpp  -  base class for a KAlarm alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2008 by David Jarvie <djarvie@kde.org>
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
#include <kconfiggroup.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kabc/lock.h>
#include <kabc/locknull.h>
#include <kcal/calendarlocal.h>

#include "alarmresource.moc"
using namespace KCal;
#include "alarmresources.h"


void              (*AlarmResource::mCalIDFunction)(CalendarLocal&) = 0;
KCalendar::Status (*AlarmResource::mFixFunction)(CalendarLocal&, const QString&, AlarmResource*, AlarmResource::FixFunc) = 0;
int                 AlarmResource::mDebugArea = 0;
bool                AlarmResource::mNoGui = false;


AlarmResource::AlarmResource()
	: ResourceCached(),
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
	init();
}

AlarmResource::AlarmResource(const KConfigGroup& group)
	: ResourceCached(group),
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

	ResourceCached::readConfig(group);
	int type = group.readEntry("AlarmType", static_cast<int>(ACTIVE));
	switch (type)
	{
		case ACTIVE:
		case ARCHIVED:
		case TEMPLATE:
			mType = static_cast<Type>(type);
			mStandard = group.readEntry("Standard", true);
			break;
		default:
			break;
	}
	mColour = group.readEntry("Color", QColor());
	init();
}

AlarmResource::AlarmResource(Type type)
	: ResourceCached(),
	  mLock(0),
	  mType(type),
	  mStandard(false),
	  mCloseAfterSave(false),
	  mCompatibility(KCalendar::Incompatible),
	  mReconfiguring(0),
	  mLoaded(false),
	  mLoading(false)
{
	init();
}

void AlarmResource::init()
{
	enableChangeNotification();
	if (mType == ARCHIVED)
	{
		// Prevent unnecessary multiple saves of archived alarm resources.
		// When multiple alarms are deleted as a group, the archive
		// resource would be saved once for each alarm. Ironically, setting
		// the resource to be automatically saved will prevent this, since
		// automatic saving delays for a second after each change before
		// actually saving the resource, thereby ensuring that they are
		// saved as a group.
		setSavePolicy(SaveAlways);
	}
}

AlarmResource::~AlarmResource()
{
	delete mLock;
}

void AlarmResource::writeConfig(KConfigGroup& group)
{
	group.writeEntry("AlarmType", static_cast<int>(mType));
	if (mColour.isValid())
		group.writeEntry("Color", mColour);
	else
		group.deleteEntry("Color");
	group.writeEntry("Standard", mStandard);
	ResourceCached::writeConfig(group);
	ResourceCalendar::writeConfig(group);
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
	bool oldReadOnly = readOnly();
	mCompatibility = KCalendar::Incompatible;   // assume the worst
	if (mFixFunction)
	{
		// Check whether the version is compatible (and convert it if desired)
		mCompatibility = (*mFixFunction)(*calendar(), filename, this, PROMPT);
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
			kDebug(KARES_DEBUG) << resourceName() << ": opened read-only (not current KAlarm format)";
		}
	}
	if (readOnly() != oldReadOnly)
		emit readOnlyChanged(this);   // the effective read-only status has changed
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
	kDebug(KARES_DEBUG) << ronly;
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

void AlarmResource::setColour(const QColor& colour)
{
	if (colour != mColour)
	{
		mColour = colour;
		emit colourChanged(this);
	}
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
	mCloseAfterSave = false;
	emit invalidate(this);
	KCal::ResourceCached::doClose();
	mLoaded = mLoading = false;
	mCompatibilityMap.clear();
}

QString AlarmResource::infoText() const
{
	KRES::Factory* factory = KRES::Factory::self("alarms");
	QString atype;
	switch (mType)
	{
		case ACTIVE:    atype = i18nc("@info/plain", "Active alarms");  break;
		case ARCHIVED:  atype = i18nc("@info/plain", "Archived alarms");  break;
		case TEMPLATE:  atype = i18nc("@info/plain", "Alarm templates");  break;
		default:        break;
	}
	QString perms = readOnly() ? i18nc("@info/plain", "Read-only") : i18nc("@info/plain", "Read-write");
	QString enabled = isActive() ? i18nc("@info/plain", "Enabled") : i18nc("@info/plain", "Disabled");
	QString std = (AlarmResources::instance()->getStandardResource(mType) == this) ? i18nc("@info/plain Parameter in 'Default resource: Yes/No'", "Yes") : i18nc("@info/plain Parameter in 'Default resource: Yes/No'", "No");
	return i18nc("@info",
	    "<title>%1</title>"
	    "<para>Resource type: %2<nl/>"
	    "Contents: %3<nl/>"
	    "%4: <filename>%5</filename><nl/>"
	    "Permissions: %6<nl/>"
	    "Status: %7<nl/>"
	    "Default resource: %8</para>",
	    resourceName(), factory->typeName(type()), atype, displayType(), displayLocation(), perms, enabled, std);
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
