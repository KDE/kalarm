/*
 *  akonadi_serializer_kalarm.cpp  -  Akonadi resource serializer for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009-2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "akonadi_serializer_kalarm.h"
#include "eventattribute.h"
#include "kacalendar.h"
#include "kaevent.h"
#include "kaeventformatter.h"

#include <akonadi/item.h>
#include <akonadi/abstractdifferencesreporter.h>
#include <akonadi/attributefactory.h>

#include <klocale.h>
#include <kdebug.h>

#include <QtCore/qplugin.h>

using namespace Akonadi;


bool SerializerPluginKAlarm::deserialize(Item& item, const QByteArray& label, QIODevice& data, int version)
{
    Q_UNUSED(version);

    if (label != Item::FullPayload)
        return false;

    KCalCore::Incidence::Ptr i = mFormat.fromString(QString::fromUtf8(data.readAll()));
    if (!i)
    {
        kWarning(5263) << "Failed to parse incidence!";
        data.seek(0);
        kWarning(5263) << QString::fromUtf8(data.readAll());
        return false;
    }
    if (i->type() != KCalCore::Incidence::TypeEvent)
    {
        kWarning(5263) << "Incidence with uid" << i->uid() << "is not an Event!";
        data.seek(0);
        return false;
    }
    KAEvent event(i.staticCast<KCalCore::Event>());
    QString mime = KAlarm::CalEvent::mimeType(event.category());
    if (mime.isEmpty()  ||  !event.isValid())
    {
        kWarning(5263) << "Event with uid" << event.id() << "contains no usable alarms!";
        data.seek(0);
        return false;
    }
    event.setItemId(item.id());

    // Set additional event data contained in attributes
    static bool attrRegistered = false;
    if (!attrRegistered)
    {
        AttributeFactory::registerAttribute<KAlarm::EventAttribute>();
        attrRegistered = true;
    }
    if (item.hasAttribute<KAlarm::EventAttribute>())
    {
        KAEvent::CmdErrType err = item.attribute<KAlarm::EventAttribute>()->commandError();
        event.setCommandError(err);
    }

    item.setMimeType(mime);
    item.setPayload<KAEvent>(event);
    return true;
}

void SerializerPluginKAlarm::serialize(const Item& item, const QByteArray& label, QIODevice& data, int& version)
{
    Q_UNUSED(version);

    if (label != Item::FullPayload || !item.hasPayload<KAEvent>())
        return;
    KAEvent e = item.payload<KAEvent>();
    KCalCore::Event::Ptr kcalEvent(new KCalCore::Event);
    e.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
    QByteArray head = "BEGIN:VCALENDAR\nPRODID:";
    head += KAlarm::Calendar::icalProductId();
    head += "\nVERSION:2.0\nX-KDE-KALARM-VERSION:";
    head += KAEvent::currentCalendarVersionString();
    head += '\n';
    data.write(head);
    data.write(mFormat.toString(kcalEvent.staticCast<KCalCore::Incidence>()).toUtf8());
    data.write("\nEND:VCALENDAR");
}

#include <kglobal.h>

void SerializerPluginKAlarm::compare(AbstractDifferencesReporter* reporter, const Item& left, const Item& right)
{
    Q_ASSERT(reporter);
    Q_ASSERT(left.hasPayload<KAEvent>());
    Q_ASSERT(right.hasPayload<KAEvent>());

    KAEvent eventL = left.payload<KAEvent>();
    KAEvent eventR = right.payload<KAEvent>();
    // Note that event attributes are not included, since they are not part of the payload
    mValueL = KAEventFormatter(eventL, false);
    mValueR = KAEventFormatter(eventR, false);

    reporter->setLeftPropertyValueTitle(i18nc("@title:column", "Changed Alarm"));
    reporter->setRightPropertyValueTitle(i18nc("@title:column", "Conflicting Alarm"));

    reportDifference(reporter, KAEventFormatter::Id);
    if (eventL.revision() != eventR.revision())
        reportDifference(reporter, KAEventFormatter::Revision);
    if (eventL.action() != eventR.action())
        reportDifference(reporter, KAEventFormatter::AlarmType);
    if (eventL.category() != eventR.category())
        reportDifference(reporter, KAEventFormatter::AlarmCategory);
    if (eventL.templateName() != eventR.templateName())
        reportDifference(reporter, KAEventFormatter::TemplateName);
    if (eventL.createdDateTime() != eventR.createdDateTime())
        reportDifference(reporter, KAEventFormatter::CreatedTime);
    if (eventL.startDateTime() != eventR.startDateTime())
        reportDifference(reporter, KAEventFormatter::StartTime);
    if (eventL.templateAfterTime() != eventR.templateAfterTime())
        reportDifference(reporter, KAEventFormatter::TemplateAfterTime);
    if (*eventL.recurrence() != *eventR.recurrence())
        reportDifference(reporter, KAEventFormatter::Recurrence);
    if (eventL.mainDateTime(true) != eventR.mainDateTime(true))
        reportDifference(reporter, KAEventFormatter::NextRecurrence);
    if (eventL.repetition() != eventR.repetition())
        reportDifference(reporter, KAEventFormatter::SubRepetition);
    if (eventL.repetition().interval() != eventR.repetition().interval())
        reportDifference(reporter, KAEventFormatter::RepeatInterval);
    if (eventL.repetition().count() != eventR.repetition().count())
        reportDifference(reporter, KAEventFormatter::RepeatCount);
    if (eventL.nextRepetition() != eventR.nextRepetition())
        reportDifference(reporter, KAEventFormatter::NextRepetition);
    if (eventL.holidaysExcluded() != eventR.holidaysExcluded())
        reportDifference(reporter, KAEventFormatter::HolidaysExcluded);
    if (eventL.workTimeOnly() != eventR.workTimeOnly())
        reportDifference(reporter, KAEventFormatter::WorkTimeOnly);
    if (eventL.lateCancel() != eventR.lateCancel())
        reportDifference(reporter, KAEventFormatter::LateCancel);
    if (eventL.autoClose() != eventR.autoClose())
        reportDifference(reporter, KAEventFormatter::AutoClose);
    if (eventL.copyToKOrganizer() != eventR.copyToKOrganizer())
        reportDifference(reporter, KAEventFormatter::CopyKOrganizer);
    if (eventL.enabled() != eventR.enabled())
        reportDifference(reporter, KAEventFormatter::Enabled);
    if (eventL.isReadOnly() != eventR.isReadOnly())
        reportDifference(reporter, KAEventFormatter::ReadOnly);
    if (eventL.toBeArchived() != eventR.toBeArchived())
        reportDifference(reporter, KAEventFormatter::Archive);
    if (eventL.customProperties() != eventR.customProperties())
        reportDifference(reporter, KAEventFormatter::CustomProperties);
    if (eventL.message() != eventR.message())
        reportDifference(reporter, KAEventFormatter::MessageText);
    if (eventL.fileName() != eventR.fileName())
        reportDifference(reporter, KAEventFormatter::MessageFile);
    if (eventL.fgColour() != eventR.fgColour())
        reportDifference(reporter, KAEventFormatter::FgColour);
    if (eventL.bgColour() != eventR.bgColour())
        reportDifference(reporter, KAEventFormatter::BgColour);
    if (eventL.font() != eventR.font())
        reportDifference(reporter, KAEventFormatter::Font);
    if (eventL.preAction() != eventR.preAction())
        reportDifference(reporter, KAEventFormatter::PreAction);
    if (eventL.cancelOnPreActionError() != eventR.cancelOnPreActionError())
        reportDifference(reporter, KAEventFormatter::PreActionCancel);
    if (eventL.dontShowPreActionError() != eventR.dontShowPreActionError())
        reportDifference(reporter, KAEventFormatter::PreActionNoError);
    if (eventL.postAction() != eventR.postAction())
        reportDifference(reporter, KAEventFormatter::PostAction);
    if (eventL.confirmAck() != eventR.confirmAck())
        reportDifference(reporter, KAEventFormatter::ConfirmAck);
    if (eventL.kmailSerialNumber() != eventR.kmailSerialNumber())
        reportDifference(reporter, KAEventFormatter::KMailSerial);
    if (eventL.beep() != eventR.beep()
    ||  eventL.speak() != eventR.speak()
    ||  eventL.audioFile() != eventR.audioFile())
        reportDifference(reporter, KAEventFormatter::Sound);
    if (eventL.repeatSound() != eventR.repeatSound())
        reportDifference(reporter, KAEventFormatter::SoundRepeat);
    if (eventL.soundVolume() != eventR.soundVolume())
        reportDifference(reporter, KAEventFormatter::SoundVolume);
    if (eventL.fadeVolume() != eventR.fadeVolume())
        reportDifference(reporter, KAEventFormatter::SoundFadeVolume);
    if (eventL.fadeSeconds() != eventR.fadeSeconds())
        reportDifference(reporter, KAEventFormatter::SoundFadeTime);
    if (eventL.reminderMinutes() != eventR.reminderMinutes())
        reportDifference(reporter, KAEventFormatter::Reminder);
    if (eventL.reminderOnceOnly() != eventR.reminderOnceOnly())
        reportDifference(reporter, KAEventFormatter::ReminderOnce);
    if (eventL.deferred() != eventR.deferred())
        reportDifference(reporter, KAEventFormatter::DeferralType);
    if (eventL.deferDateTime() != eventR.deferDateTime())
        reportDifference(reporter, KAEventFormatter::DeferralTime);
    if (eventL.deferDefaultMinutes() != eventR.deferDefaultMinutes())
        reportDifference(reporter, KAEventFormatter::DeferDefault);
    if (eventL.deferDefaultDateOnly() != eventR.deferDefaultDateOnly())
        reportDifference(reporter, KAEventFormatter::DeferDefaultDate);
    if (eventL.command() != eventR.command())
        reportDifference(reporter, KAEventFormatter::Command);
    if (eventL.logFile() != eventR.logFile())
        reportDifference(reporter, KAEventFormatter::LogFile);
    if (eventL.commandXterm() != eventR.commandXterm())
        reportDifference(reporter, KAEventFormatter::CommandXTerm);
    if (eventL.emailSubject() != eventR.emailSubject())
        reportDifference(reporter, KAEventFormatter::EmailSubject);
    if (eventL.emailFromId() != eventR.emailFromId())
        reportDifference(reporter, KAEventFormatter::EmailFromId);
    if (eventL.emailAddresses() != eventR.emailAddresses())
        reportDifference(reporter, KAEventFormatter::EmailTo);
    if (eventL.emailBcc() != eventR.emailBcc())
        reportDifference(reporter, KAEventFormatter::EmailBcc);
    if (eventL.emailMessage() != eventR.emailMessage())
        reportDifference(reporter, KAEventFormatter::EmailBody);
    if (eventL.emailAttachments() != eventR.emailAttachments())
        reportDifference(reporter, KAEventFormatter::EmailAttachments);

    KLocale* locale = KGlobal::locale();
    reporter->addProperty(AbstractDifferencesReporter::ConflictMode, i18nc("@label", "Item revision"),
                          locale->convertDigits(QString::number(left.revision()), locale->digitSet()),
                          locale->convertDigits(QString::number(right.revision()), locale->digitSet()));
}

void SerializerPluginKAlarm::reportDifference(AbstractDifferencesReporter* reporter, KAEventFormatter::Parameter id)
{
    if (mValueL.isApplicable(id)  ||  mValueR.isApplicable(id))
        reporter->addProperty(AbstractDifferencesReporter::ConflictMode, KAEventFormatter::label(id), mValueL.value(id), mValueR.value(id));
}

Q_EXPORT_PLUGIN2(akonadi_serializer_kalarm, SerializerPluginKAlarm)

#include "akonadi_serializer_kalarm.moc"

// vim: et sw=4:
