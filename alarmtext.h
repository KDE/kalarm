/*
 *  alarmtext.h  -  text/email alarm text conversion
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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


class AlarmText
{
	public:
		AlarmText(const QString& text = QString::null) : mBody(text), mIsEmail(false) { }
		void           setText(const QString& text);
		void           setEmail(const QString& to, const QString& from, const QString& time, const QString& subject, const QString& body);
		QString        text() const;
		QString        to() const        { return mTo; }
		QString        from() const      { return mFrom; }
		QString        time() const      { return mTime; }
		QString        subject() const   { return mSubject; }
		QString        body() const      { return mIsEmail ? mBody : QString::null; }
		bool           isEmpty() const;
		bool           isEmail() const   { return mIsEmail; }
		static QString emailHeaders(const QString& text, bool subjectOnly);

	private:
		static void    setUpTranslations();
		static QString mMessageFromPrefix;
		static QString mMessageToPrefix;
		static QString mMessageDatePrefix;
		static QString mMessageSubjectPrefix;
		QString        mBody, mFrom, mTo, mTime, mSubject;
		bool           mIsEmail;
};

#endif // ALARMTEXT_H
