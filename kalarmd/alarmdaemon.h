/*
    KDE Alarm Daemon.

    This file is part of the KDE alarm daemon.
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>
    Based on the original, (c) 1998, 1999 Preston Brown

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

#ifndef _ALARMDAEMON_H
#define _ALARMDAEMON_H

#include <ksimpleconfig.h>

#include <libkcal/calendarlocal.h>

#include "alarmdaemoniface.h"
#include "clientinfo.h"
#include "adcalendar.h"
#include "adconfigdatarw.h"

class AlarmDaemon : public QObject, public ADConfigDataRW, virtual public AlarmDaemonIface
{
    Q_OBJECT
  public:
    AlarmDaemon(QObject *parent = 0L, const char *name = 0L);
    virtual ~AlarmDaemon();

  private slots:
    void    calendarLoaded( ADCalendarBase *, bool success );
    void    checkAlarmsSlot();
    void    checkAlarms();

  private:
    // DCOP interface
    void    enableAutoStart(bool enable);
    void    enableCal(const QString& urlString, bool enable)
                       { enableCal_(expandURL(urlString), enable); }
    void    reloadCal(const QCString& appname, const QString& urlString)
                       { reloadCal_(appname, expandURL(urlString), false); }
    void    reloadMsgCal(const QCString& appname, const QString& urlString)
                       { reloadCal_(appname, expandURL(urlString), true); }
    void    addCal(const QCString& appname, const QString& urlString)
                       { addCal_(appname, expandURL(urlString), false); }
    void    addMsgCal(const QCString& appname, const QString& urlString)
                       { addCal_(appname, expandURL(urlString), true); }
    void    removeCal(const QString& urlString)
                       { removeCal_(expandURL(urlString)); }
    void    resetMsgCal(const QCString& appname, const QString& urlString)
                       { resetMsgCal_(appname, expandURL(urlString)); }
    void    registerApp(const QCString& appName, const QString& appTitle,
                        const QCString& dcopObject, int notificationType,
                        bool displayCalendarName)
                       { registerApp_(appName, appTitle, dcopObject, notificationType,
                                      displayCalendarName, false); }
    void    reregisterApp(const QCString& appName, const QString& appTitle,
                        const QCString& dcopObject, int notificationType,
                        bool displayCalendarName)
                       { registerApp_(appName, appTitle, dcopObject, notificationType,
                                      displayCalendarName, true); }
    void    registerGui(const QCString& appName, const QCString& dcopObject);
    void    readConfig();
    void    quit();
    void    forceAlarmCheck() { checkAlarms(); }
    void    dumpDebug();
    QStringList    dumpAlarms();

  private:

    struct GuiInfo
    {
      GuiInfo()  { }
      explicit GuiInfo(const QCString &dcopObj) : dcopObject(dcopObj) { }
      QCString  dcopObject;     // DCOP object name
    };
    typedef QMap<QCString, GuiInfo> GuiMap;  // maps GUI client names against their data

    void        registerApp_(const QCString& appName, const QString& appTitle,
                        const QCString& dcopObject, int notificationType,
                        bool displayCalendarName, bool reregister);
    void        enableCal_(const QString& urlString, bool enable);
    void        addCal_(const QCString& appname, const QString& urlString, bool msgCal);
    void        reloadCal_(const QCString& appname, const QString& urlString, bool msgCal);
    void        reloadCal_(ADCalendarBase*);
    void        resetMsgCal_(const QCString& appname, const QString& urlString);
    void        removeCal_(const QString& urlString);
    bool        checkAlarms(ADCalendarBase*);
    void        checkAlarms(const QCString& appName);
    void        checkEventAlarms(const Event& event, QValueList<QDateTime>& alarmtimes);
    bool        notifyEvent(ADCalendarBase*, const QString& eventID);
    void        notifyGuiCalStatus(const ADCalendarBase*);
    void        notifyGui(AlarmGuiChangeType, const QString& calendarURL = QString::null);
    void        notifyGui(AlarmGuiChangeType, const QString& calendarURL, const QCString &appname);
//    void        writeConfigClientGui(const QCString& appName, const QString& dcopObject);
    const GuiInfo* getGuiInfo(const QCString &appName) const;
    void        addConfigClient(KSimpleConfig&, const QCString& appName, const QString& key);
    void        readCheckInterval();
    void        setTimerStatus();

    GuiMap            mGuis;                // client GUI application names and data
    QTimer*           mAlarmTimer;
    QString           mClientDataFile;      // path of file containing client data
    int               mCheckInterval;       // alarm check interval (minutes)
    bool              mAlarmTimerSyncing;   // true while alarm timer interval < 1 minute
};

#endif
