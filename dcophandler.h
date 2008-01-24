/*
 *  dcophandler.h  -  handler for DCOP calls by other applications
 *  Program:  kalarm
 *  Copyright © 2001,2002,2004-2006,2008 by David Jarvie <djarvie@kde.org>
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
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DCOPHANDLER_H
#define DCOPHANDLER_H

#include <qwidget.h>
#include <dcopobject.h>

#include "datetime.h"
#include "kalarmiface.h"


class DcopHandler : public QWidget, virtual public KAlarmIface
{
    public:
	DcopHandler();
	virtual bool cancelEvent(const QString& url,const QString& eventId);
	virtual bool triggerEvent(const QString& url,const QString& eventId);

	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins, const QString& recurrence,
	                             int subRepeatInterval, int subRepeatCount);
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins, int recurType, int recurInterval, int recurCount);
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, const QString& recurrence,
	                          int subRepeatInterval, int subRepeatCount);
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, int recurType, int recurInterval, int recurCount);
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& recurrence, int subRepeatInterval, int subRepeatCount);
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             int recurType, int recurInterval, int recurCount);
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             int recurType, int recurInterval, const QString& endDateTime);
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           const QString& recurrence, int recurInterval, int recurCount);
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           int recurType, int recurInterval, int recurCount);
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           int recurType, int recurInterval, const QString& endDateTime);
	virtual bool edit(const QString& eventID);
	virtual bool editNew(const QString& templateName);

    private:
	static bool scheduleMessage(const QString& message, const DateTime& start, int lateCancel, unsigned flags,
                                const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                const KURL& audioFile, int reminderMins, const KARecurrence&,
	                            int subRepeatInterval = 0, int subRepeatCount = 0);
	static bool scheduleFile(const KURL& file, const DateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                             const KURL& audioFile, int reminderMins, const KARecurrence&,
	                         int subRepeatInterval = 0, int subRepeatCount = 0);
	static bool scheduleCommand(const QString& commandLine, const DateTime& start, int lateCancel, unsigned flags,
                                const KARecurrence&, int subRepeatInterval = 0, int subRepeatCount = 0);
	static bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                              const QString& attachments, const DateTime& start, int lateCancel, unsigned flags,
                              const KARecurrence&, int subRepeatInterval = 0, int subRepeatCount = 0);
	static DateTime  convertStartDateTime(const QString& startDateTime);
	static unsigned  convertStartFlags(const DateTime& start, unsigned flags);
	static QColor    convertBgColour(const QString& bgColor);
	static bool      convertRecurrence(DateTime& start, KARecurrence&, const QString& startDateTime, const QString& icalRecurrence, int& subRepeatInterval);
	static bool      convertRecurrence(DateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, int recurCount);
	static bool      convertRecurrence(DateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, const QString& endDateTime);
	static bool      convertRecurrence(KARecurrence&, const DateTime& start, int recurType, int recurInterval, int recurCount, const QDateTime& end);
};


#ifdef OLD_DCOP
class DcopHandlerOld : public QWidget, public DCOPObject
{
		Q_OBJECT
	public:
		DcopHandlerOld();
		~DcopHandlerOld()  { }
		virtual bool process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray& replyData);
};
#endif

#endif // DCOPHANDLER_H
