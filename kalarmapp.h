/*
 *  kalarmapp.h  -  the KAlarm application object
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef KALARMAPP_H
#define KALARMAPP_H

class QDateTime;

#include <kuniqueapp.h>
#include <kurl.h>
class KAction;

#include "msgevent.h"
class AlarmCalendar;
class KAlarmMainWindow;
class MessageWin;
class TrayWindow;
class DaemonGuiHandler;
class Settings;
class ActionAlarmsEnabled;

extern const char* DAEMON_APP_NAME;
extern const char* DAEMON_DCOP_OBJECT;
extern const char* GUI_DCOP_OBJECT_NAME;


class DcopHandler : public QWidget, DCOPObject
{
		Q_OBJECT
	public:
		DcopHandler(const char* name);
		~DcopHandler()  { }
		virtual bool process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray& replyData);
};


class KAlarmApp : public KUniqueApplication
{
		Q_OBJECT
	public:
		~KAlarmApp();
		virtual int       newInstance();
		static KAlarmApp* getInstance();
		AlarmCalendar&    getCalendar()                   { return *mCalendar; }
		void              checkCalendar()                 { initCheck(); }
		Settings*         settings()                      { return mSettings; }
		bool              KDEDesktop() const              { return mKDEDesktop; }
		bool              runInSystemTray() const;
		bool              sessionClosingDown() const      { return mSessionClosingDown; }
		void              quitIf()                        { quitIf(0); }
		void              addWindow(TrayWindow* w)        { mTrayWindow = w; }
		void              removeWindow(TrayWindow*);
		TrayWindow*       trayWindow() const              { return mTrayWindow; }
		KAlarmMainWindow* trayMainWindow() const;
		bool              displayTrayIcon(bool show, KAlarmMainWindow* = 0L);
		bool              trayIconDisplayed() const       { return !!mTrayWindow; }
		DaemonGuiHandler* daemonGuiHandler() const        { return mDaemonGuiHandler; }
		ActionAlarmsEnabled* actionAlarmEnable() const    { return mActionAlarmEnable; }
		KAction*          actionPreferences() const       { return mActionPrefs; }
		KAction*          actionDaemonPreferences() const { return mActionDaemonPrefs; }
		void              resetDaemon();
		bool              isDaemonRunning();
		void              readDaemonCheckInterval();
		QSize             readConfigWindowSize(const char* window, const QSize& defaultSize);
		void              writeConfigWindowSize(const char* window, const QSize&);
		virtual void      commitData(QSessionManager&);

		bool              addEvent(const KAlarmEvent&, KAlarmMainWindow*);
		void              modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow*);
		void              updateEvent(const KAlarmEvent&, KAlarmMainWindow*);
		void              deleteEvent(KAlarmEvent&, KAlarmMainWindow*, bool tellDaemon = true);
		bool              execAlarm(KAlarmEvent&, const KAlarmAlarm&, bool reschedule, bool allowDefer = true);
		void              rescheduleAlarm(KAlarmEvent&, int alarmID);
//		void              triggerEvent(const QString& eventID)        { handleEvent(eventID, EVENT_TRIGGER); }
		void              deleteEvent(const QString& eventID)         { handleEvent(eventID, EVENT_CANCEL); }
		int               maxLateness();
		// Methods called indirectly by the DCOP interface
		bool              scheduleEvent(const QString& text, const QDateTime&, const QColor& bg, int flags,
		                                const QString& audioFile, KAlarmAlarm::Type, KAlarmEvent::RecurType,
		                                int repeatInterval, int repeatCount, const QDateTime& endTime);
		void              handleEvent(const QString& calendarFile, const QString& eventID)    { handleEvent(calendarFile, eventID, EVENT_HANDLE); }
		void              triggerEvent(const QString& calendarFile, const QString& eventID)   { handleEvent(calendarFile, eventID, EVENT_TRIGGER); }
		void              deleteEvent(const QString& calendarFile, const QString& eventID)    { handleEvent(calendarFile, eventID, EVENT_CANCEL); }

		static int        isTextFile(const KURL&);
	public slots:
		void              displayMainWindow();
		void              slotDaemonPreferences();
	signals:
		void              trayIconToggled();
	protected:
		KAlarmApp();
	private slots:
		void              slotPreferences();
		void              toggleAlarmsEnabled();
		void              slotSettingsChanged();
	private:
		enum EventFunc { EVENT_HANDLE, EVENT_TRIGGER, EVENT_CANCEL };
		enum AlarmFunc { ALARM_TRIGGER, ALARM_CANCEL, ALARM_RESCHEDULE };
		bool              initCheck(bool calendarOnly = false);
		void              quitIf(int exitCode);
		void              changeStartOfDay();
		void              setUpDcop();
		bool              stopDaemon();
		void              startDaemon();
		void              reloadDaemon();
		void              registerWithDaemon();
		void              handleEvent(const QString& calendarFile, const QString& eventID, EventFunc);
		bool              handleEvent(const QString& eventID, EventFunc);
		void              handleAlarm(KAlarmEvent&, KAlarmAlarm&, AlarmFunc, bool updateCalAndDisplay);

		static KAlarmApp*     theInstance;         // the one and only KAlarmApp instance
		static int            activeCount;         // number of active instances without main windows
		DcopHandler*          mDcopHandler;        // the parent of the main DCOP receiver object
		DaemonGuiHandler*     mDaemonGuiHandler;   // the parent of the system tray DCOP receiver object
		TrayWindow*           mTrayWindow;         // active system tray icon
		AlarmCalendar*        mCalendar;           // the calendar containing all the alarms
		ActionAlarmsEnabled*  mActionAlarmEnable;  // action to enable/disable alarms
		KAction*              mActionPrefs;        // action to display the preferences dialog
		KAction*              mActionDaemonPrefs;  // action to display the alarm daemon preferences dialog
		Settings*             mSettings;           // program preferences
		QDateTime             mLastDaemonCheck;    // last time daemon checked alarms before check interval change
		QDateTime             mNextDaemonCheck;    // next time daemon will check alarms after check interval change
		QTime                 mStartOfDay;         // start-of-day time currently in use
		int                   mDaemonCheckInterval;// daemon check interval (seconds)
		bool                  mDaemonRegistered;   // true if we've registered with alarm daemon
		bool                  mKDEDesktop;         // running on KDE desktop
		bool                  mDaemonRunning;      // whether the alarm daemon is currently running
		bool                  mSessionClosingDown; // session manager is closing the application
		bool                  mOldRunInSystemTray; // running continuously in system tray was selected
		bool                  mDisableAlarmsIfStopped; // disable alarms whenever KAlarm is not running
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H
