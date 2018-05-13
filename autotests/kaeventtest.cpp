/*
   This file is part of kalarmcal library, which provides access to KAlarm
   calendar data.

   Copyright (c) 2018 David Jarvie <djarvie@kde.org>

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published by
   the Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
   License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kaeventtest.h"

#include "kaevent.h"
using namespace KAlarmCal;

#include <kcalcore/event.h>
#include <kcalcore/alarm.h>
using namespace KCalCore;
#include <AkonadiCore/collection.h>

#include <KHolidays/HolidayRegion>
#include <kconfiggroup.h>

#include <QtTest>

QTEST_MAIN(KAEventTest)

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
    const QString text(QStringLiteral("message"));
    const QColor fgColour(130, 110, 240);
    const QColor bgColour(20, 70, 140);
    const QFont  font(QStringLiteral("Helvetica"), 10, QFont::Bold, true);
    const KAEvent::Flags flags(KAEvent::CONFIRM_ACK | KAEvent::AUTO_CLOSE);
    {
        // Display alarm
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
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
        QCOMPARE(event.actionTypes(), KAEvent::ACT_DISPLAY);
        QCOMPARE(event.actionSubType(), KAEvent::MESSAGE);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Display file alarm
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::FILE, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
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
        QCOMPARE(event.actionTypes(), KAEvent::ACT_DISPLAY);
        QCOMPARE(event.actionSubType(), KAEvent::FILE);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Command alarm
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::COMMAND, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QCOMPARE(event.command(), text);
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::ACT_COMMAND);
        QCOMPARE(event.actionSubType(), KAEvent::COMMAND);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Email alarm
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::EMAIL, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.message(), text);
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QCOMPARE(event.emailMessage(), text);
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::ACT_EMAIL);
        QCOMPARE(event.actionSubType(), KAEvent::EMAIL);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }
    {
        // Audio alarm
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::AUDIO, 3, flags);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QVERIFY(event.cleanText().isEmpty());
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QCOMPARE(event.audioFile(), text);
        QCOMPARE(event.flags(), flags);
        QCOMPARE(event.actionTypes(), KAEvent::ACT_AUDIO);
        QCOMPARE(event.actionSubType(), KAEvent::AUDIO);
        QCOMPARE(event.lateCancel(), 3);
        QVERIFY(!event.isReadOnly());
    }

    // This tests the basic KCalCore::Event properties.
    // Custom properties are tested later.
    const QDateTime createdDt(QDate(2009,4,13), QTime(11,14,0), QTimeZone("UTC"));
    const QString uid(QStringLiteral("fd45-77398a2"));
    {
        // Display alarm
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setDisplayAlarm(text);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.message(), text);
        QCOMPARE(event.displayMessage(), text);
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::ACT_DISPLAY);
        QCOMPARE(event.actionSubType(), KAEvent::MESSAGE);
        QVERIFY(!event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
    {
        // Display file alarm
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(text);
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
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QCOMPARE(event.fileName(), text);
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::ACT_DISPLAY);
        QCOMPARE(event.actionSubType(), KAEvent::FILE);
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
        kcalevent->setSummary(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setProcedureAlarm(text, args);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), cmdline);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QCOMPARE(event.command(), cmdline);
        QVERIFY(event.emailMessage().isEmpty());
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::ACT_COMMAND);
        QCOMPARE(event.actionSubType(), KAEvent::COMMAND);
        QVERIFY(!event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
    {
        // Email alarm
        const QString subject(QStringLiteral("Subject 1"));
        const Person::Ptr addressee(new Person{QStringLiteral("Fred"), QStringLiteral("fred@freddy.com")});
        const Person::List addressees{addressee};
        const QStringList attachments{QStringLiteral("/tmp/xyz"), QStringLiteral("/home/fred/attch.p")};
        Event::Ptr kcalevent(new Event);
        kcalevent->setCreated(createdDt);
        kcalevent->setDtStart(dt.qDateTime());
        kcalevent->setSummary(text);
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(false);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setEmailAlarm(subject, text, addressees, attachments);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QCOMPARE(event.message(), text);
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QCOMPARE(event.emailMessage(), text);
        QVERIFY(event.audioFile().isEmpty());
        QCOMPARE(event.actionTypes(), KAEvent::ACT_EMAIL);
        QCOMPARE(event.actionSubType(), KAEvent::EMAIL);
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
        kcalevent->setUid(uid);
        kcalevent->setRevision(12);
        kcalevent->setReadOnly(true);
        Alarm::Ptr kcalalarm = kcalevent->newAlarm();
        kcalalarm->setAudioAlarm(text);

        KAEvent event(kcalevent);
        QCOMPARE(event.createdDateTime().qDateTime(), createdDt);
        QCOMPARE(event.startDateTime(), DateTime(dt));
        QCOMPARE(event.cleanText(), text);
        QVERIFY(event.message().isEmpty());
        QVERIFY(event.displayMessage().isEmpty());
        QVERIFY(event.fileName().isEmpty());
        QVERIFY(event.command().isEmpty());
        QVERIFY(event.emailMessage().isEmpty());
        QCOMPARE(event.audioFile(), text);
        QCOMPARE(event.actionTypes(), KAEvent::ACT_AUDIO);
        QCOMPARE(event.actionSubType(), KAEvent::AUDIO);
        QVERIFY(event.isReadOnly());
        QCOMPARE(event.id(), uid);
        QCOMPARE(event.revision(), 12);
    }
}

void KAEventTest::flags()
{
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));
    const QString text(QStringLiteral("message"));
    const QColor fgColour(130, 110, 240);
    const QColor bgColour(20, 70, 140);
    const QFont  font(QStringLiteral("Helvetica"), 10, QFont::Bold, true);
    {
        const KAEvent::Flags flags(KAEvent::BEEP | KAEvent::DEFAULT_FONT);
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags);
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
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags);
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
        KAEvent event(dtDateOnly, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags);
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
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags | KAEvent::ANY_TIME);
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
        const KAEvent::Flags flags(KAEvent::CONFIRM_ACK | KAEvent::SPEAK);
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags | KAEvent::EXCL_HOLIDAYS);
        QVERIFY(!event.startDateTime().isDateOnly());
        QCOMPARE(event.flags(), flags);
        QVERIFY(!event.repeatAtLogin());
        QVERIFY(event.enabled());
        QVERIFY(!event.beep());
        QVERIFY(!event.useDefaultFont());
        QVERIFY(event.confirmAck());
        QVERIFY(event.speak());
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
        KAEvent::setHolidays(KHolidays::HolidayRegion());
        const KAEvent::Flags flags(KAEvent::AUTO_CLOSE | KAEvent::EXCL_HOLIDAYS | KAEvent::REPEAT_SOUND);
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags);
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
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::MESSAGE, 3, flags);
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
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::COMMAND, 3, flags);
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
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::COMMAND, 3, flags);
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
        KAEvent event(dt, text, bgColour, fgColour, font, KAEvent::EMAIL, 3, flags);
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
            Person::Ptr addr(new Person(QStringLiteral("Cliff Edge"), QStringLiteral("cliff@edge.com")));
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

}

void KAEventTest::kcalevent()
{
    // Check KCalCore::Event custom properties.
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));
    const QDateTime createdDt(QDate(2009,4,13), QTime(11,14,0), QTimeZone("UTC"));

    // Event category
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("ACTIVE"));
        KAEvent event(kcalevent);
        QCOMPARE(event.category(), CalEvent::ACTIVE);
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
        Akonadi::Collection::Id collectionId = -1;
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalevent->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING"));
        {
            kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("DISPLAYING"));
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
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;ONCE;27M"));
        KAEvent event(kcalevent);
        QCOMPARE(event.deferDefaultMinutes(), 0);
        QVERIFY(!event.deferDefaultDateOnly());
        QVERIFY(event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), -27);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;-27H"));
        KAEvent event(kcalevent);
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), 27*60);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("REMINDER;27D"));
        KAEvent event(kcalevent);
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), -27*60*24);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("BCC"));
        KAEvent event(kcalevent);
        QVERIFY(!event.reminderOnceOnly());
        QCOMPARE(event.reminderMinutes(), 0);
        QVERIFY(event.emailBcc());
        QCOMPARE(event.templateAfterTime(), -1);
    }
    {
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "FLAGS", QStringLiteral("TMPLAFTTIME;31"));
        KAEvent event(kcalevent);
        QVERIFY(!event.emailBcc());
        QCOMPARE(event.templateAfterTime(), 31);
    }
    //TODO: "FLAGS", "KMAIL"

    // Alarm custom properties
    {
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("FILE"));
        kcalalarm->setStartOffset(5*60);
        KAEvent event(kcalevent);
        QCOMPARE(event.actionSubType(), KAEvent::FILE);
    }
    {
        Alarm::Ptr kcalalarm;
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt, kcalalarm);
        kcalalarm->setCustomProperty("KALARM", "TYPE", QStringLiteral("DEFERRAL"));
        kcalalarm->setStartOffset(5*60);
        KAEvent event(kcalevent);
        QCOMPARE(event.actionSubType(), KAEvent::MESSAGE);
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
#warning REPEAT is only for main alarm expired
        // Test date/time event with sub-repetition but no recurrence
        Event::Ptr kcalevent = createKcalEvent(dt.qDateTime(), createdDt);
        kcalevent->setCustomProperty("KALARM", "REPEAT", QStringLiteral("17:5"));
        KAEvent event(kcalevent);
        QCOMPARE(event.recurType(), KARecurrence::MINUTELY);
        QCOMPARE(event.recurInterval(), 17);
        QCOMPARE(event.recurrence()->duration(), 6);
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
        QCOMPARE(event.emailFromId(), 2589);
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
// HIDDEN_REMINDER flag
    const KADateTime dt(QDate(2010,5,13), QTime(3, 45, 0), QTimeZone("Europe/London"));

// vim: et sw=4:
