/*
 *  kaevent.h  -  represents calendar events
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#ifndef KAEVENT_H
#define KAEVENT_H

#include "kaeventdata.h"
#include "preferences.h"

class AlarmResource;

/** KAEvent corresponds to a KCal::Event instance */
class KAEvent
{
    public:
        typedef QList<KAEvent*> List;
        enum            // flags for use in DCOP calls, etc.
        {
            BEEP            = KAEventData::BEEP,            // sound audible beep when alarm is displayed
            REPEAT_AT_LOGIN = KAEventData::REPEAT_AT_LOGIN, // repeat alarm at every login
            ANY_TIME        = KAEventData::ANY_TIME,        // only a date is specified for the alarm, not a time
            CONFIRM_ACK     = KAEventData::CONFIRM_ACK,     // closing the alarm message window requires confirmation prompt
            EMAIL_BCC       = KAEventData::EMAIL_BCC,       // blind copy the email to the user
            DEFAULT_FONT    = KAEventData::DEFAULT_FONT,    // use default alarm message font
            REPEAT_SOUND    = KAEventData::REPEAT_SOUND,    // repeat sound file while alarm is displayed
            DISABLED        = KAEventData::DISABLED,        // alarm is currently disabled
            AUTO_CLOSE      = KAEventData::AUTO_CLOSE,      // auto-close alarm window after late-cancel period
            SCRIPT          = KAEventData::SCRIPT,          // command is a script, not a shell command line
            EXEC_IN_XTERM   = KAEventData::EXEC_IN_XTERM,   // execute command in terminal window
            SPEAK           = KAEventData::SPEAK,           // speak the message when the alarm is displayed
            COPY_KORGANIZER = KAEventData::COPY_KORGANIZER, // KOrganizer should hold a copy of the event
            EXCL_HOLIDAYS   = KAEventData::EXCL_HOLIDAYS,   // don't trigger alarm on holidays
            WORK_TIME_ONLY  = KAEventData::WORK_TIME_ONLY,  // trigger alarm only during working hours
            DISPLAY_COMMAND = KAEventData::DISPLAY_COMMAND  // display command output in alarm window
        };
        enum TriggerType    // alarm trigger type
        {
            ALL_TRIGGER,       // next trigger, including reminders, ignoring working hours & holidays
            MAIN_TRIGGER,      // next trigger, excluding reminders, ignoring working hours & holidays
            WORK_TRIGGER,      // next main working time trigger, excluding reminders
            ALL_WORK_TRIGGER,  // next actual working time trigger, including reminders
            DISPLAY_TRIGGER    // next trigger time for display purposes (i.e. excluding reminders)
        };
        enum CmdErrType     // command execution error type for last time the alarm was triggered
        {
            CMD_NO_ERROR   = 0,      // no error
            CMD_ERROR      = 0x01,   // command alarm execution failed
            CMD_ERROR_PRE  = 0x02,   // pre-alarm command execution failed
            CMD_ERROR_POST = 0x04,   // post-alarm command execution failed
            CMD_ERROR_PRE_POST = CMD_ERROR_PRE | CMD_ERROR_POST
        };

        KAEvent();
        KAEvent(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg,
                const QFont& f, KAEventData::Action, int lateCancel, int flags, bool changesPending = false);
        explicit KAEvent(const KCal::Event*);
        void               set(const KCal::Event*);
        void               set(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg, const QFont&, KAEventData::Action, int lateCancel, int flags, bool changesPending = false);
        void               setEmail(uint from, const EmailAddressList& addrs, const QString& subject, const QStringList& attachments)
                                                                            { d->mEventData->setEmail(from, addrs, subject, attachments); }
        void               setResource(AlarmResource* r)                    { d->mResource = r; }
        void               setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds)
                                                                            { d->mEventData->setAudioFile(filename, volume, fadeVolume, fadeSeconds); }
        void               setTemplate(const QString& name, int afterTime = -1) { d->mEventData->setTemplate(name, afterTime); }
        void               setActions(const QString& pre, const QString& post, bool cancelOnError)
                                                                            { d->mEventData->setActions(pre, post, cancelOnError); }
        KAEventData::OccurType setNextOccurrence(const KDateTime& preDateTime)  { return d->mEventData->setNextOccurrence(preDateTime, Preferences::startOfDay()); }
        void               setFirstRecurrence()                             { d->mEventData->setFirstRecurrence(Preferences::startOfDay()); }
        void               setCategory(KCalEvent::Status s)                 { d->mEventData->setCategory(s); }
        void               setUid(KCalEvent::Status s)                      { d->mEventData->setUid(s); }
        void               setEventId(const QString& id)                    { d->mEventData->setEventId(id); }
        void               setTime(const KDateTime& dt)                     { d->mEventData->setTime(dt); }
        void               setSaveDateTime(const KDateTime& dt)             { d->mEventData->setSaveDateTime(dt); }
        void               setLateCancel(int lc)                            { d->mEventData->setLateCancel(lc); }
        void               setAutoClose(bool ac)                            { d->mEventData->setAutoClose(ac); }
        void               setRepeatAtLogin(bool rl)                        { d->mEventData->setRepeatAtLogin(rl); }
        void               setExcludeHolidays(bool ex)                      { d->mEventData->setExcludeHolidays(ex); }
        void               setWorkTimeOnly(bool wto)                        { d->mEventData->setWorkTimeOnly(wto); }
        void               setKMailSerialNumber(unsigned long n)            { d->mEventData->setKMailSerialNumber(n); }
        void               setLogFile(const QString& logfile)               { d->mEventData->setLogFile(logfile); }
        void               setReminder(int minutes, bool onceOnly)          { d->mEventData->setReminder(minutes, onceOnly); }
        bool               defer(const DateTime& dt, bool reminder, bool adjustRecurrence = false)
                                                               { return d->mEventData->defer(dt, reminder, Preferences::startOfDay(), adjustRecurrence); }
        void               cancelDefer()                       { d->mEventData->cancelDefer(); }
        void               setDeferDefaultMinutes(int minutes, bool dateOnly = false)
                                                               { d->mEventData->setDeferDefaultMinutes(minutes, dateOnly); }
        bool               setDisplaying(const KAEvent& e, KAAlarm::Type t, const QString& resourceID, const KDateTime& dt, bool showEdit, bool showDefer)
                                                               { return d->setDisplaying(*e.d, t, resourceID, dt, showEdit, showDefer); }
        void               reinstateFromDisplaying(const KCal::Event* e, QString& resourceID, bool& showEdit, bool& showDefer)
                                                               { d->mEventData->reinstateFromDisplaying(e, resourceID, showEdit, showDefer); }
        void               setCommandError(const QString& configString)  { d->setCommandError(configString); }
        void               setCommandError(CmdErrType t) const { d->setCommandError(t); }
        void               setArchive()                        { d->mEventData->setArchive(); }
        void               setEnabled(bool enable)             { d->mEventData->setEnabled(enable); }
        void               startChanges()                      { d->mEventData->startChanges(); }
        void               endChanges()                        { d->mEventData->endChanges(); }
        void               setUpdated()                        { d->mEventData->setUpdated(); }
        void               clearUpdated() const                { d->mEventData->clearUpdated(); }
        void               clearResourceId()                   { d->mEventData->clearResourceId(); }
        void               updateWorkHours() const             { if (d->mEventData->workTimeOnly()) const_cast<KAEvent*>(this)->d->eventUpdated(d->mEventData); }
        void               updateHolidays() const              { if (d->mEventData->holidaysExcluded()) const_cast<KAEvent*>(this)->d->eventUpdated(d->mEventData); }
        void               removeExpiredAlarm(KAAlarm::Type t) { d->mEventData->removeExpiredAlarm(t); }
        void               incrementRevision()                 { d->mEventData->incrementRevision(); }

        KAEventData*       eventData() const                   { return d->mEventData; }
        const QString&     cleanText() const                   { return d->mEventData->cleanText(); }
        QString            message() const                     { return d->mEventData->message(); }
        QString            fileName() const                    { return d->mEventData->fileName(); }
        QString            command() const                     { return d->mEventData->command(); }
        uint               emailFromId() const                 { return d->mEventData->emailFromId(); }
        const EmailAddressList& emailAddresses() const         { return d->mEventData->emailAddresses(); }
        QString            emailAddresses(const QString& sep) const  { return d->mEventData->emailAddresses().join(sep); }
        QStringList        emailPureAddresses() const          { return d->mEventData->emailPureAddresses(); }
        QString            emailPureAddresses(const QString& sep) const  { return d->mEventData->emailPureAddresses(sep); }
        const QString&     emailSubject() const                { return d->mEventData->emailSubject(); }
        const QStringList& emailAttachments() const            { return d->mEventData->emailAttachments(); }
        QString            emailAttachments(const QString& sep) const  { return d->mEventData->emailAttachments().join(sep); }
        bool               emailBcc() const                    { return d->mEventData->emailBcc(); }
        const QColor&      bgColour() const                    { return d->mEventData->bgColour(); }
        const QColor&      fgColour() const                    { return d->mEventData->fgColour(); }
        bool               useDefaultFont() const              { return d->mEventData->useDefaultFont(); }
        QFont              font() const;
        int                lateCancel() const                  { return d->mEventData->lateCancel(); }
        bool               autoClose() const                   { return d->mEventData->autoClose(); }
        bool               commandScript() const               { return d->mEventData->commandScript(); }
        bool               confirmAck() const                  { return d->mEventData->confirmAck(); }
        bool               repeatAtLogin() const               { return d->mEventData->repeatAtLogin(); }
        Repetition         repetition() const                  { return d->mEventData->repetition(); }
        bool               beep() const                        { return d->mEventData->beep(); }
        bool               isTemplate() const                  { return d->mEventData->isTemplate(); }
        const QString&     templateName() const                { return d->mEventData->templateName(); }
        bool               usingDefaultTime() const            { return d->mEventData->usingDefaultTime(); }
        int                templateAfterTime() const           { return d->mEventData->templateAfterTime(); }
        KAAlarm            alarm(KAAlarm::Type t) const        { return d->mEventData->alarm(t); }
        KAAlarm            firstAlarm() const                  { return d->mEventData->firstAlarm(); }
        KAAlarm            nextAlarm(const KAAlarm& al) const  { return d->mEventData->nextAlarm(al); }
        KAAlarm            nextAlarm(KAAlarm::Type t) const    { return d->mEventData->nextAlarm(t); }
        KAAlarm            convertDisplayingAlarm() const { return d->mEventData->convertDisplayingAlarm(); }
        bool               updateKCalEvent(KCal::Event* e, bool checkUid = true, bool original = false) const
                                                          { return d->mEventData->updateKCalEvent(e, checkUid, original); }
        KAEventData::Action action() const                { return d->mEventData->action(); }
        bool               displayAction() const          { return d->mEventData->displayAction(); }
        const QString&     id() const                     { return d->mEventData->id(); }
        bool               valid() const                  { return d->mEventData->valid(); }
        int                alarmCount() const             { return d->mEventData->alarmCount(); }
        const DateTime&    startDateTime() const          { return d->mEventData->startDateTime(); }
        DateTime           mainDateTime(bool withRepeats = false) const
                                                          { return d->mEventData->mainDateTime(withRepeats); }
        QDate              mainDate() const               { return d->mEventData->mainDate(); }
        QTime              mainTime() const               { return d->mEventData->mainTime(); }
        DateTime           mainEndRepeatTime() const      { return d->mEventData->mainEndRepeatTime(); }
        DateTime           nextTrigger(TriggerType t) const { return d->nextTrigger(t); }
        int                reminder() const               { return d->mEventData->reminder(); }
        bool               reminderOnceOnly() const       { return d->mEventData->reminderOnceOnly(); }
        bool               reminderDeferral() const       { return d->mEventData->reminderDeferral(); }
        int                reminderArchived() const       { return d->mEventData->reminderArchived(); }
        DateTime           deferDateTime() const          { return d->mEventData->deferDateTime(); }
        DateTime           deferralLimit(KAEventData::DeferLimitType* t = 0) const
                                                          { return d->mEventData->deferralLimit(Preferences::startOfDay(), t); }
        int                deferDefaultMinutes() const    { return d->mEventData->deferDefaultMinutes(); }
        bool               deferDefaultDateOnly() const   { return d->mEventData->deferDefaultDateOnly(); }
        const QString&     messageFileOrCommand() const   { return d->mEventData->messageFileOrCommand(); }
        QString            logFile() const                { return d->mEventData->logFile(); }
        bool               commandXterm() const           { return d->mEventData->commandXterm(); }
        bool               commandDisplay() const         { return d->mEventData->commandDisplay(); }
        unsigned long      kmailSerialNumber() const      { return d->mEventData->kmailSerialNumber(); }
        bool               copyToKOrganizer() const       { return d->mEventData->copyToKOrganizer(); }
        bool               holidaysExcluded() const       { return d->mEventData->holidaysExcluded(); }
        bool               workTimeOnly() const           { return d->mEventData->workTimeOnly(); }
        bool               speak() const                  { return d->mEventData->speak(); }
        const QString&     audioFile() const              { return d->mEventData->audioFile(); }
        float              soundVolume() const            { return d->mEventData->soundVolume(); }
        float              fadeVolume() const             { return d->mEventData->fadeVolume(); }
        int                fadeSeconds() const            { return d->mEventData->fadeSeconds(); }
        bool               repeatSound() const            { return d->mEventData->repeatSound(); }
        const QString&     preAction() const              { return d->mEventData->preAction(); }
        const QString&     postAction() const             { return d->mEventData->postAction(); }
        bool               cancelOnPreActionError() const { return d->mEventData->cancelOnPreActionError(); }
        bool               recurs() const                 { return d->mEventData->recurs(); }
        KARecurrence::Type recurType() const              { return d->mEventData->recurType(); }
        KARecurrence*      recurrence() const             { return d->mEventData->recurrence(); }
        int                recurInterval() const          { return d->mEventData->recurInterval(); }  // recurrence period in units of the recurrence period type (minutes, days, etc)
        KCal::Duration     longestRecurrenceInterval() const  { return d->mEventData->longestRecurrenceInterval(); }
        QString            recurrenceText(bool brief = false) const  { return d->mEventData->recurrenceText(brief); }
        QString            repetitionText(bool brief = false) const  { return d->mEventData->repetitionText(brief); }
        bool               occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const
                                                          { return d->mEventData->occursAfter(preDateTime, Preferences::startOfDay(), includeRepetitions); }
        KAEventData::OccurType nextOccurrence(const KDateTime& preDateTime, DateTime& result, KAEventData::OccurOption o = KAEventData::IGNORE_REPETITION) const
                                                          { return d->nextOccurrence(preDateTime, result, o); }
        KAEventData::OccurType previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const
                                                          { return d->mEventData->previousOccurrence(afterDateTime, result, Preferences::startOfDay(), includeRepetitions); }
        int                flags() const                  { return d->mEventData->flags(); }
        bool               deferred() const               { return d->mEventData->deferred(); }
        bool               toBeArchived() const           { return d->mEventData->toBeArchived(); }
        bool               enabled() const                { return d->mEventData->enabled(); }
        bool               updated() const                { return d->mEventData->updated(); }
        bool               mainExpired() const            { return d->mEventData->mainExpired(); }
        bool               expired() const                { return d->mEventData->expired(); }
        KCalEvent::Status  category() const               { return d->mEventData->category(); }
        bool               displaying() const             { return d->mEventData->displaying(); }
        QString            resourceId() const             { return d->mEventData->resourceId(); }
        AlarmResource*     resource() const               { return d->mResource; }
        CmdErrType         commandError() const           { return d->mCommandError; }
        static QString     commandErrorConfigGroup()      { return Private::mCmdErrConfigGroup; }

        bool               setRepetition(const Repetition& r)  { return d->mEventData->setRepetition(r); }
        void               setNoRecur()                   { d->mEventData->setNoRecur(); }
        void               setRecurrence(const KARecurrence& r)  { d->mEventData->setRecurrence(r); }
        bool               setRecurMinutely(int freq, int count, const KDateTime& end)
                                                { return d->mEventData->setRecurMinutely(freq, count, end); }
        bool               setRecurDaily(int freq, const QBitArray& days, int count, const QDate& end)
                                                { return d->mEventData->setRecurDaily(freq, days, count, end); }
        bool               setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end)
                                                { return d->mEventData->setRecurWeekly(freq, days, count, end); }
        bool               setRecurMonthlyByDate(int freq, const QList<int>& days, int count, const QDate& end)
                                                { return d->mEventData->setRecurMonthlyByDate(freq, days, count, end); }
        bool               setRecurMonthlyByPos(int freq, const QList<KAEventData::MonthPos>& pos, int count, const QDate& end)
                                                { return d->mEventData->setRecurMonthlyByPos(freq, pos, count, end); }
        bool               setRecurAnnualByDate(int freq, const QList<int>& months, int day, KARecurrence::Feb29Type f, int count, const QDate& end)
                                                { return d->mEventData->setRecurAnnualByDate(freq, months, day, f, count, end); }
        bool               setRecurAnnualByPos(int freq, const QList<KAEventData::MonthPos>& pos, const QList<int>& months, int count, const QDate& end)
                                                       { return d->mEventData->setRecurAnnualByPos(freq, pos, months, count, end); }
        void               adjustRecurrenceStartOfDay()  { d->mEventData->adjustRecurrenceStartOfDay(); }
#ifdef KDE_NO_DEBUG_OUTPUT
        void               dumpDebug() const  { }
#else
        void               dumpDebug() const    { d->dumpDebug(); }
#endif

    private:
        class Private : public KAEventData::Observer, public QSharedData
        {
            public:
                Private();
                Private(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg,
                        const QFont& f, KAEventData::Action, int lateCancel, int flags, bool changesPending = false);
                explicit Private(const KCal::Event*);
                Private(const Private&);
                ~Private()         { delete mEventData; }
                Private&           operator=(const Private& e)       { if (&e != this) copy(e);  return *this; }
                bool               setDisplaying(const Private&, KAAlarm::Type, const QString& resourceID, const KDateTime& dt, bool showEdit, bool showDefer);
                void               setCommandError(const QString& configString);
                void               setCommandError(CmdErrType) const;
                DateTime           nextTrigger(TriggerType) const;
                KAEventData::OccurType nextOccurrence(const KDateTime& preDateTime, DateTime& result, KAEventData::OccurOption o = KAEventData::IGNORE_REPETITION) const
                                         { return mEventData->nextOccurrence(preDateTime, result, Preferences::startOfDay(), o); }
                KAEventData::OccurType previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const
                                         { return mEventData->previousOccurrence(afterDateTime, result, Preferences::startOfDay(), includeRepetitions); }
                virtual void       eventUpdated(const KAEventData*);
#ifdef KDE_NO_DEBUG_OUTPUT
                void               dumpDebug() const  { }
#else
                void               dumpDebug() const;
#endif
            private:
                void               copy(const Private&, bool copyEventData = true);
                bool               mayOccurDailyDuringWork(const KDateTime&) const;
                int                nextWorkRepetition(const KDateTime& pre) const;
                void               calcNextWorkingTime(const DateTime& nextTrigger) const;
                DateTime           nextWorkingTime() const;

            public:
                static QString     mCmdErrConfigGroup; // config file group for command error recording
                AlarmResource*     mResource;          // resource which owns the event (for convenience - not used by this class)
            private:
                mutable DateTime   mAllTrigger;        // next trigger time, including reminders, ignoring working hours
                mutable DateTime   mMainTrigger;       // next trigger time, ignoring reminders and working hours
                mutable DateTime   mAllWorkTrigger;    // next trigger time, taking account of reminders and working hours
                mutable DateTime   mMainWorkTrigger;   // next trigger time, ignoring reminders but taking account of working hours
            public:
                mutable CmdErrType mCommandError;      // command execution error last time the alarm triggered
                KAEventData*       mEventData;         // event data for this instance
        };

        QSharedDataPointer<class Private> d;
};

#endif // KAEVENT_H
