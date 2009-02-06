/*
 *  alarmtext.cpp  -  text/email alarm text conversion
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "alarmtext.h"

#include "alarmevent.h"

#include <kcal/todo.h>
#include <klocale.h>
#include <kglobal.h>
#include <QStringList>


QString AlarmText::mFromPrefix;
QString AlarmText::mToPrefix;
QString AlarmText::mCcPrefix;
QString AlarmText::mDatePrefix;
QString AlarmText::mSubjectPrefix;
QString AlarmText::mTitlePrefix;
QString AlarmText::mLocnPrefix;
QString AlarmText::mDuePrefix;
QString AlarmText::mFromPrefixEn    = QLatin1String("From:");
QString AlarmText::mToPrefixEn      = QLatin1String("To:");
QString AlarmText::mCcPrefixEn      = QLatin1String("Cc:");
QString AlarmText::mDatePrefixEn    = QLatin1String("Date:");
QString AlarmText::mSubjectPrefixEn = QLatin1String("Subject:");


void AlarmText::clear()
{
	mType = None;
	mBody.clear();
	mTo.clear();
	mFrom.clear();
	mCc.clear();
	mTime.clear();
	mSubject.clear();
	mKMailSerialNum = 0;
}

void AlarmText::setText(const QString& text)
{
	clear();
	mBody = text;
	if (text.startsWith(QLatin1String("#!")))
		mType = Script;
}

void AlarmText::setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
                         const QString& subject, const QString& body, unsigned long kmailSerialNumber)
{
	clear();
	mType           = Email;
	mTo             = to;
	mFrom           = from;
	mCc             = cc;
	mTime           = time;
	mSubject        = subject;
	mBody           = body;
	mKMailSerialNum = kmailSerialNumber;
}

void AlarmText::setTodo(const KCal::Todo* todo)
{
	clear();
	mType    = Todo;
	mSubject = todo->summary();
	mBody    = todo->description();
	mTo      = todo->location();
	if (todo->hasDueDate())
	{
		KDateTime due = todo->dtDue(false);   // fetch the next due date
		if (todo->hasStartDate()  &&  todo->dtStart() != due)
		{
			mTime = todo->allDay() ? KGlobal::locale()->formatDate(due.date(), KLocale::ShortDate)
			                       : KGlobal::locale()->formatDateTime(due.dateTime());
		}
	}
}

/******************************************************************************
*  Return the text for a text message alarm, in display format.
*/
QString AlarmText::displayText() const
{
	QString text;
	switch (mType)
	{
		case Email:
			// Format the email into a text alarm
			setUpTranslations();
			text = mFromPrefix + '\t' + mFrom + '\n';
			text += mToPrefix + '\t' + mTo + '\n';
			if (!mCc.isEmpty())
				text += mCcPrefix + '\t' + mCc + '\n';
			if (!mTime.isEmpty())
				text += mDatePrefix + '\t' + mTime + '\n';
			text += mSubjectPrefix + '\t' + mSubject;
			if (!mBody.isEmpty())
			{
				text += "\n\n";
				text += mBody;
			}
			break;
		case Todo:
			// Format the todo into a text alarm
			setUpTranslations();
			if (!mSubject.isEmpty())
				text = mTitlePrefix + '\t' + mSubject + '\n';
			if (!mTo.isEmpty())
				text += mLocnPrefix + '\t' + mTo + '\n';
			if (!mTime.isEmpty())
				text += mDuePrefix + '\t' + mTime + '\n';
			if (!mBody.isEmpty())
			{
				if (!text.isEmpty())
					text += '\n';
				text += mBody;
			}
			break;
		default:
			text = mBody;
			break;
	}
	return text;
}

/******************************************************************************
*  Return whether there is any text.
*/
bool AlarmText::isEmpty() const
{
	if (!mBody.isEmpty())
		return false;
	if (mType != Email)
		return true;
	return mFrom.isEmpty() && mTo.isEmpty() && mCc.isEmpty() && mTime.isEmpty() && mSubject.isEmpty();
}

/******************************************************************************
*  Check whether a text is an email.
*/
bool AlarmText::checkIfEmail(const QString& text)
{
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	return emailHeaderCount(lines);
}

/******************************************************************************
*  Check whether a text is an email.
*  Reply = number of email header lines, or 0 if not an email.
*/
int AlarmText::emailHeaderCount(const QStringList& lines)
{
	setUpTranslations();
	int maxn = lines.count();
	if (maxn >= 4
	&&  lines[0].startsWith(mFromPrefix)
	&&  lines[1].startsWith(mToPrefix))
	{
		int n = 2;
		if (lines[2].startsWith(mCcPrefix))
			++n;
		if (maxn > n + 1
		&&  lines[n].startsWith(mDatePrefix)
		&&  lines[n+1].startsWith(mSubjectPrefix))
			return n+2;
	}
	return 0;
}

/******************************************************************************
*  Check whether a text is an email, and if so return its headers or optionally
*  only its subject line.
*  Reply = headers/subject line, or QString() if not the text of an email.
*/
QString AlarmText::emailHeaders(const QString& text, bool subjectOnly)
{
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	int n = emailHeaderCount(lines);
	if (!n)
		return QString();
	if (subjectOnly)
		return lines[n-1].mid(mSubjectPrefix.length()).trimmed();
	QString h = lines[0];
	for (int i = 1;  i < n;  ++i)
	{
		h += '\n';
		h += lines[i];
	}
	return h;
}

/******************************************************************************
* Return the Todo title line, if the text is for a Todo.
*/
QString AlarmText::todoTitle(const QString& text)
{
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	int n;
	for (n = 0;  n < lines.count() && lines[n].contains('\t');  ++n) ;
	if (!n  ||  n > 3)
		return QString();
	QString title;
	int i = 0;
	if (lines[i].startsWith(mTitlePrefix + '\t'))
	{
		title = lines[i].mid(mTitlePrefix.length()).trimmed();
		++i;
	}
	if (i < n  &&  lines[i].startsWith(mLocnPrefix + '\t'))
		++i;
	if (i < n  &&  lines[i].startsWith(mDuePrefix + '\t'))
		++i;
	if (i == n)
	{
		// It's a Todo text
		if (!title.isEmpty())
			return title;
		if (n < lines.count())
			return lines[n];
	}
	return QString();
}

/******************************************************************************
*  Translate an alarm calendar text to a display text.
*  Translation is needed for email texts, since the alarm calendar stores
*  untranslated email prefixes.
*  'email' is set to indicate whether it is an email text.
*/
QString AlarmText::fromCalendarText(const QString& text, bool& email)
{
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	int maxn = lines.count();
	if (maxn >= 4
	&&  lines[0].startsWith(mFromPrefixEn)
	&&  lines[1].startsWith(mToPrefixEn))
	{
		int n = 2;
		if (lines[2].startsWith(mCcPrefixEn))
			++n;
		if (maxn > n + 1
		&&  lines[n].startsWith(mDatePrefixEn)
		&&  lines[n+1].startsWith(mSubjectPrefixEn))
		{
			setUpTranslations();
			QString dispText;
			dispText = mFromPrefix + lines[0].mid(mFromPrefixEn.length()) + '\n';
			dispText += mToPrefix + lines[1].mid(mToPrefixEn.length()) + '\n';
			if (n == 3)
				dispText += mCcPrefix + lines[2].mid(mCcPrefixEn.length()) + '\n';
			dispText += mDatePrefix + lines[n].mid(mDatePrefixEn.length()) + '\n';
			dispText += mSubjectPrefix + lines[n+1].mid(mSubjectPrefixEn.length());
			int i = text.indexOf(mSubjectPrefixEn);
			i = text.indexOf('\n', i);
			if (i > 0)
				dispText += text.mid(i);
			email = true;
			return dispText;
		}
	}
	email = false;
	return text;
}

/******************************************************************************
*  Return the text for a text message alarm, in alarm calendar format.
*  (The prefix strings are untranslated in the calendar.)
*/
QString AlarmText::toCalendarText(const QString& text)
{
	setUpTranslations();
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	int maxn = lines.count();
	if (maxn >= 4
	&&  lines[0].startsWith(mFromPrefix)
	&&  lines[1].startsWith(mToPrefix))
	{
		int n = 2;
		if (lines[2].startsWith(mCcPrefix))
			++n;
		if (maxn > n + 1
		&&  lines[n].startsWith(mDatePrefix)
		&&  lines[n+1].startsWith(mSubjectPrefix))
		{
			// Format the email into a text alarm
			QString calText;
			calText = mFromPrefixEn + lines[0].mid(mFromPrefix.length()) + '\n';
			calText += mToPrefixEn + lines[1].mid(mToPrefix.length()) + '\n';
			if (n == 3)
				calText += mCcPrefixEn + lines[2].mid(mCcPrefix.length()) + '\n';
			calText += mDatePrefixEn + lines[n].mid(mDatePrefix.length()) + '\n';
			calText += mSubjectPrefixEn + lines[n+1].mid(mSubjectPrefix.length());
			int i = text.indexOf(mSubjectPrefix);
			i = text.indexOf('\n', i);
			if (i > 0)
				calText += text.mid(i);
			return calText;
		}
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
		mFromPrefix    = i18nc("@info/plain 'From' email address", "From:");
		mToPrefix      = i18nc("@info/plain Email addressee", "To:");
		mCcPrefix      = i18nc("@info/plain Copy-to in email headers", "Cc:");
		mDatePrefix    = i18nc("@info/plain", "Date:");
		mSubjectPrefix = i18nc("@info/plain Email subject", "Subject:");
		// Todo prefixes
		mTitlePrefix   = i18nc("@info/plain Todo calendar item's title field", "To-do:");
		mLocnPrefix    = i18nc("@info/plain Todo calendar item's location field", "Location:");
		mDuePrefix     = i18nc("@info/plain Todo calendar item's due date/time", "Due:");
	}
}

/******************************************************************************
*  Return the alarm summary text for either single line or tooltip display.
*  The maximum number of line returned is determined by 'maxLines'.
*  If 'truncated' is non-null, it will be set true if the text returned has been
*  truncated, other than to strip a trailing newline.
*/
QString AlarmText::summary(const KAEvent* event, int maxLines, bool* truncated)
{
	QString text = (event->action() == KAEvent::EMAIL) ? event->emailSubject() : event->cleanText();
	if (event->action() == KAEvent::MESSAGE)
	{
		// If the message is the text of an email, return its headers or just subject line
		QString subject = emailHeaders(text, (maxLines <= 1));
		if (!subject.isNull())
		{
			if (truncated)
				*truncated = true;
			return subject;
		}
		if (maxLines == 1)
		{
			// If the message is the text of a todo, return either the
			// title/description or the whole text.
			subject = todoTitle(text);
			if (!subject.isEmpty())
			{
				if (truncated)
					*truncated = true;
				return subject;
			}
		}
	}
	if (truncated)
		*truncated = false;
	if (text.count('\n') < maxLines)
		return text;
	int newline = -1;
	for (int i = 0;  i < maxLines;  ++i)
	{
		newline = text.indexOf('\n', newline + 1);
		if (newline < 0)
			return text;       // not truncated after all !?!
	}
	if (newline == static_cast<int>(text.length()) - 1)
		return text.left(newline);    // text ends in newline
	if (truncated)
		*truncated = true;
	return text.left(newline + (maxLines <= 1 ? 0 : 1)) + QLatin1String("...");
}
