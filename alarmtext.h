/*
 *  alarmtext.h  -  text/email alarm text conversion
 *  Program:  kalarm
 *  Copyright (C) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ALARMTEXT_H
#define ALARMTEXT_H

#include <qstring.h>
class QStringList;
class KAEvent;


class AlarmText
{
	public:
		AlarmText(const QString& text = QString::null)  { setText(text); }
		void           setText(const QString&);
		void           setScript(const QString& text)   { setText(text);  mIsScript = true; }
		void           setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
		                        const QString& subject, const QString& body, unsigned long kmailSerialNumber = 0);
		QString        displayText() const;
		QString        calendarText() const;
		QString        to() const                 { return mTo; }
		QString        from() const               { return mFrom; }
		QString        cc() const                 { return mCc; }
		QString        time() const               { return mTime; }
		QString        subject() const            { return mSubject; }
		QString        body() const               { return mIsEmail ? mBody : QString::null; }
		bool           isEmpty() const;
		bool           isEmail() const            { return mIsEmail; }
		bool           isScript() const           { return mIsScript; }
		unsigned long  kmailSerialNumber() const  { return mKMailSerialNum; }
		static QString summary(const KAEvent&, int maxLines = 1, bool* truncated = 0);
		static bool    checkIfEmail(const QString&);
		static QString emailHeaders(const QString&, bool subjectOnly);
		static QString fromCalendarText(const QString&, bool& email);
		static QString toCalendarText(const QString&);

	private:
		static void    setUpTranslations();
		static int     emailHeaderCount(const QStringList&);

		static QString mFromPrefix;       // translated header prefixes
		static QString mToPrefix;
		static QString mCcPrefix;
		static QString mDatePrefix;
		static QString mSubjectPrefix;
		static QString mFromPrefixEn;     // untranslated header prefixes
		static QString mToPrefixEn;
		static QString mCcPrefixEn;
		static QString mDatePrefixEn;
		static QString mSubjectPrefixEn;
		QString        mBody, mFrom, mTo, mCc, mTime, mSubject;
		unsigned long  mKMailSerialNum;   // if email, message's KMail serial number, else 0
		bool           mIsEmail;
		bool           mIsScript;
};

#endif // ALARMTEXT_H
