/*
 *  alarmevent.h  -  represents calendar alarms and events
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

#ifndef KALARMEVENT_H
#define KALARMEVENT_H

#include "kaeventdata.h"
#include "preferences.h"

class AlarmResource;

/** KAEvent corresponds to a KCal::Event instance */
class KAEvent : public KAEventData::Observer
{
	public:
		typedef QList<KAEvent*> List;
		enum            // flags for use in DCOP calls, etc.
		{
			BEEP            = KAEventData::BEEP,    // sound audible beep when alarm is displayed
			REPEAT_AT_LOGIN = KAEventData::REPEAT_AT_LOGIN,    // repeat alarm at every login
			ANY_TIME        = KAEventData::ANY_TIME,    // only a date is specified for the alarm, not a time
			CONFIRM_ACK     = KAEventData::CONFIRM_ACK,    // closing the alarm message window requires confirmation prompt
			EMAIL_BCC       = KAEventData::EMAIL_BCC,    // blind copy the email to the user
			DEFAULT_FONT    = KAEventData::DEFAULT_FONT,    // use default alarm message font
			REPEAT_SOUND    = KAEventData::REPEAT_SOUND,    // repeat sound file while alarm is displayed
			DISABLED        = KAEventData::DISABLED,   // alarm is currently disabled
			AUTO_CLOSE      = KAEventData::AUTO_CLOSE,   // auto-close alarm window after late-cancel period
			SCRIPT          = KAEventData::SCRIPT,   // command is a script, not a shell command line
			EXEC_IN_XTERM   = KAEventData::EXEC_IN_XTERM,   // execute command in terminal window
			SPEAK           = KAEventData::SPEAK,  // speak the message when the alarm is displayed
			COPY_KORGANIZER = KAEventData::COPY_KORGANIZER,  // KOrganizer should hold a copy of the event
			EXCL_HOLIDAYS   = KAEventData::EXCL_HOLIDAYS,  // don't trigger alarm on holidays
			WORK_TIME_ONLY  = KAEventData::WORK_TIME_ONLY,  // trigger alarm only during working hours
			DISPLAY_COMMAND = KAEventData::DISPLAY_COMMAND, // display command output in alarm window
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
		KAEvent(const KAEvent&);
		~KAEvent()         { delete mEventData; }
		KAEvent&           operator=(const KAEvent& e)       { if (&e != this) copy(e);  return *this; }
		void               set(const KCal::Event*);
		void               set(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg, const QFont&, KAEventData::Action, int lateCancel, int flags, bool changesPending = false);
		void               setEmail(uint from, const EmailAddressList& addrs, const QString& subject, const QStringList& attachments)
		                                                                    { mEventData->setEmail(from, addrs, subject, attachments); }
		void               setResource(AlarmResource* r)                    { mResource = r; }
		void               setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds)
		                                                                    { mEventData->setAudioFile(filename, volume, fadeVolume, fadeSeconds); }
		void               setTemplate(const QString& name, int afterTime = -1) { mEventData->setTemplate(name, afterTime); }
		void               setActions(const QString& pre, const QString& post, bool cancelOnError)
		                                                                    { mEventData->setActions(pre, post, cancelOnError); }
		KAEventData::OccurType setNextOccurrence(const KDateTime& preDateTime)  { return mEventData->setNextOccurrence(preDateTime, Preferences::startOfDay()); }
		void               setFirstRecurrence()                             { mEventData->setFirstRecurrence(Preferences::startOfDay()); }
		void               setCategory(KCalEvent::Status s)                 { mEventData->setCategory(s); }
		void               setUid(KCalEvent::Status s)                      { mEventData->setUid(s); }
		void               setEventId(const QString& id)                    { mEventData->setEventId(id); }
		void               setTime(const KDateTime& dt)                     { mEventData->setTime(dt); }
		void               setSaveDateTime(const KDateTime& dt)             { mEventData->setSaveDateTime(dt); }
		void               setLateCancel(int lc)                            { mEventData->setLateCancel(lc); }
		void               setAutoClose(bool ac)                            { mEventData->setAutoClose(ac); }
		void               setRepeatAtLogin(bool rl)                        { mEventData->setRepeatAtLogin(rl); }
		void               setExcludeHolidays(bool ex)                      { mEventData->setExcludeHolidays(ex); }
		void               setWorkTimeOnly(bool wto)                        { mEventData->setWorkTimeOnly(wto); }
		void               setKMailSerialNumber(unsigned long n)            { mEventData->setKMailSerialNumber(n); }
		void               setLogFile(const QString& logfile)               { mEventData->setLogFile(logfile); }
		void               setReminder(int minutes, bool onceOnly)          { mEventData->setReminder(minutes, onceOnly); }
		bool               defer(const DateTime& dt, bool reminder, bool adjustRecurrence = false)
		                                                       { return mEventData->defer(dt, reminder, Preferences::startOfDay(), adjustRecurrence); }
		void               cancelDefer()                       { mEventData->cancelDefer(); }
		void               setDeferDefaultMinutes(int minutes) { mEventData->setDeferDefaultMinutes(minutes); }
		bool               setDisplaying(const KAEvent&, KAAlarm::Type, const QString& resourceID, const KDateTime& dt, bool showEdit, bool showDefer);
		void               reinstateFromDisplaying(const KCal::Event* e, QString& resourceID, bool& showEdit, bool& showDefer)
		                                                       { mEventData->reinstateFromDisplaying(e, resourceID, showEdit, showDefer); }
		void               setCommandError(const QString& configString);
		void               setCommandError(CmdErrType) const;
		void               setArchive()                        { mEventData->setArchive(); }
		void               setEnabled(bool enable)             { mEventData->setEnabled(enable); }
		void               startChanges()                      { mEventData->startChanges(); }
		void               endChanges()                        { mEventData->endChanges(); }
		void               setUpdated()                        { mEventData->setUpdated(); }
		void               clearUpdated() const                { mEventData->clearUpdated(); }
		void               clearResourceId()                   { mEventData->clearResourceId(); }
		void               updateWorkHours() const             { if (mEventData->workTimeOnly()) const_cast<KAEvent*>(this)->eventUpdated(mEventData); }
		void               updateHolidays() const              { if (mEventData->holidaysExcluded()) const_cast<KAEvent*>(this)->eventUpdated(mEventData); }
		void               removeExpiredAlarm(KAAlarm::Type t) { mEventData->removeExpiredAlarm(t); }
		void               incrementRevision()                 { mEventData->incrementRevision(); }

		KAEventData*       eventData() const                   { return mEventData; }
		const QString&     cleanText() const                   { return mEventData->cleanText(); }
		QString            message() const                     { return mEventData->message(); }
		QString            fileName() const                    { return mEventData->fileName(); }
		QString            command() const                     { return mEventData->command(); }
		uint               emailFromId() const                 { return mEventData->emailFromId(); }
		const EmailAddressList& emailAddresses() const         { return mEventData->emailAddresses(); }
		QString            emailAddresses(const QString& sep) const  { return mEventData->emailAddresses().join(sep); }
		const QString&     emailSubject() const                { return mEventData->emailSubject(); }
		const QStringList& emailAttachments() const            { return mEventData->emailAttachments(); }
		QString            emailAttachments(const QString& sep) const  { return mEventData->emailAttachments().join(sep); }
		bool               emailBcc() const                    { return mEventData->emailBcc(); }
		const QColor&      bgColour() const                    { return mEventData->bgColour(); }
		const QColor&      fgColour() const                    { return mEventData->fgColour(); }
		bool               useDefaultFont() const              { return mEventData->useDefaultFont(); }
		QFont              font() const;
		int                lateCancel() const                  { return mEventData->lateCancel(); }
		bool               autoClose() const                   { return mEventData->autoClose(); }
		bool               commandScript() const               { return mEventData->commandScript(); }
		bool               confirmAck() const                  { return mEventData->confirmAck(); }
		bool               repeatAtLogin() const               { return mEventData->repeatAtLogin(); }
		int                repeatCount() const                 { return mEventData->repeatCount(); }
		KCal::Duration     repeatInterval() const              { return mEventData->repeatInterval(); }
		bool               beep() const                        { return mEventData->beep(); }
		bool               isTemplate() const                  { return mEventData->isTemplate(); }
		const QString&     templateName() const                { return mEventData->templateName(); }
		bool               usingDefaultTime() const            { return mEventData->usingDefaultTime(); }
		int                templateAfterTime() const           { return mEventData->templateAfterTime(); }
		KAAlarm            alarm(KAAlarm::Type t) const        { return mEventData->alarm(t); }
		KAAlarm            firstAlarm() const                  { return mEventData->firstAlarm(); }
		KAAlarm            nextAlarm(const KAAlarm& al) const  { return mEventData->nextAlarm(al); }
		KAAlarm            nextAlarm(KAAlarm::Type t) const    { return mEventData->nextAlarm(t); }
		KAAlarm            convertDisplayingAlarm() const { return mEventData->convertDisplayingAlarm(); }
		bool               updateKCalEvent(KCal::Event* e, bool checkUid = true, bool original = false) const
		                                                  { return mEventData->updateKCalEvent(e, checkUid, original); }
		KAEventData::Action             action() const                 { return mEventData->action(); }
		bool               displayAction() const          { return mEventData->displayAction(); }
		const QString&     id() const                     { return mEventData->id(); }
		bool               valid() const                  { return mEventData->valid(); }
		int                alarmCount() const             { return mEventData->alarmCount(); }
		const DateTime&    startDateTime() const          { return mEventData->startDateTime(); }
		DateTime           mainDateTime(bool withRepeats = false) const
		                                                  { return mEventData->mainDateTime(withRepeats); }
		QDate              mainDate() const               { return mEventData->mainDate(); }
		QTime              mainTime() const               { return mEventData->mainTime(); }
		DateTime           mainEndRepeatTime() const      { return mEventData->mainEndRepeatTime(); }
		DateTime           nextTrigger(TriggerType) const;
		int                reminder() const               { return mEventData->reminder(); }
		bool               reminderOnceOnly() const       { return mEventData->reminderOnceOnly(); }
		bool               reminderDeferral() const       { return mEventData->reminderDeferral(); }
		int                reminderArchived() const       { return mEventData->reminderArchived(); }
		DateTime           deferDateTime() const          { return mEventData->deferDateTime(); }
		DateTime           deferralLimit(KAEventData::DeferLimitType* t = 0) const
		                                                  { return mEventData->deferralLimit(Preferences::startOfDay(), t); }
		int                deferDefaultMinutes() const    { return mEventData->deferDefaultMinutes(); }
		const QString&     messageFileOrCommand() const   { return mEventData->messageFileOrCommand(); }
		QString            logFile() const                { return mEventData->logFile(); }
		bool               commandXterm() const           { return mEventData->commandXterm(); }
		bool               commandDisplay() const         { return mEventData->commandDisplay(); }
		unsigned long      kmailSerialNumber() const      { return mEventData->kmailSerialNumber(); }
		bool               copyToKOrganizer() const       { return mEventData->copyToKOrganizer(); }
		bool               holidaysExcluded() const       { return mEventData->holidaysExcluded(); }
		bool               workTimeOnly() const           { return mEventData->workTimeOnly(); }
		bool               speak() const                  { return mEventData->speak(); }
		const QString&     audioFile() const              { return mEventData->audioFile(); }
		float              soundVolume() const            { return mEventData->soundVolume(); }
		float              fadeVolume() const             { return mEventData->fadeVolume(); }
		int                fadeSeconds() const            { return mEventData->fadeSeconds(); }
		bool               repeatSound() const            { return mEventData->repeatSound(); }
		const QString&     preAction() const              { return mEventData->preAction(); }
		const QString&     postAction() const             { return mEventData->postAction(); }
		bool               cancelOnPreActionError() const { return mEventData->cancelOnPreActionError(); }
		bool               recurs() const                 { return mEventData->recurs(); }
		KARecurrence::Type recurType() const              { return mEventData->recurType(); }
		KARecurrence*      recurrence() const             { return mEventData->recurrence(); }
		int                recurInterval() const          { return mEventData->recurInterval(); }  // recurrence period in units of the recurrence period type (minutes, days, etc)
		KCal::Duration     longestRecurrenceInterval() const  { return mEventData->longestRecurrenceInterval(); }
		QString            recurrenceText(bool brief = false) const  { return mEventData->recurrenceText(brief); }
		QString            repetitionText(bool brief = false) const  { return mEventData->repetitionText(brief); }
		bool               occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const
		                                                  { return mEventData->occursAfter(preDateTime, Preferences::startOfDay(), includeRepetitions); }
		KAEventData::OccurType nextOccurrence(const KDateTime& preDateTime, DateTime& result, KAEventData::OccurOption o = KAEventData::IGNORE_REPETITION) const
		                                                  { return mEventData->nextOccurrence(preDateTime, result, Preferences::startOfDay(), o); }
		KAEventData::OccurType previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const
		                                                  { return mEventData->previousOccurrence(afterDateTime, result, Preferences::startOfDay(), includeRepetitions); }
		int                flags() const                  { return mEventData->flags(); }
		bool               deferred() const               { return mEventData->deferred(); }
		bool               toBeArchived() const           { return mEventData->toBeArchived(); }
		bool               enabled() const                { return mEventData->enabled(); }
		bool               updated() const                { return mEventData->updated(); }
		bool               mainExpired() const            { return mEventData->mainExpired(); }
		bool               expired() const                { return mEventData->expired(); }
		KCalEvent::Status  category() const               { return mEventData->category(); }
		bool               displaying() const             { return mEventData->displaying(); }
		QString            resourceId() const             { return mEventData->resourceId(); }
		AlarmResource*     resource() const               { return mResource; }
		CmdErrType         commandError() const           { return mCommandError; }
		static QString     commandErrorConfigGroup()      { return mCmdErrConfigGroup; }

		bool               setRepetition(const KCal::Duration& d, int count)  { return mEventData->setRepetition(d, count); }
		void               setNoRecur()                   { mEventData->setNoRecur(); }
		void               setRecurrence(const KARecurrence& r)  { mEventData->setRecurrence(r); }
		bool               setRecurMinutely(int freq, int count, const KDateTime& end)
		                                        { return mEventData->setRecurMinutely(freq, count, end); }
		bool               setRecurDaily(int freq, const QBitArray& days, int count, const QDate& end)
		                                        { return mEventData->setRecurDaily(freq, days, count, end); }
		bool               setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end)
		                                        { return mEventData->setRecurWeekly(freq, days, count, end); }
		bool               setRecurMonthlyByDate(int freq, const QList<int>& days, int count, const QDate& end)
		                                        { return mEventData->setRecurMonthlyByDate(freq, days, count, end); }
		bool               setRecurMonthlyByPos(int freq, const QList<KAEventData::MonthPos>& pos, int count, const QDate& end)
		                                        { return mEventData->setRecurMonthlyByPos(freq, pos, count, end); }
		bool               setRecurAnnualByDate(int freq, const QList<int>& months, int day, KARecurrence::Feb29Type f, int count, const QDate& end)
		                                        { return mEventData->setRecurAnnualByDate(freq, months, day, f, count, end); }
		bool               setRecurAnnualByPos(int freq, const QList<KAEventData::MonthPos>& pos, const QList<int>& months, int count, const QDate& end)
	       	                                        { return mEventData->setRecurAnnualByPos(freq, pos, months, count, end); }
#ifdef NDEBUG
		void               dumpDebug() const  { }
#else
		void               dumpDebug() const;
#endif

	private:
		virtual void       eventUpdated(const KAEventData*);
		void               copy(const KAEvent&, bool copyEventData = true);
		bool               mayOccurDailyDuringWork(const KDateTime&) const;
		int                nextWorkRepetition(const KDateTime& pre) const;
		void               calcNextWorkingTime(const DateTime& nextTrigger) const;
		DateTime           nextWorkingTime() const;

		static QString     mCmdErrConfigGroup;// config file group for command error recording
		AlarmResource*     mResource;         // resource which owns the event (for convenience - not used by this class)
		mutable DateTime   mAllTrigger;       // next trigger time, including reminders, ignoring working hours
		mutable DateTime   mMainTrigger;      // next trigger time, ignoring reminders and working hours
		mutable DateTime   mAllWorkTrigger;   // next trigger time, taking account of reminders and working hours
		mutable DateTime   mMainWorkTrigger;  // next trigger time, ignoring reminders but taking account of working hours
		mutable CmdErrType mCommandError;     // command execution error last time the alarm triggered
		KAEventData*       mEventData;        // event data for this instance
};

#endif // KALARMEVENT_H
