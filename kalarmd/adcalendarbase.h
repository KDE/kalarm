/*
    Calendar access for KDE Alarm Daemon and KDE Alarm Daemon GUI.

    This file is part of the KDE alarm daemon.
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>

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

#ifndef ADCALENDARBASE_H
#define ADCALENDARBASE_H

#include <libkcal/calendarlocal.h>
namespace KIO {
  class Job;
}
class AlarmDaemon;

using namespace KCal;

// Base class for Alarm Daemon calendar access
class ADCalendarBase : public CalendarLocal
{
    Q_OBJECT
  public:
    enum Type { KORGANIZER = 0, KALARM = 1 };
    ADCalendarBase(const QString& url, const QCString& appname, Type);
    ~ADCalendarBase()  { }

    const QString&   urlString() const   { return mUrlString; }
    const QCString&  appName() const     { return mAppName; }
    int              rcIndex() const     { return mRcIndex; }
    const QDateTime& lastCheck() const   { return mLastCheck; }
    bool             loaded() const      { return mLoaded; }
    Type             actionType() const  { return mActionType; }

    virtual void setEnabled( bool ) = 0;
    virtual bool enabled() const = 0;

    virtual void setAvailable( bool ) = 0;
    virtual bool available() const = 0;

    // client has registered since calendar was
    // constructed, but has not since added the
    // calendar. Monitoring is disabled.
    void setUnregistered( bool u ) { mUnregistered = u; }
    bool unregistered() const { return mUnregistered; }

    virtual bool loadFile() = 0;
    bool         downloading() const  { return !mTempFileName.isNull(); }

    void         setRcIndex(int i)                  { mRcIndex = i; }
    void         setLastCheck(const QDateTime& dt)  { mLastCheck = dt; }

    virtual void setEventHandled(const Event*, const QValueList<QDateTime> &) = 0;
    virtual bool eventHandled(const Event*, const QValueList<QDateTime> &) = 0;

    // Check status of mLoadedConnected and set it to true
    bool         setLoadedConnected();  

    void         dump() const;

  signals:
    void         loaded(ADCalendarBase*, bool success);

  protected:
    bool         loadFile_();

  private:
    ADCalendarBase(const ADCalendarBase&);             // prohibit copying
    ADCalendarBase& operator=(const ADCalendarBase&);  // prohibit copying
    void         loadLocalFile( const QString& filename );

  private slots:
    void         slotDownloadJobResult( KIO::Job * );

  protected:
    struct EventItem
    {
      EventItem() : eventSequence(0) { }
      EventItem(const QString& url, int seqno, const QValueList<QDateTime>& alarmtimes)
        : calendarURL(url), eventSequence(seqno), alarmTimes(alarmtimes) {}

      QString   calendarURL;
      int       eventSequence;
      QValueList<QDateTime> alarmTimes;
    };

    typedef QMap<QString, EventItem>  EventsMap;   // event ID, calendar URL/event sequence num
    static EventsMap  eventsHandled_; // IDs of displayed KALARM type events

  private:
    QString           mUrlString;     // calendar file URL
    QCString          mAppName;       // name of application owning this calendar
    Type              mActionType;    // action to take on event
    QDateTime         mLastCheck;     // time at which calendar was last checked for alarms
    QString           mTempFileName;  // temporary file used if currently downloading, else null
    int               mRcIndex;       // index within 'clients' RC file for this calendar's entry
    bool              mLoaded;        // true if calendar file is currently loaded
    bool              mLoadedConnected; // true if the loaded() signal has been connected to AlarmDaemon
    bool              mUnregistered;  // client has registered, but has not since added the calendar
};

typedef QPtrList<ADCalendarBase> CalendarList;

class ADCalendarBaseFactory
{
  public:
    virtual ADCalendarBase *create(const QString& url, const QCString& appname, ADCalendarBase::Type) = 0;
};


#endif
