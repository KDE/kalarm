/*
   This file is part of kalarmcal library, which provides access to KAlarm
   calendar data.

   SPDX-FileCopyrightText: 2018-2023 David Jarvie <djarvie@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kaeventtest.h"

#include "kaevent.h"
#include "holidays.h"
using namespace KAlarmCal;

#include <KCalendarCore/Event>
#include <KCalendarCore/Alarm>
using namespace KCalendarCore;

#include <QTest>

QTEST_GUILESS_MAIN(KAEventTest)

namespace
{
const QString SC = QStringLiteral(";");
}

//////////////////////////////////////////////////////
// Constructors and basic property information methods
//////////////////////////////////////////////////////

void KAEventTest::constructors()
{
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));
    const QString name(QStringLiteral("name"));
    const QString text(QStringLiteral("message"));
    const QColor fgColour(130, 110, 240);
    const QColor bgColour(20, 70, 140);
    const QFont  font(QStringLiteral("Helvetica"), 10, QFont::Bold, true);
    const KAEvent::Flags flags(KAEvent::CONFIRM_ACK | KAEvent::AUTO_CLOSE);
    {
        // Display alarm
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QCOMPARE(event.message(), text);
        QCOMPARE(event.displayMessage(), text);
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.bgColour(), bgColour);
        QCOMPARE(event.fgColour(), fgColour);
        QCOMPARE(event.font(), font);
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::Action::Display);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Message);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Display file alarm
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::File, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QCOMPARE(event.fileName(), text);
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.bgColour(), bgColour);
        QCOMPARE(event.fgColour(), fgColour);
        QCOMPARE(event.font(), font);
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::Action::Display);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::File);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Command alarm
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Command, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QCOMPARE(event.command(), text);
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::Action::Command);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Command);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Email alarm
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Email, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QCOMPARE(event.message(), text);
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QCOMPARE(event.emailMessage(), text);
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::Action::Email);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Email);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Audio alarm
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Audio, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QVERIFY(event.cleanText().isEmpty());
        QCOMPARE(event.name(), name);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QCOMPARE(event.audioFile(), text);
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::Action::Audio);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Audio);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }

    // This tests the basic KCalendarCore::Event properties.
    // Custom properties are tested later.
    const QDateTime createdDt(QDate(2009,4,13), QTime(11,14,0), QTimeZone("UTC"));
    const QString uid(QStringLiteral("fd45-77398a2"));
    {
        // Display alarm
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(name);
        kcalevent->setDescription(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setDisplayAlarm(text);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QCOMPARE(event.message(), text);
        QCOMPARE(event.displayMessage(), text);
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::Action::Display);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Message);
        QVERIFY(!event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
    {
        // Display file alarm
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(name);
        kcalevent->setDescription(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setDisplayAlarm(text);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("FILE"));

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QCOMPARE(event.fileName(), text);
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::Action::Display);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::File);
        QVERIFY(!event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
    {
        // Command alarm
        const QString args(QStringLiteral("-x anargument"));
        const QString cmdline(text + QStringLiteral(" ") + args);
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(name);
        kcalevent->setDescription(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(text, args);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), cmdline);
        QCOMPARE(event.name(), name);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QCOMPARE(event.command(), cmdline);
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::Action::Command);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Command);
        QVERIFY(!event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
    {
        // Email alarm
        const QString subject(QStringLiteral("Subject 1"));
        const Person addressee{QStringLiteral("Fred"), QStringLiteral("fred@freddy.com")};
        const Person::List addressees{addressee};
        const QStringList attachments{QStringLiteral("/tmp/xyz"), QStringLiteral("/home/fred/attch.p")};
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(name);
        kcalevent->setDescription(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setEmailAlarm(subject, text, addressees, attachments);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QCOMPARE(event.message(), text);
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QCOMPARE(event.emailMessage(), text);
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::Action::Email);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Email);
        QVERIFY(!event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
        QCOMPARE(event.emailAddresses(), QStringList{QStringLiteral("Fred <fred@freddy.com>")});
        QCOMPARE(event.emailSubject(), subject);
        QCOMPARE(event.emailAttachments(), attachments);
    }
    {
        // Audio alarm
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(name);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(true);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setAudioAlarm(text);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.name(), name);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QCOMPARE(event.audioFile(), text);
        QCOMPARE(event.actionTypes(), KAEvent::Action::Audio);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Audio);
        QVERIFY(event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
}

void KAEventTest::flags()
{
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));
    const QString name(QStringLiteral("name"));
    const QString text(QStringLiteral("message"));
    const QColor fgColour(130, 110, 240);
    const QColor bgColour(20, 70, 140);
    const QFont  font(QStringLiteral("Helvetica"), 10, QFont::Bold, true);
    {
        const KAEvent::Flags flags(KAEvent::BEEP | KAEvent::DEFAULT_FONT);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(event.beep());
        QVERIFY(event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::REPEAT_AT_LOGIN | KAEvent::DISABLED);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(event.repeatAtLogin());
        QVERIFY(!event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KADateTime dtDateOnly(QDate(2010,5,13), QTimeZone("Europe/London"));
        const KAEvent::Flags flags(KAEvent::REPEAT_AT_LOGIN | KAEvent::DISABLED);
        KAEvent event(dtDateOnly, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QVERIFY(event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags | KAEvent::ANY_TIME);
        QVERIFY(event.repeatAtLogin());
        QVERIFY(!event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::REPEAT_AT_LOGIN | KAEvent::DISABLED);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags | KAEvent::ANY_TIME);
        QVERIFY(event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags | KAEvent::ANY_TIME);
        QVERIFY(event.repeatAtLogin());
        QVERIFY(!event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::CONFIRM_ACK | KAEvent::SPEAK | KAEvent::EXCL_HOLIDAYS);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(event.confirmAck());
        QVERIFY(event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        static Holidays holidays;
        KAEvent::setHolidays(holidays);
        const KAEvent::Flags flags(KAEvent::AUTO_CLOSE | KAEvent::EXCL_HOLIDAYS | KAEvent::REPEAT_SOUND);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(event.autoClose());
        QVERIFY(event.holidaysExcluded());
        QVERIFY(event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::COPY_KORGANIZER | KAEvent::WORK_TIME_ONLY);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(event.copyToKOrganizer());
        QVERIFY(event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::SCRIPT | KAEvent::EXEC_IN_XTERM);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Command, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(event.commandScript());
        QVERIFY(event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::DISPLAY_COMMAND | KAEvent::REMINDER_ONCE);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Command, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(event.commandDisplay());
        QVERIFY(event.reminderOnceOnly());
        QVERIFY(!event.emailBcc());
    }
    {
        const KAEvent::Flags flags(KAEvent::EMAIL_BCC);
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Email, 3, flags);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(!event.confirmAck());
        QVERIFY(!event.speak());
        QVERIFY(!event.autoClose());
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(!event.repeatSound());
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(!event.workTimeOnly());
        QVERIFY(!event.commandScript());
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(!event.reminderOnceOnly());
        QVERIFY(event.emailBcc());
    }
}

namespace
{

Event::Ptr createKcalEvent(const QDateTime& start, const QDateTime& created, Alarm::Ptr& kcalalarm, Alarm::Type type = Alarm::Display)
{
    Event::Ptr kcalevent(new Event);
    kcalevent->setCreated(created);
    kcalevent->setDtStart(start);
    kcalalarm = kcalevent->newAlarm();
    kcalalarm->setType(type);
    switch (type) {
        case Alarm::Display:
            kcalalarm->setText(QStringLiteral("message"));
            break;
        case Alarm::Procedure:
            kcalalarm->setProgramFile(QStringLiteral("/tmp/cmd.sh"));
            kcalalarm->setProgramArguments(QStringLiteral("-a PERM"));
            break;
        case Alarm::Email: {
            Person addr(QStringLiteral("Cliff Edge"), QStringLiteral("cliff@edge.com"));
            kcalalarm->setMailSubject(QStringLiteral("Subject"));
            kcalalarm->setMailText(QStringLiteral("message"));
            kcalalarm->setMailAddress(addr);
            kcalalarm->setMailAttachment(QStringLiteral("/tmp/secret.txt"));
            break;
        }
        case Alarm::Audio:
            kcalalarm->setAudioFile(QStringLiteral("/tmp/sample.ogg"));
            break;
        default:
            break;
    }
    return kcalevent;
}

Event::Ptr createKcalEvent(const QDateTime& start, const QDateTime& created, Alarm::Type type = Alarm::Display)
{
    Alarm::Ptr kcalalarm;
    return createKcalEvent(start, created, kcalalarm, type);
}

Alarm::Ptr copyKcalAlarm(Event::Ptr& kcalevent, Alarm::Ptr& kcalalarm)
{
    Alarm::Ptr newAlarm = kcalevent->newAlarm();
    *newAlarm.data() = *kcalalarm.data();
    return newAlarm;
}

}

void KAEventTest::fromKCalEvent()
{
    // Check KCalendarCore::Event custom properties.
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));
    const QDateTime createdDt(QDate(2009,4,13), QTime(11,14,0), QTimeZone("UTC"));

    // Event category, UID, revision, start time, created time
    {
        const QString uid = QStringLiteral("fa74ec931");
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("ACTIVE"));
        kcalevent->setUid(uid);
        kcalevent->setRevision(273);
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::ACTIVE);
        QCOMPARE(event.startDateTime().kDateTime(), dt);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 273);
    }
    {
        // Start time using LocalZone
        const KADateTime dtLocal(dt.date(), dt.time(), KADateTime::LocalZone);
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("ACTIVE"));
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("LOCAL"));
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::ACTIVE);
        QCOMPARE(event.startDateTime().kDateTime(), dtLocal);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("TEMPLATE"));
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::TEMPLATE);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("ARCHIVED"));
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::ARCHIVED);
    }
    {
        bool showEdit = false;
        bool showDefer = false;
        ResourceId collectionId = -1;
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        Alarm::Ptr kcalalarmDisp = copyKcalAlarm(kcalevent, kcalalarm);
        {
            kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING"));
            kcalalarmDisp->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING"));
            KAEvent event(kcalevent);
            QCOMPARE(event.category(), CalEvent::DISPLAYING);
            KAEvent event2;
            event2.reinstateFromDisplaying(kcalevent, collectionId, showEdit, showDefer);
            QCOMPARE(event2.category(), CalEvent::ACTIVE);
            QVERIFY(!event2.repeatAtLogin());
            QCOMPARE(collectionId, -1);
            QVERIFY(!showEdit);
            QVERIFY(!showDefer);
        }
        {
            kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING;7;EDIT"));
            KAEvent event(kcalevent);
            QCOMPARE(event.category(), CalEvent::DISPLAYING);
            KAEvent event2;
            event2.reinstateFromDisplaying(kcalevent, collectionId, showEdit, showDefer);
            QCOMPARE(event2.category(), CalEvent::ACTIVE);
            QVERIFY(!event2.deferred());
            QCOMPARE(collectionId, 7);
            QVERIFY(showEdit);
            QVERIFY(!showDefer);
        }
        {
            kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING;-1;DEFER"));
            kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("DEFERRAL"));
            kcalalarmDisp->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING,DEFERRAL"));
            KAEvent event(kcalevent);
            QCOMPARE(event.category(), CalEvent::DISPLAYING);
            KAEvent event2;
            event2.reinstateFromDisplaying(kcalevent, collectionId, showEdit, showDefer);
            QCOMPARE(event2.category(), CalEvent::ACTIVE);
            QVERIFY(event2.deferred());
            QVERIFY(!event2.deferDateTime().isDateOnly());
            QCOMPARE(collectionId, -1);
            QVERIFY(!showEdit);
            QVERIFY(showDefer);
        }
        {
            kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING;634;DEFER;EDIT"));
            kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("DATE_DEFERRAL"));
            kcalalarmDisp->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING,DATE_DEFERRAL"));
            KAEvent event(kcalevent);
            QCOMPARE(event.category(), CalEvent::DISPLAYING);
            KAEvent event2;
            event2.reinstateFromDisplaying(kcalevent, collectionId, showEdit, showDefer);
            QCOMPARE(event2.category(), CalEvent::ACTIVE);
            QVERIFY(event2.deferred());
            QVERIFY(event2.deferDateTime().isDateOnly());
            QCOMPARE(collectionId, 634);
            QVERIFY(showEdit);
            QVERIFY(showDefer);
        }
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING;DEFER"));
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::DISPLAYING);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING;EDIT"));
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::DISPLAYING);
    }

    // Event flags
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("DATE"));
        KAEvent event(kcalevent);
        QVERIFY(event.startDateTime().isDateOnly());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("ACKCONF"));
        KAEvent event(kcalevent);
        QVERIFY(!event.startDateTime().isDateOnly());
        QVERIFY(event.confirmAck());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("KORG"));
        KAEvent event(kcalevent);
        QVERIFY(!event.confirmAck());
        QVERIFY(event.copyToKOrganizer());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("EXHOLIDAYS"));
        KAEvent event(kcalevent);
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(event.holidaysExcluded());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("WORKTIME"));
        KAEvent event(kcalevent);
        QVERIFY(!event.holidaysExcluded());
        QVERIFY(event.workTimeOnly());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("LATECANCEL;4"));
        KAEvent event(kcalevent);
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.lateCancel(), 4);
        QVERIFY(!event.autoClose());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("LOGIN"));
        KAEvent event(kcalevent);
        QCOMPARE(event.lateCancel(), 0);
        QVERIFY(event.repeatAtLogin(true));
        QVERIFY(!event.repeatAtLogin(false));
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("LATECLOSE;16"));
        KAEvent event(kcalevent);
        QVERIFY(!event.repeatAtLogin());
        QCOMPARE(event.lateCancel(), 16);
        QVERIFY(event.autoClose());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("KORG"));
        KAEvent event(kcalevent);
        QCOMPARE(event.lateCancel(), 0);
        QVERIFY(!event.autoClose());
        QVERIFY(event.copyToKOrganizer());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("ARCHIVE"));
        KAEvent event(kcalevent);
        QVERIFY(!event.copyToKOrganizer());
        QVERIFY(event.toBeArchived());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("DEFER;7"));
        KAEvent event(kcalevent);
        QVERIFY(!event.toBeArchived());
        QCOMPARE(event.deferDefaultMinutes(), 7);
        QVERIFY(!event.deferDefaultDateOnly());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("DEFER;6D"));
        KAEvent event(kcalevent);
        QCOMPARE(event.deferDefaultMinutes(), 6);
        QVERIFY(event.deferDefaultDateOnly());
    }
    {
        // Reminder after the event, first recurrence only
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;ONCE;27M"));
        kcalalarm = copyKcalAlarm(kcalevent, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("REMINDER"));
        kcalalarm->setStartOffset(-27*60);
        KAEvent event(kcalevent);
        QCOMPARE(event.deferDefaultMinutes(), 0);
        QVERIFY(!event.deferDefaultDateOnly());
        QVERIFY(event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), -27);
    }
    {
        // Reminder before the event
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;-27H"));
        kcalalarm = copyKcalAlarm(kcalevent, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("REMINDER"));
        kcalalarm->setStartOffset(-27*3600);
        KAEvent event(kcalevent);
        QVERIFY(event.reminderActive());
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), 27*60);
    }
    {
        // Reminder after the event
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;27D"));
        kcalalarm = copyKcalAlarm(kcalevent, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("REMINDER"));
        kcalalarm->setStartOffset(Duration(27, Duration::Days));
        KAEvent event(kcalevent);
        QVERIFY(event.reminderActive());
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), -27*60*24);
    }
    {
        // Reminder before the event
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;10M"));
        kcalalarm = copyKcalAlarm(kcalevent, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("REMINDER"));
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("HIDE"));
        kcalalarm->setStartOffset(10*60);
        KAEvent event(kcalevent);
        QVERIFY(!event.reminderActive());
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), -10);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("BCC"));
        KAEvent event(kcalevent);
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), 0);
        QVERIFY(event.emailBcc());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("TMPLAFTTIME;31"));
        KAEvent event(kcalevent);
        QVERIFY(!event.emailBcc());
        QCOMPARE(event.templateAfterTime(), 31);
        QCOMPARE(event.emailId(), KAEvent::EmailId(-1));
    }
    {
        // Akonadi item ID, with alarm message in email format
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("KMAIL;759231"));
        kcalalarm->setText(QStringLiteral("From: a@b.c\nTo: d@e.f\nDate: Sun, 01 Apr 2018 17:36:06 +0100\nSubject: About this"));
        KAEvent event(kcalevent);
        QCOMPARE(event.templateAfterTime(), -1);
        QCOMPARE(event.emailId(), KAEvent::EmailId(759231));
    }
    {
        // Akonadi item ID, with alarm message in wrong format
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("KMAIL;759231"));
        KAEvent event(kcalevent);
        QCOMPARE(event.emailId(), KAEvent::EmailId(-1));
    }

    // Alarm custom properties
    {
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("FILE"));
        kcalalarm->setStartOffset(5*60);
        KAEvent event(kcalevent);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::File);
    }
    {
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("DEFERRAL"));
        kcalalarm->setStartOffset(5*60);
        KAEvent event(kcalevent);
        QCOMPARE(event.actionSubType(), KAEvent::SubAction::Message);
        QVERIFY(event.mainExpired());
    }
    {
        QFont font(QStringLiteral("Monospace"), 8);
        font.setBold(true);
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "FONTCOLOR", QStringLiteral("#27A8F3;#94B0FF;") + font.toString());
        KAEvent event(kcalevent);
        QVERIFY(!event.mainExpired());
        QCOMPARE(event.bgColour(), QColor("#27A8F3"));
        QCOMPARE(event.fgColour(), QColor("#94B0FF"));
        QCOMPARE(event.font(), font);
    }
    {
        // Non-repeating sound
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm = kcalevent->newAlarm();
        kcalalarm->setType(Alarm::Audio);
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("SPEAK"));
        KAEvent event(kcalevent);
        QVERIFY(event.speak());
        QCOMPARE(event.repeatSoundPause(), -1);
    }
    {
        // Sound volume
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm = kcalevent->newAlarm();
        kcalalarm->setAudioAlarm(QStringLiteral("/tmp/next.ogg"));
        kcalalarm->setCustomProperty("KALARM", "VOLUME", QStringLiteral("0.7;0.3;9"));
        KAEvent event(kcalevent);
        QVERIFY(!event.speak());
        QCOMPARE(event.soundVolume(), 0.7f);
        QCOMPARE(event.fadeVolume(), 0.3f);
        QCOMPARE(event.fadeSeconds(), 9);
    }
    {
        // Sound volume
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm, Alarm::Audio);
        kcalalarm->setCustomProperty("KALARM", "VOLUME", QStringLiteral("0.7;0.3;9"));
        KAEvent event(kcalevent);
        QCOMPARE(event.soundVolume(), 0.7f);
        QCOMPARE(event.fadeVolume(), 0.3f);
        QCOMPARE(event.fadeSeconds(), 9);
    }
    {
        // Display alarm with repeating sound, without pause
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm = kcalevent->newAlarm();
        kcalalarm->setAudioAlarm(QStringLiteral("/tmp/next.ogg"));
        kcalalarm->setRepeatCount(-1);
        kcalalarm->setSnoozeTime(Duration(0));
        KAEvent event(kcalevent);
        QCOMPARE(event.repeatSoundPause(), 0);
    }
    {
        // Display alarm with repeating sound, with pause
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm = kcalevent->newAlarm();
        kcalalarm->setAudioAlarm(QStringLiteral("/tmp/next.ogg"));
        kcalalarm->setRepeatCount(-2);
        kcalalarm->setSnoozeTime(Duration(6));
        KAEvent event(kcalevent);
        QCOMPARE(event.repeatSoundPause(), 6);
    }
    {
        // Audio alarm with repeating sound, without pause
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm, Alarm::Audio);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("SOUNDREPEAT,0"));
        KAEvent event(kcalevent);
        QCOMPARE(event.repeatSoundPause(), 0);
    }
    {
        // Audio alarm with repeating sound, with pause
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm, Alarm::Audio);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("SOUNDREPEAT,4"));
        KAEvent event(kcalevent);
        QCOMPARE(event.repeatSoundPause(), 4);
    }

    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, Alarm::Procedure);
        kcalevent->setCustomProperty("KALARM", "LOG", QStringLiteral("xterm:"));
        KAEvent event(kcalevent);
        QVERIFY(event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QVERIFY(event.logFile().isEmpty());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "LOG", QStringLiteral("display:"));
        KAEvent event(kcalevent);
        QVERIFY(!event.commandXterm());
        QVERIFY(event.commandDisplay());
        QVERIFY(event.logFile().isEmpty());
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        const QString file(QStringLiteral("/tmp/file"));
        kcalevent->setCustomProperty("KALARM", "LOG", file);
        KAEvent event(kcalevent);
        QVERIFY(!event.commandXterm());
        QVERIFY(!event.commandDisplay());
        QCOMPARE(event.logFile(), file);

        QVERIFY(!event.recurs());
    }

    {
        // Test date/time event with recurrence and sub-repetition
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setHourly(3);
        {
            KAEvent event(kcalevent);
            QCOMPARE(event.repetition().interval().asSeconds(), 0);
            QCOMPARE(event.repetition().count(), 0);
            QCOMPARE(event.nextRepetition(), 0);
        }
        kcalalarm->setSnoozeTime(17*60);
        kcalalarm->setRepeatCount(5);
        kcalalarm->setCustomProperty("KALARM", "NEXTREPEAT", QStringLiteral("2"));
        {
            KAEvent event(kcalevent);
            QCOMPARE(event.repetition().interval().asSeconds(), 17*60);
            QCOMPARE(event.repetition().count(), 5);
            QCOMPARE(event.nextRepetition(), 2);
        }
    }
    {
        // Test deferred event whose main alarm has expired, with sub-repetition
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("DEFERRAL"));
        kcalevent->setCustomProperty("KALARM", "REPEAT", QStringLiteral("17:5"));
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setHourly(3);
        KAEvent event(kcalevent);
        QCOMPARE(event.recurType(), KARecurrence::MINUTELY);
        QCOMPARE(event.recurInterval(), 3*60);
        QCOMPARE(event.repetition().interval().asSeconds(), 17*60);
        QCOMPARE(event.repetition().count(), 5);
    }
    {
        // Test deferred event whose main alarm has not expired, with sub-repetition
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "REPEAT", QStringLiteral("17:5"));
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setHourly(3);
        KAEvent event(kcalevent);
        QCOMPARE(event.recurType(), KARecurrence::MINUTELY);
        QCOMPARE(event.recurInterval(), 3*60);
        QCOMPARE(event.repetition().interval().asSeconds(), 0);
        QCOMPARE(event.repetition().count(), 0);
    }

    {
        // Test date/time event with next recurrence
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setHourly(3);
        kcalevent->setCustomProperty("KALARM", "NEXTRECUR", QStringLiteral("20100514T051700"));
        KAEvent event(kcalevent);
        QVERIFY(event.recurs());
        QCOMPARE(event.recurType(), KARecurrence::MINUTELY);
        QCOMPARE(event.recurInterval(), 3*60);
        QCOMPARE(event.repetition().interval().asSeconds(), 0);
        QCOMPARE(event.repetition().count(), 0);
        QVERIFY(event.mainDateTime() > event.startDateTime());
    }
    {
        // Test date/time event with date-only next recurrence
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setHourly(3);
        kcalevent->setCustomProperty("KALARM", "NEXTRECUR", QStringLiteral("20100514"));
        KAEvent event(kcalevent);
        QVERIFY(event.recurs());
        QCOMPARE(event.recurType(), KARecurrence::MINUTELY);
        QCOMPARE(event.recurInterval(), 3*60);
        QCOMPARE(event.repetition().interval().asSeconds(), 0);
        QCOMPARE(event.repetition().count(), 0);
        QCOMPARE(event.mainDateTime(), event.startDateTime());
    }
    {
        // Test date-only event with next recurrence
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setDaily(3);
        kcalevent->setCustomProperty("KALARM", "NEXTRECUR", QStringLiteral("20100516"));
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("DATE"));
        KAEvent event(kcalevent);
        QVERIFY(event.recurs());
        QCOMPARE(event.recurType(), KARecurrence::DAILY);
        QCOMPARE(event.recurInterval(), 3);
        QCOMPARE(event.repetition().interval().asSeconds(), 0);
        QCOMPARE(event.repetition().count(), 0);
        QVERIFY(event.mainDateTime() > event.startDateTime());
    }
    {
        // Test date-only event with date/time next recurrence
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Recurrence* recurrence = kcalevent->recurrence();
        recurrence->setStartDateTime(QDateTime(QDate(2010,5,13), QTime(5,17,0), QTimeZone("Europe/London")), false);
        recurrence->setDaily(3);
        kcalevent->setCustomProperty("KALARM", "NEXTRECUR", QStringLiteral("20100516T051700"));
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("DATE"));
        KAEvent event(kcalevent);
        QVERIFY(event.recurs());
        QCOMPARE(event.recurType(), KARecurrence::DAILY);
        QCOMPARE(event.recurInterval(), 3);
        QCOMPARE(event.repetition().interval().asSeconds(), 0);
        QCOMPARE(event.repetition().count(), 0);
        QCOMPARE(event.mainDateTime(), event.startDateTime());
    }
    {
        // Pre-action alarm
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(QStringLiteral("/tmp/action.sh"), QStringLiteral("-h"));
        kcalalarm->setStartOffset(0);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("PRE"));
        KAEvent event(kcalevent);
        QCOMPARE(event.preAction(), QStringLiteral("/tmp/action.sh -h"));
        QCOMPARE(event.extraActionOptions(), 0);
    }
    {
        // Pre-action alarm
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(QStringLiteral("/tmp/action.sh"), QStringLiteral("-h"));
        kcalalarm->setStartOffset(0);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("PRE"));
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("EXECDEFER"));
        KAEvent event(kcalevent);
        QCOMPARE(event.preAction(), QStringLiteral("/tmp/action.sh -h"));
        QCOMPARE(event.extraActionOptions(), KAEvent::ExecPreActOnDeferral);
    }
    {
        // Pre-action alarm
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(QStringLiteral("/tmp/action.sh"), QStringLiteral("-h"));
        kcalalarm->setStartOffset(0);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("PRE"));
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("ERRCANCEL"));
        KAEvent event(kcalevent);
        QCOMPARE(event.preAction(), QStringLiteral("/tmp/action.sh -h"));
        QCOMPARE(event.extraActionOptions(), KAEvent::CancelOnPreActError);
    }
    {
        // Pre-action alarm
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(QStringLiteral("/tmp/action.sh"), QStringLiteral("-h"));
        kcalalarm->setStartOffset(0);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("PRE"));
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("ERRNOSHOW"));
        KAEvent event(kcalevent);
        QCOMPARE(event.preAction(), QStringLiteral("/tmp/action.sh -h"));
        QCOMPARE(event.extraActionOptions(), KAEvent::DontShowPreActError);
    }
    {
        // Pre-action alarm
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(QStringLiteral("/tmp/action.sh"), QStringLiteral("-h"));
        kcalalarm->setStartOffset(0);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("PRE"));
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("ERRNOSHOW;ERRCANCEL;EXECDEFER"));
        KAEvent event(kcalevent);
        QCOMPARE(event.preAction(), QStringLiteral("/tmp/action.sh -h"));
        QCOMPARE(event.extraActionOptions(), KAEvent::DontShowPreActError | KAEvent::CancelOnPreActError | KAEvent::ExecPreActOnDeferral);
    }
    {
        // Post-action alarm
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(QStringLiteral("/tmp/action.sh"), QStringLiteral("-h"));
        kcalalarm->setStartOffset(0);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("POST"));
        KAEvent event(kcalevent);
        QCOMPARE(event.postAction(), QStringLiteral("/tmp/action.sh -h"));
        QCOMPARE(event.extraActionOptions(), 0);
    }
    {
        // Email-from ID
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm, Alarm::Email);
        kcalalarm->setCustomProperty("KALARM", "FLAGS", QStringLiteral("EMAILID;2589"));
        KAEvent event(kcalevent);
        QCOMPARE(event.emailFromId(), 2589U);
    }
    {
        // Archived repeat-at-login
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("LOGIN"));
        KAEvent event(kcalevent);
        QVERIFY(event.repeatAtLogin(false));
    }
}

void KAEventTest::toKCalEvent()
{
    // Check KCalendarCore::Event custom properties.
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));
    const KADateTime createdDt(QDate(2009,4,13), QTime(11,14,0), QTimeZone("UTC"));
    const QString name(QStringLiteral("name"));
    const QString text = QStringLiteral("message");
    const QColor fgColour(0x82, 0x6e, 0xf0);
    const QColor bgColour(0x14, 0x46, 0x8c);
    const QFont  font(QStringLiteral("Helvetica"), 10, QFont::Bold, true);
    const QString uid = QStringLiteral("fa74ec931");

    {
        // Event category, UID, revision, start time using time zone, created time
        KAEvent event(dt, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, KAEvent::CONFIRM_ACK);
        event.setEventId(uid);
        event.incrementRevision();
        event.incrementRevision();
        event.setCategory(CalEvent::ACTIVE);
        event.setCreatedDateTime(createdDt);
        Event::Ptr kcalevent(new Event);
        QVERIFY(event.updateKCalEvent(kcalevent, KAEvent::UidAction::Set, true));
        QCOMPARE(kcalevent->uid(), uid);
        QCOMPARE(kcalevent->revision(), 2);
        QCOMPARE(kcalevent->customProperty("KALARM", "TYPE"), QStringLiteral("ACTIVE"));
        QStringList flags = kcalevent->customProperty("KALARM", "FLAGS").split(SC);
        QCOMPARE(flags.size(), 3);   // must contain LATECANCEL;3 and ACKCONF
        QCOMPARE(flags.removeAll(QStringLiteral("ACKCONF")), 1);
        QCOMPARE(flags.at(0), QStringLiteral("LATECANCEL"));
        QCOMPARE(flags.at(1), QStringLiteral("3"));
        QCOMPARE(kcalevent->dtStart(), dt.qDateTime());
        QCOMPARE(kcalevent->created(), createdDt.qDateTime());
        QCOMPARE(kcalevent->summary(), name);
        const Alarm::List kcalalarms = kcalevent->alarms();
        QCOMPARE(kcalalarms.size(), 1);
        Alarm::Ptr kcalalarm(kcalalarms[0]);
        QCOMPARE(kcalalarm->type(), Alarm::Display);
        QCOMPARE(kcalalarm->text(), text);
        QCOMPARE(kcalalarm->customProperty("KALARM", "FONTCOLOR").toUpper(), (QStringLiteral("#14468C;#826EF0;") + font.toString()).toUpper());
    }
    {
        // Start time using LocalZone
        const KADateTime dtl(QDate(2010,5,13), QTime(3, 45, 0), KADateTime::LocalZone);
        KAEvent event(dtl, name, text, bgColour, fgColour, font, KAEvent::SubAction::Message, 3, KAEvent::CONFIRM_ACK);
        event.setEventId(uid);
        event.incrementRevision();
        event.setCategory(CalEvent::ACTIVE);
        event.setCreatedDateTime(createdDt);
        Event::Ptr kcalevent(new Event);
        QVERIFY(event.updateKCalEvent(kcalevent, KAEvent::UidAction::Set, true));
        QCOMPARE(kcalevent->uid(), uid);
        QCOMPARE(kcalevent->revision(), 1);
        QCOMPARE(kcalevent->customProperty("KALARM", "TYPE"), QStringLiteral("ACTIVE"));
        QStringList flags = kcalevent->customProperty("KALARM", "FLAGS").split(SC);
        QCOMPARE(flags.size(), 4);   // must contain LOCAL, LATECANCEL;3 and ACKCONF
        QVERIFY(flags.contains(QLatin1StringView("LOCAL")));
        const QTimeZone sysTz = QTimeZone::systemTimeZone();
        const QDateTime dtCurrentTz(dtl.date(), dtl.time(), sysTz);
qDebug()<<"dtStart:"<<kcalevent->dtStart()<<", tz:"<<kcalevent->dtStart().timeZone();
qDebug()<<"sysTz.isValid:"<<sysTz.isValid()<<", dtCurrentTz:"<<dtCurrentTz<<", dtCurrentTz.timeZone:"<<dtCurrentTz.timeZone();
qDebug()<<"dtl:"<<dtl.qDateTime();
qDebug()<<"sysTz:"<<sysTz<<", dtl:"<<dtl.qTimeZone()<<", dtl.qdt:"<<dtl.qDateTime().timeZone();
        QCOMPARE(kcalevent->dtStart(), dtl.qDateTime());
        if (sysTz.isValid())
            QCOMPARE(kcalevent->dtStart().toTimeZone(sysTz), QDateTime(dtl.date(), dtl.time(), sysTz));
        QCOMPARE(kcalevent->created(), createdDt.qDateTime());
    }
}

void KAEventTest::setNextOccurrence()
{
    // Test setNextOccurrence() going from before to after a shift from daylight savings
    // to standard time, for a daily recurrence at a clock time which occurs twice (once
    // before the time shift, and once an hour later when the clock time repeats).
    const KADateTime dt(QDate(2005, 10, 29), QTime(1, 30, 0), QTimeZone("Europe/London"));
    KAEvent event(dt, QStringLiteral("name"), QStringLiteral("text"), Qt::black, Qt::white, QFont(), KAEvent::SubAction::Message, 0, KAEvent::DEFAULT_FONT);
    event.setRecurDaily(1, QBitArray(7, true), -1, QDate());
    KAEvent::OccurType type = event.setNextOccurrence(dt);
    const DateTime next1 = event.mainDateTime();
    QCOMPARE(type, KAEvent::OccurType::RecurDateTime);
    QCOMPARE(next1.date(), QDate(2005, 10, 30));
    QCOMPARE(next1.effectiveTime(), QTime(1, 30, 0));
    KAEvent eventUTC = event;

    const KADateTime dt1(QDate(2005, 10, 30), QTime(1, 30, 0), QTimeZone("Europe/London"));
    type = event.setNextOccurrence(next1.kDateTime());
    const DateTime next2 = event.mainDateTime();
    QCOMPARE(type, KAEvent::OccurType::RecurDateTime);
    QCOMPARE(next2.date(), QDate(2005, 10, 31));
    QCOMPARE(next2.effectiveTime(), QTime(1, 30, 0));

    const KADateTime dt2(QDate(2005, 10, 30), QTime(1, 30, 0), KADateTime::UTC);
    type = eventUTC.setNextOccurrence(dt2);
    const DateTime next3 = eventUTC.mainDateTime();
    QCOMPARE(type, KAEvent::OccurType::RecurDateTime);
    QCOMPARE(next3.date(), QDate(2005, 10, 31));
    QVERIFY(!next3.isSecondOccurrence());
}

#include "moc_kaeventtest.cpp"

// vim: et sw=4:
