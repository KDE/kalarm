/*
 *  kalarmapp.h  -  description
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef KALARMAPP_H
#define KALARMAPP_H

#include <vector>

#include <kuniqueapp.h>
#include <kurl.h>

#include <calendarlocal.h>
#include "msgevent.h"
using namespace KCal;

class KAlarmMainWindow;
class MessageWin;
class GeneralSettings;


class MainWidget : public QWidget, DCOPObject
{
		Q_OBJECT
	public:
		MainWidget(const char* name);
		~MainWidget()  { }
		virtual bool process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray& replyData);
};


class AlarmCalendar
{
	public:
		AlarmCalendar() : calendar(0L) { }
		bool              open();
		bool              load();
		bool              save()                              { return save(localFile); }
		void              close();
		MessageEvent*     getEvent(const QString& uniqueID)   { return static_cast<MessageEvent*>(calendar->getEvent(uniqueID)); }
		QPtrList<Event>   getAllEvents()                      { return calendar->getAllEvents(); }
		void              addEvent(const MessageEvent* e)     { calendar->addEvent(const_cast<MessageEvent*>(e)); }
		void              updateEvent(const MessageEvent* e)  { calendar->updateEvent(const_cast<MessageEvent*>(e)); }
		void              deleteEvent(MessageEvent* e)        { calendar->deleteEvent(e); }
		bool              isOpen() const                      { return !!calendar; }
		void              getURL() const;
		const QString     urlString() const                   { getURL();  return url.url(); }
	private:
		class AlarmCalendarLocal : public CalendarLocal
		{
			public:
				void updateEvent(Incidence* i)  { CalendarLocal::updateEvent(i); }
		};
		AlarmCalendarLocal* calendar;
		KURL              url;         // URL of calendar file
		QString           localFile;   // local name of calendar file
		bool              vCal;        // true if calendar file is in VCal format

		bool              save(const QString& tempFile);
};


class KAlarmApp : public KUniqueApplication
{
	public:
		~KAlarmApp();
		virtual int       newInstance();
		static KAlarmApp* getInstance();
		AlarmCalendar&    getCalendar()       { return calendar; }
		GeneralSettings*  generalSettings()   { return m_generalSettings; }
		void              deleteWindow(KAlarmMainWindow*);
		void              resetDaemon();
		void              addMessage(const MessageEvent*, KAlarmMainWindow*);
		void              modifyMessage(MessageEvent* oldEvent, const MessageEvent* newEvent, KAlarmMainWindow*);
		void              deleteMessage(MessageEvent*, KAlarmMainWindow*, bool tellDaemon = true);
		bool              rescheduleMessage(MessageEvent*);
		void              displayMessage(const QString& eventID)        { handleMessage(eventID, EVENT_DISPLAY); }
		void              deleteMessage(const QString& eventID)         { handleMessage(eventID, EVENT_CANCEL); }
		void              rescheduleMessage(const QString& eventID)     { handleMessage(eventID, EVENT_RESCHEDULE); }
		// DCOP interface methods
		bool              scheduleMessage(const QString& message, const QDateTime*, const QColor& bg, int flags, int repeatCount, int repeatInterval);
		void              handleMessage(const QString& calendarFile, const QString& eventID)    { handleMessage(calendarFile, eventID, EVENT_HANDLE); }
		void              displayMessage(const QString& calendarFile, const QString& eventID)   { handleMessage(calendarFile, eventID, EVENT_DISPLAY); }
		void              deleteMessage(const QString& calendarFile, const QString& eventID)    { handleMessage(calendarFile, eventID, EVENT_CANCEL); }
	protected:
		KAlarmApp();
	private:
		enum EventFunc { EVENT_HANDLE, EVENT_DISPLAY, EVENT_CANCEL, EVENT_RESCHEDULE };
		bool              initCheck(bool daemon = true);
		bool              stopDaemon();
		void              startDaemon();
		void              reloadDaemon();
		void              handleMessage(const QString& calendarFile, const QString& eventID, EventFunc);
		bool              handleMessage(const QString& eventID, EventFunc);
		static bool       convWakeTime(const QCString timeParam, QDateTime&);

		static KAlarmApp*         theInstance;
		MainWidget*               mainWidget;          // the parent of the DCOP receiver object
		vector<KAlarmMainWindow*> mainWindowList;      // active main windows
		AlarmCalendar             calendar;
		bool                      daemonRegistered;    // true if we've registered with alarm daemon
		GeneralSettings*          m_generalSettings;   // general program preferences
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H
