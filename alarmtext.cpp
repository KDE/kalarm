/*
 *  alarmtext.cpp  -  text/email alarm text conversion
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"
#include <qstringlist.h>
#include <klocale.h>
#include "alarmtext.h"


QString AlarmText::mMessageFromPrefix;
QString AlarmText::mMessageToPrefix;
QString AlarmText::mMessageDatePrefix;
QString AlarmText::mMessageSubjectPrefix;


void AlarmText::setText(const QString& text)
{
	mIsEmail = false;
	mBody    = text;
	mTo = mFrom = mTime = mSubject = QString::null;
}

void AlarmText::setEmail(const QString& to, const QString& from, const QString& time, const QString& subject, const QString& body)
{
	mIsEmail = true;
	mTo      = to;
	mFrom    = from;
	mTime    = time;
	mSubject = subject;
	mBody    = body;
}

/******************************************************************************
*  Return the text for a text message alarm.
*/
QString AlarmText::text() const
{
	if (mIsEmail)
	{
		// Format the email into a text alarm
		setUpTranslations();
		QString text;
		text = mMessageFromPrefix + '\t';
		text += mFrom;
		text += '\n';
		text += mMessageToPrefix + '\t';
		text += mTo;
		text += '\n';
		text += mMessageDatePrefix + '\t';
		text += mTime;
		text += '\n';
		text += mMessageSubjectPrefix + '\t';
		text += mSubject;
		if (!mBody.isEmpty())
		{
			text += "\n\n";
			text += mBody;
		}
		return text;
	}
	return mBody;
}

/******************************************************************************
*  Return whether there is any text.
*/
bool AlarmText::isEmpty() const
{
	if (!mBody.isEmpty())
		return false;
	if (!mIsEmail)
		return true;
	return mFrom.isEmpty() && mTo.isEmpty() && mTime.isEmpty() && mSubject.isEmpty();
}

/******************************************************************************
*  Check whether a text is an email, and if so return its headers or optionally
*  only its subject line.
*  Reply = headers/subject line, or QString::null if not the text of an email.
*/
QString AlarmText::emailHeaders(const QString& text, bool subjectOnly)
{
	setUpTranslations();
	QStringList lines = QStringList::split('\n', text);
	if (lines.count() >= 4
	&&  lines[0].startsWith(mMessageFromPrefix)
	&&  lines[1].startsWith(mMessageToPrefix)
	&&  lines[2].startsWith(mMessageDatePrefix)
	&&  lines[3].startsWith(mMessageSubjectPrefix))
	{
		if (subjectOnly)
			return lines[3].mid(mMessageSubjectPrefix.length());
		return lines[0] + '\n' + lines[1] + '\n' + lines[2] + '\n' + lines[3];
	}
	return QString::null;
}

/******************************************************************************
*  Set up messages used by executeDropEvent() and emailHeaders().
*/
void AlarmText::setUpTranslations()
{
	if (mMessageFromPrefix.isNull())
	{
		mMessageFromPrefix    = i18n("'From' email address", "From:");
		mMessageToPrefix      = i18n("Email addressee", "To:");
		mMessageDatePrefix    = i18n("Date:");
		mMessageSubjectPrefix = i18n("Email subject", "Subject:");
	}
}
