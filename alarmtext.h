/*
 *  alarmtext.h  -  text/email alarm text conversion
 *  Program:  kalarm
 *  (C) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef ALARMTEXT_H
#define ALARMTEXT_H

#include <qstring.h>
class KAEvent;


class AlarmText
{
	public:
		AlarmText(const QString& text = QString::null)  { setText(text); }
		void           setText(const QString&);
		void           setScript(const QString& text)   { setText(text);  mIsScript = true; }
		void           setEmail(const QString& to, const QString& from, const QString& time, const QString& subject, const QString& body);
		QString        displayText() const;
		QString        calendarText() const;
		QString        to() const        { return mTo; }
		QString        from() const      { return mFrom; }
		QString        time() const      { return mTime; }
		QString        subject() const   { return mSubject; }
		QString        body() const      { return mIsEmail ? mBody : QString::null; }
		bool           isEmpty() const;
		bool           isEmail() const   { return mIsEmail; }
		bool           isScript() const  { return mIsScript; }
		static QString summary(const KAEvent&, int maxLines = 1, bool* truncated = 0);
		static QString emailHeaders(const QString&, bool subjectOnly);
		static QString fromCalendarText(const QString&);
		static QString toCalendarText(const QString&);

	private:
		static void    setUpTranslations();
		static QString mFromPrefix;       // translated header prefixes
		static QString mToPrefix;
		static QString mDatePrefix;
		static QString mSubjectPrefix;
		static QString mFromPrefixEn;     // untranslated header prefixes
		static QString mToPrefixEn;
		static QString mDatePrefixEn;
		static QString mSubjectPrefixEn;
		QString        mBody, mFrom, mTo, mTime, mSubject;
		bool           mIsEmail;
		bool           mIsScript;
};

#endif // ALARMTEXT_H
