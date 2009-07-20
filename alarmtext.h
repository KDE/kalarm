/*
 *  alarmtext.h  -  text/email alarm text conversion
 *  Program:  kalarm
 *  Copyright © 2004,2005,2008,2009 by David Jarvie <djarvie@kde.org>
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

#include <QString>
class QStringList;
class KAEventData;
namespace KCal { class Todo; }


class AlarmText
{
	public:
		AlarmText(const QString& text = QString())  { setText(text); }
		void           setText(const QString&);
		void           setScript(const QString& text)   { setText(text);  mType = Script; }
		void           setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
		                        const QString& subject, const QString& body, unsigned long kmailSerialNumber = 0);
		void           setTodo(const KCal::Todo*);
		QString        displayText() const;
		QString        calendarText() const;
		QString        to() const                 { return mTo; }
		QString        from() const               { return mFrom; }
		QString        cc() const                 { return mCc; }
		QString        time() const               { return mTime; }
		QString        subject() const            { return mSubject; }
		QString        body() const               { return mType == Email ? mBody : QString(); }
		// Todo data
		QString        summary() const            { return mSubject; }
		QString        location() const           { return mTo; }
		QString        due() const                { return mTime; }
		QString        description() const        { return mBody; }
		bool           isEmpty() const;
		bool           isEmail() const            { return mType == Email; }
		bool           isScript() const           { return mType == Script; }
		bool           isTodo() const             { return mType == Todo; }
		unsigned long  kmailSerialNumber() const  { return mKMailSerialNum; }
		static QString summary(const KAEventData*, int maxLines = 1, bool* truncated = 0);
		static bool    checkIfEmail(const QString&);
		static QString emailHeaders(const QString&, bool subjectOnly);
		static QString fromCalendarText(const QString&, bool& email);
		static QString toCalendarText(const QString&);

	private:
		enum Type { None, Email, Script, Todo };
		void           clear();
		static void    setUpTranslations();
		static int     emailHeaderCount(const QStringList&);
		static QString todoTitle(const QString& text);

		static QString mFromPrefix;       // translated header prefixes
		static QString mToPrefix;
		static QString mCcPrefix;
		static QString mDatePrefix;
		static QString mSubjectPrefix;
		static QString mTitlePrefix;
		static QString mLocnPrefix;
		static QString mDuePrefix;
		static QString mFromPrefixEn;     // untranslated header prefixes
		static QString mToPrefixEn;
		static QString mCcPrefixEn;
		static QString mDatePrefixEn;
		static QString mSubjectPrefixEn;
		QString        mBody, mFrom, mTo, mCc, mTime, mSubject;
		unsigned long  mKMailSerialNum;   // if email, message's KMail serial number, else 0
		Type           mType;
		bool           mIsEmail;
};

#endif // ALARMTEXT_H
