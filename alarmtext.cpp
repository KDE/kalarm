/*
 *  alarmtext.cpp  -  text/email alarm text conversion
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"
#include <qstringlist.h>
#include <klocale.h>

#include "alarmevent.h"
#include "editdlg.h"
#include "alarmtext.h"


QString AlarmText::mFromPrefix;
QString AlarmText::mToPrefix;
QString AlarmText::mDatePrefix;
QString AlarmText::mSubjectPrefix;
QString AlarmText::mFromPrefixEn    = QString::fromLatin1("From:");
QString AlarmText::mToPrefixEn      = QString::fromLatin1("To:");
QString AlarmText::mDatePrefixEn    = QString::fromLatin1("Date:");
QString AlarmText::mSubjectPrefixEn = QString::fromLatin1("Subject:");


void AlarmText::setText(const QString& text)
{
	mBody     = text;
	mIsScript = text.startsWith(QString::fromLatin1("#!"));
	mIsEmail  = false;
	mTo = mFrom = mTime = mSubject = QString::null;
}

void AlarmText::setEmail(const QString& to, const QString& from, const QString& time, const QString& subject, const QString& body)
{
	mIsScript = false;
	mIsEmail  = true;
	mTo       = to;
	mFrom     = from;
	mTime     = time;
	mSubject  = subject;
	mBody     = body;
}

/******************************************************************************
*  Return the text for a text message alarm, in display format.
*/
QString AlarmText::displayText() const
{
	if (mIsEmail)
	{
		// Format the email into a text alarm
		setUpTranslations();
		QString text;
		text = mFromPrefix + '\t' + mFrom + '\n';
		text += mToPrefix + '\t' + mTo + '\n';
		text += mDatePrefix + '\t' + mTime + '\n';
		text += mSubjectPrefix + '\t' + mSubject;
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
	&&  lines[0].startsWith(mFromPrefix)
	&&  lines[1].startsWith(mToPrefix)
	&&  lines[2].startsWith(mDatePrefix)
	&&  lines[3].startsWith(mSubjectPrefix))
	{
		if (subjectOnly)
			return lines[3].mid(mSubjectPrefix.length()).stripWhiteSpace();
		return lines[0] + '\n' + lines[1] + '\n' + lines[2] + '\n' + lines[3];
	}
	return QString::null;
}

/******************************************************************************
*  Translate an alarm calendar text to a display text.
*  Translation is needed for email texts, since the alarm calendar stores
*  untranslated email prefixes.
*/
QString AlarmText::fromCalendarText(const QString& text)
{
	QStringList lines = QStringList::split('\n', text);
	if (lines.count() >= 4
	&&  lines[0].startsWith(mFromPrefixEn)
	&&  lines[1].startsWith(mToPrefixEn)
	&&  lines[2].startsWith(mDatePrefixEn)
	&&  lines[3].startsWith(mSubjectPrefixEn))
	{
		setUpTranslations();
		QString dispText;
		dispText = mFromPrefix + lines[0].mid(mFromPrefixEn.length()) + '\n';
		dispText += mToPrefix + lines[1].mid(mToPrefixEn.length()) + '\n';
		dispText += mDatePrefix + lines[2].mid(mDatePrefixEn.length()) + '\n';
		dispText += mSubjectPrefix + lines[3].mid(mSubjectPrefixEn.length());
		int i = text.find(mSubjectPrefixEn);
		i = text.find('\n', i);
		if (i > 0)
			dispText += text.mid(i);
		return dispText;
	}
	else
		return text;
}

/******************************************************************************
*  Return the text for a text message alarm, in alarm calendar format.
*  (The prefix strings are untranslated in the calendar.)
*/
QString AlarmText::toCalendarText(const QString& text)
{
	setUpTranslations();
	QStringList lines = QStringList::split('\n', text);
	if (lines.count() >= 4
	&&  lines[0].startsWith(mFromPrefix)
	&&  lines[1].startsWith(mToPrefix)
	&&  lines[2].startsWith(mDatePrefix)
	&&  lines[3].startsWith(mSubjectPrefix))
	{
		// Format the email into a text alarm
		QString calText;
		calText = mFromPrefixEn + lines[0].mid(mFromPrefix.length()) + '\n';
		calText += mToPrefixEn + lines[1].mid(mToPrefix.length()) + '\n';
		calText += mDatePrefixEn + lines[2].mid(mDatePrefix.length()) + '\n';
		calText += mSubjectPrefixEn + lines[3].mid(mSubjectPrefix.length());
		int i = text.find(mSubjectPrefix);
		i = text.find('\n', i);
		if (i > 0)
			calText += text.mid(i);
		return calText;
	}
	return text;
}

/******************************************************************************
*  Set up messages used by executeDropEvent() and emailHeaders().
*/
void AlarmText::setUpTranslations()
{
	if (mFromPrefix.isNull())
	{
		mFromPrefix    = EditAlarmDlg::i18n_EmailFrom();
		mToPrefix      = EditAlarmDlg::i18n_EmailTo();
		mDatePrefix    = i18n("Date:");
		mSubjectPrefix = EditAlarmDlg::i18n_EmailSubject();
	}
}

/******************************************************************************
*  Return the alarm summary text for either single line or tooltip display.
*  The maximum number of line returned is determined by 'maxLines'.
*  If 'truncated' is non-null, it will be set true if the text returned has been
*  truncated, other than to strip a trailing newline.
*/
QString AlarmText::summary(const KAEvent& event, int maxLines, bool* truncated)
{
	QString text = (event.action() == KAEvent::EMAIL) ? event.emailSubject() : event.cleanText();
	if (event.action() == KAEvent::MESSAGE)
	{
		// If the message is the text of an email, return its headers or just subject line
		QString subject = emailHeaders(text, (maxLines <= 1));
		if (!subject.isNull())
		{
			if (truncated)
				*truncated = true;
			return subject;
		}
	}
	if (truncated)
		*truncated = false;
	if (text.contains('\n') < maxLines)
		return text;
	int newline = -1;
	for (int i = 0;  i < maxLines;  ++i)
	{
		newline = text.find('\n', newline + 1);
		if (newline < 0)
			return text;       // not truncated after all !?!
	}
	if (newline == static_cast<int>(text.length()) - 1)
		return text.left(newline);    // text ends in newline
	if (truncated)
		*truncated = true;
	return text.left(newline + (maxLines <= 1 ? 0 : 1)) + QString::fromLatin1("...");
}
