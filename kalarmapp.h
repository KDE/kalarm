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
		int               load();
		bool              save()                              { return save(localFile); }
		void              close();
		Event*            getEvent(const QString& uniqueID)   { return calendar->getEvent(uniqueID); }
		QPtrList<Event>   getAllEvents()                      { return calendar->getAllEvents(); }
		void              addEvent(const KAlarmEvent&);
		void              updateEvent(const KAlarmEvent&);
		void              deleteEvent(const QString& eventID);
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

		bool              create();
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
		void              addMessage(const KAlarmEvent&, KAlarmMainWindow*);
		void              modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow*);
		void              updateMessage(const KAlarmEvent&, KAlarmMainWindow*);
		void              deleteMessage(KAlarmEvent&, KAlarmMainWindow*, bool tellDaemon = true);
		void              rescheduleAlarm(const QString& eventID, int alarmID);
		void              displayMessage(const QString& eventID)        { handleMessage(eventID, EVENT_DISPLAY); }
		void              deleteMessage(const QString& eventID)         { handleMessage(eventID, EVENT_CANCEL); }
		QSize             readConfigWindowSize(const char* window, const QSize& defaultSize);
		void              writeConfigWindowSize(const char* window, const QSize&);
		// DCOP interface methods
		bool              scheduleMessage(const QString& message, const QDateTime*, const QColor& bg,
		                                  int flags, bool file, int repeatCount, int repeatInterval);
		void              handleMessage(const QString& calendarFile, const QString& eventID)    { handleMessage(calendarFile, eventID, EVENT_HANDLE); }
		void              displayMessage(const QString& calendarFile, const QString& eventID)   { handleMessage(calendarFile, eventID, EVENT_DISPLAY); }
		void              deleteMessage(const QString& calendarFile, const QString& eventID)    { handleMessage(calendarFile, eventID, EVENT_CANCEL); }
		static const int          MAX_LATENESS = 65;   // maximum number of seconds late to display a late-cancel alarm
	protected:
		KAlarmApp();
	private:
		enum EventFunc { EVENT_HANDLE, EVENT_DISPLAY, EVENT_CANCEL };
		enum AlarmFunc { ALARM_DISPLAY, ALARM_CANCEL, ALARM_RESCHEDULE };
		bool              initCheck(bool daemon = true);
		bool              stopDaemon();
		void              startDaemon();
		void              reloadDaemon();
		void              handleMessage(const QString& calendarFile, const QString& eventID, EventFunc);
		bool              handleMessage(const QString& eventID, EventFunc);
		void              handleAlarm(KAlarmEvent&, KAlarmAlarm&, AlarmFunc);
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
