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

class AlarmCalendar;
class KAlarmEvent;
class KAlarmAlarm;
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
		bool              addMessage(const KAlarmEvent&, KAlarmMainWindow*);
		void              modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow*);
		void              updateMessage(const KAlarmEvent&, KAlarmMainWindow*);
		void              deleteMessage(KAlarmEvent&, KAlarmMainWindow*, bool tellDaemon = true);
		void              rescheduleAlarm(KAlarmEvent&, int alarmID);
		void              displayMessage(const QString& eventID)        { handleMessage(eventID, EVENT_DISPLAY); }
		void              deleteMessage(const QString& eventID)         { handleMessage(eventID, EVENT_CANCEL); }
		QSize             readConfigWindowSize(const char* window, const QSize& defaultSize);
		void              writeConfigWindowSize(const char* window, const QSize&);
		virtual void      commitData(QSessionManager&);
		// DCOP interface methods
		bool              scheduleMessage(const QString& message, const QDateTime*, const QColor& bg,
		                                  int flags, bool file, int repeatCount, int repeatInterval);
		void              handleMessage(const QString& calendarFile, const QString& eventID)    { handleMessage(calendarFile, eventID, EVENT_HANDLE); }
		void              displayMessage(const QString& calendarFile, const QString& eventID)   { handleMessage(calendarFile, eventID, EVENT_DISPLAY); }
		void              deleteMessage(const QString& calendarFile, const QString& eventID)    { handleMessage(calendarFile, eventID, EVENT_CANCEL); }
		static const int          MAX_LATENESS;   // maximum number of seconds late to display a late-cancel alarm
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
		enum EventFunc { EVENT_HANDLE, EVENT_DISPLAY, EVENT_CANCEL };
		enum AlarmFunc { ALARM_DISPLAY, ALARM_CANCEL, ALARM_RESCHEDULE };
		bool              initCheck(bool calendarOnly = false);
		void              quitIf(int exitCode);
		void              setUpDcop();
		bool              stopDaemon();
		void              startDaemon();
		void              reloadDaemon();
		void              registerWithDaemon();
		void              handleMessage(const QString& calendarFile, const QString& eventID, EventFunc);
		bool              handleMessage(const QString& eventID, EventFunc);
		void              handleAlarm(KAlarmEvent&, KAlarmAlarm&, AlarmFunc, bool updateCalAndDisplay);
		static bool       convWakeTime(const QCString timeParam, QDateTime&);

		static KAlarmApp*     theInstance;         // the one and only KAlarmApp instance
		static int            activeCount;         // number of active instances without main windows
		DcopHandler*          mDcopHandler;        // the parent of the main DCOP receiver object
		DaemonGuiHandler*     mDaemonGuiHandler;   // the parent of the system tray DCOP receiver object
		TrayWindow*           mTrayWindow;         // active system tray icon
		AlarmCalendar*        mCalendar;           // the calendar containing all the alarms
		ActionAlarmsEnabled*  mActionAlarmEnable;  // action to enable/disable alarms
		KAction*              mActionPrefs;        // action to display the preferences dialog
		KAction*              mActionDaemonPrefs;  // action to display the alarm daemon preferences dialog
		bool                  mDaemonRegistered;   // true if we've registered with alarm daemon
		Settings*             mSettings;           // program preferences
		bool                  mKDEDesktop;         // running on KDE desktop
		bool                  mDaemonRunning;      // whether the alarm daemon is currently running
		bool                  mSessionClosingDown; // session manager is closing the application
		bool                  mOldRunInSystemTray; // running continuously in system tray was selected
		bool                  mDisableAlarmsIfStopped; // disable alarms whenever KAlarm is not running
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H
