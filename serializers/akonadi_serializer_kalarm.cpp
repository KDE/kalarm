/*
 *  akonadi_serializer_kalarm.cpp  -  Akonadi resource serializer for KAlarm
 *  SPDX-FileCopyrightText: 2009-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "akonadi_serializer_kalarm.h"
#include "kaeventformatter.h"
#include "akonadi_serializer_kalarm_debug.h"

#include "eventattribute.h"
#include "kacalendar.h"
#include "kaevent.h"

#include <AkonadiCore/item.h>
#include <AkonadiCore/abstractdifferencesreporter.h>
#include <AkonadiCore/attributefactory.h>

#include <klocalizedstring.h>

#include <QLocale>
#include <qplugin.h>

using namespace Akonadi;
using namespace KAlarmCal;

// Convert from backend data stream to a KAEvent, and set it into the item's payload.
bool SerializerPluginKAlarm::deserialize(Item &item, const QByteArray &label, QIODevice &data, int version)
{
    Q_UNUSED(version);

    if (label != Item::FullPayload) {
        return false;
    }

    KCalendarCore::Incidence::Ptr i = mFormat.fromString(QString::fromUtf8(data.readAll()));
    if (!i) {
        qCWarning(AKONADI_SERIALIZER_KALARM_LOG) << "Failed to parse incidence!";
        data.seek(0);
        qCWarning(AKONADI_SERIALIZER_KALARM_LOG) << QString::fromUtf8(data.readAll());
        return false;
    }
    if (i->type() != KCalendarCore::Incidence::TypeEvent) {
        qCWarning(AKONADI_SERIALIZER_KALARM_LOG) << "Incidence with uid" << i->uid() << "is not an Event!";
        data.seek(0);
        return false;
    }
    KAEvent event(i.staticCast<KCalendarCore::Event>());
    const QString mime = CalEvent::mimeType(event.category());
    if (mime.isEmpty() || !event.isValid()) {
        qCWarning(AKONADI_SERIALIZER_KALARM_LOG) << "Event with uid" << event.id() << "contains no usable alarms!";
        data.seek(0);
        return false;
    }

    // Set additional event data contained in attributes
    if (mRegistered.isEmpty()) {
        AttributeFactory::registerAttribute<KAlarmCal::EventAttribute>();
        mRegistered = QStringLiteral("x");   // set to any non-null string
    }
    const EventAttribute dummy;
    if (item.hasAttribute(dummy.type())) {
        Attribute *a = item.attribute(dummy.type());
        if (!a) {
            qCCritical(AKONADI_SERIALIZER_KALARM_LOG) << "deserialize(): Event with uid" << event.id() << "contains null attribute";
        } else {
            EventAttribute *evAttr = dynamic_cast<EventAttribute *>(a);
            if (!evAttr) {
                // Registering EventAttribute doesn't work in the serializer
                // unless the application also registers it. This doesn't
                // matter unless the application uses KAEvent class.
                qCCritical(AKONADI_SERIALIZER_KALARM_LOG) << "deserialize(): Event with uid" << event.id()
                                                          << "contains unknown type EventAttribute (application must call AttributeFactory::registerAttribute())";
            } else {
                KAEvent::CmdErrType err = evAttr->commandError();
                event.setCommandError(err);
            }
        }
    }

    item.setMimeType(mime);
    item.setPayload<KAEvent>(event);
    return true;
}

// Convert an item's KAEvent payload to backend data stream.
void SerializerPluginKAlarm::serialize(const Item &item, const QByteArray &label, QIODevice &data, int &version)
{
    Q_UNUSED(version);

    if (label != Item::FullPayload || !item.hasPayload<KAEvent>()) {
        return;
    }
    const KAEvent e = item.payload<KAEvent>();
    KCalendarCore::Event::Ptr kcalEvent(new KCalendarCore::Event);
    e.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
    QByteArray head = "BEGIN:VCALENDAR\nPRODID:";
    head += KACalendar::icalProductId();
    head += "\nVERSION:2.0\nX-KDE-KALARM-VERSION:";
    head += KAEvent::currentCalendarVersionString();
    head += '\n';
    data.write(head);
    data.write(mFormat.toString(kcalEvent.staticCast<KCalendarCore::Incidence>()).toUtf8());
    data.write("\nEND:VCALENDAR");
}

void SerializerPluginKAlarm::compare(AbstractDifferencesReporter *reporter, const Item &left, const Item &right)
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
    if (eventL.revision() != eventR.revision()) {
        reportDifference(reporter, KAEventFormatter::Revision);
    }
    if (eventL.actionSubType() != eventR.actionSubType()) {
        reportDifference(reporter, KAEventFormatter::AlarmType);
    }
    if (eventL.category() != eventR.category()) {
        reportDifference(reporter, KAEventFormatter::AlarmCategory);
    }
    if (eventL.templateName() != eventR.templateName()) {
        reportDifference(reporter, KAEventFormatter::TemplateName);
    }
    if (eventL.createdDateTime() != eventR.createdDateTime()) {
        reportDifference(reporter, KAEventFormatter::CreatedTime);
    }
    if (eventL.startDateTime() != eventR.startDateTime()) {
        reportDifference(reporter, KAEventFormatter::StartTime);
    }
    if (eventL.templateAfterTime() != eventR.templateAfterTime()) {
        reportDifference(reporter, KAEventFormatter::TemplateAfterTime);
    }
    if (*eventL.recurrence() != *eventR.recurrence()) {
        reportDifference(reporter, KAEventFormatter::Recurrence);
    }
    if (eventL.mainDateTime(true) != eventR.mainDateTime(true)) {
        reportDifference(reporter, KAEventFormatter::NextRecurrence);
    }
    if (eventL.repetition() != eventR.repetition()) {
        reportDifference(reporter, KAEventFormatter::SubRepetition);
    }
    if (eventL.repetition().interval() != eventR.repetition().interval()) {
        reportDifference(reporter, KAEventFormatter::RepeatInterval);
    }
    if (eventL.repetition().count() != eventR.repetition().count()) {
        reportDifference(reporter, KAEventFormatter::RepeatCount);
    }
    if (eventL.nextRepetition() != eventR.nextRepetition()) {
        reportDifference(reporter, KAEventFormatter::NextRepetition);
    }
    if (eventL.holidaysExcluded() != eventR.holidaysExcluded()) {
        reportDifference(reporter, KAEventFormatter::HolidaysExcluded);
    }
    if (eventL.workTimeOnly() != eventR.workTimeOnly()) {
        reportDifference(reporter, KAEventFormatter::WorkTimeOnly);
    }
    if (eventL.lateCancel() != eventR.lateCancel()) {
        reportDifference(reporter, KAEventFormatter::LateCancel);
    }
    if (eventL.autoClose() != eventR.autoClose()) {
        reportDifference(reporter, KAEventFormatter::AutoClose);
    }
    if (eventL.copyToKOrganizer() != eventR.copyToKOrganizer()) {
        reportDifference(reporter, KAEventFormatter::CopyKOrganizer);
    }
    if (eventL.enabled() != eventR.enabled()) {
        reportDifference(reporter, KAEventFormatter::Enabled);
    }
    if (eventL.isReadOnly() != eventR.isReadOnly()) {
        reportDifference(reporter, KAEventFormatter::ReadOnly);
    }
    if (eventL.toBeArchived() != eventR.toBeArchived()) {
        reportDifference(reporter, KAEventFormatter::Archive);
    }
    if (eventL.customProperties() != eventR.customProperties()) {
        reportDifference(reporter, KAEventFormatter::CustomProperties);
    }
    if (eventL.message() != eventR.message()) {
        reportDifference(reporter, KAEventFormatter::MessageText);
    }
    if (eventL.fileName() != eventR.fileName()) {
        reportDifference(reporter, KAEventFormatter::MessageFile);
    }
    if (eventL.fgColour() != eventR.fgColour()) {
        reportDifference(reporter, KAEventFormatter::FgColour);
    }
    if (eventL.bgColour() != eventR.bgColour()) {
        reportDifference(reporter, KAEventFormatter::BgColour);
    }
    if (eventL.font() != eventR.font()) {
        reportDifference(reporter, KAEventFormatter::Font);
    }
    if (eventL.preAction() != eventR.preAction()) {
        reportDifference(reporter, KAEventFormatter::PreAction);
    }
    if ((eventL.extraActionOptions() & KAEvent::CancelOnPreActError) != (eventR.extraActionOptions() & KAEvent::CancelOnPreActError)) {
        reportDifference(reporter, KAEventFormatter::PreActionCancel);
    }
    if ((eventL.extraActionOptions() & KAEvent::DontShowPreActError) != (eventR.extraActionOptions() & KAEvent::DontShowPreActError)) {
        reportDifference(reporter, KAEventFormatter::PreActionNoError);
    }
    if (eventL.postAction() != eventR.postAction()) {
        reportDifference(reporter, KAEventFormatter::PostAction);
    }
    if (eventL.confirmAck() != eventR.confirmAck()) {
        reportDifference(reporter, KAEventFormatter::ConfirmAck);
    }
    if (eventL.akonadiItemId() != eventR.akonadiItemId()) {
        reportDifference(reporter, KAEventFormatter::AkonadiItem);
    }
    if (eventL.beep() != eventR.beep()
        || eventL.speak() != eventR.speak()
        || eventL.audioFile() != eventR.audioFile()) {
        reportDifference(reporter, KAEventFormatter::Sound);
    }
    if (eventL.repeatSound() != eventR.repeatSound()) {
        reportDifference(reporter, KAEventFormatter::SoundRepeat);
    }
    if (eventL.soundVolume() != eventR.soundVolume()) {
        reportDifference(reporter, KAEventFormatter::SoundVolume);
    }
    if (eventL.fadeVolume() != eventR.fadeVolume()) {
        reportDifference(reporter, KAEventFormatter::SoundFadeVolume);
    }
    if (eventL.fadeSeconds() != eventR.fadeSeconds()) {
        reportDifference(reporter, KAEventFormatter::SoundFadeTime);
    }
    if (eventL.reminderMinutes() != eventR.reminderMinutes()) {
        reportDifference(reporter, KAEventFormatter::Reminder);
    }
    if (eventL.reminderOnceOnly() != eventR.reminderOnceOnly()) {
        reportDifference(reporter, KAEventFormatter::ReminderOnce);
    }
    if (eventL.deferred() != eventR.deferred()) {
        reportDifference(reporter, KAEventFormatter::DeferralType);
    }
    if (eventL.deferDateTime() != eventR.deferDateTime()) {
        reportDifference(reporter, KAEventFormatter::DeferralTime);
    }
    if (eventL.deferDefaultMinutes() != eventR.deferDefaultMinutes()) {
        reportDifference(reporter, KAEventFormatter::DeferDefault);
    }
    if (eventL.deferDefaultDateOnly() != eventR.deferDefaultDateOnly()) {
        reportDifference(reporter, KAEventFormatter::DeferDefaultDate);
    }
    if (eventL.command() != eventR.command()) {
        reportDifference(reporter, KAEventFormatter::Command);
    }
    if (eventL.logFile() != eventR.logFile()) {
        reportDifference(reporter, KAEventFormatter::LogFile);
    }
    if (eventL.commandXterm() != eventR.commandXterm()) {
        reportDifference(reporter, KAEventFormatter::CommandXTerm);
    }
    if (eventL.emailSubject() != eventR.emailSubject()) {
        reportDifference(reporter, KAEventFormatter::EmailSubject);
    }
    if (eventL.emailFromId() != eventR.emailFromId()) {
        reportDifference(reporter, KAEventFormatter::EmailFromId);
    }
    if (eventL.emailAddresses() != eventR.emailAddresses()) {
        reportDifference(reporter, KAEventFormatter::EmailTo);
    }
    if (eventL.emailBcc() != eventR.emailBcc()) {
        reportDifference(reporter, KAEventFormatter::EmailBcc);
    }
    if (eventL.emailMessage() != eventR.emailMessage()) {
        reportDifference(reporter, KAEventFormatter::EmailBody);
    }
    if (eventL.emailAttachments() != eventR.emailAttachments()) {
        reportDifference(reporter, KAEventFormatter::EmailAttachments);
    }

    QLocale locale;
    reporter->addProperty(AbstractDifferencesReporter::ConflictMode, i18nc("@label", "Item revision"),
                          locale.toString(left.revision()), locale.toString(right.revision()));
}

void SerializerPluginKAlarm::reportDifference(AbstractDifferencesReporter *reporter, KAEventFormatter::Parameter id)
{
    if (mValueL.isApplicable(id) || mValueR.isApplicable(id)) {
        reporter->addProperty(AbstractDifferencesReporter::ConflictMode, KAEventFormatter::label(id), mValueL.value(id), mValueR.value(id));
    }
}

QString SerializerPluginKAlarm::extractGid(const Item &item) const
{
    return item.hasPayload<KAEvent>() ? item.payload<KAEvent>().id() : QString();
}
