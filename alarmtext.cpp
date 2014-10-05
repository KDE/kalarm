/*
 *  alarmtext.cpp  -  text/email alarm text conversion
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2004,2005,2007-2013 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "alarmtext.h"

#include "kaevent.h"

#ifdef KALARMCAL_USE_KRESOURCES
#include <kcal/todo.h>
#endif
#include <klocale.h>
#include <klocalizedstring.h>
#include <kglobal.h>
#include <QStringList>

namespace
{
const int MAIL_FROM_LINE = 0;   // line number containing From in email text
const int MAIL_TO_LINE   = 1;   // line number containing To in email text
const int MAIL_CC_LINE   = 2;   // line number containing CC in email text
const int MAIL_MIN_LINES = 4;   // allow for From, To, no CC, Date, Subject
}

namespace KAlarmCal
{

class AlarmText::Private
{
    public:
        enum Type { None, Email, Script, Todo };
        QString        displayText() const;
        void           clear();
        static void    initialise();
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
        static bool    mInitialised;

        QString        mBody, mFrom, mTo, mCc, mTime, mSubject;
        unsigned long  mKMailSerialNum;   // if email, message's KMail serial number, else 0
        Type           mType;
        bool           mIsEmail;
};

QString AlarmText::Private::mFromPrefix;
QString AlarmText::Private::mToPrefix;
QString AlarmText::Private::mCcPrefix;
QString AlarmText::Private::mDatePrefix;
QString AlarmText::Private::mSubjectPrefix;
QString AlarmText::Private::mTitlePrefix;
QString AlarmText::Private::mLocnPrefix;
QString AlarmText::Private::mDuePrefix;
QString AlarmText::Private::mFromPrefixEn;
QString AlarmText::Private::mToPrefixEn;
QString AlarmText::Private::mCcPrefixEn;
QString AlarmText::Private::mDatePrefixEn;
QString AlarmText::Private::mSubjectPrefixEn;
bool    AlarmText::Private::mInitialised = false;

void AlarmText::Private::initialise()
{
    if (!mInitialised)
    {
        mInitialised     = true;
        mFromPrefixEn    = QLatin1String("From:");
        mToPrefixEn      = QLatin1String("To:");
        mCcPrefixEn      = QLatin1String("Cc:");
        mDatePrefixEn    = QLatin1String("Date:");
        mSubjectPrefixEn = QLatin1String("Subject:");
    }
}

AlarmText::AlarmText(const QString& text)
    : d(new Private)
{
    Private::initialise();
    setText(text);
}

AlarmText::AlarmText(const AlarmText& other)
    : d(new Private(*other.d))
{
}

AlarmText::~AlarmText()
{
    delete d;
}

AlarmText& AlarmText::operator=(const AlarmText& other)
{
    if (&other != this)
        *d = *other.d;
    return *this;
}

void AlarmText::setText(const QString& text)
{
    d->clear();
    d->mBody = text;
    if (text.startsWith(QLatin1String("#!")))
        d->mType = Private::Script;
}

void AlarmText::setScript(const QString& text)
{
    setText(text);
    d->mType = Private::Script;
}

void AlarmText::setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
                         const QString& subject, const QString& body, unsigned long kmailSerialNumber)
{
    d->clear();
    d->mType           = Private::Email;
    d->mTo             = to;
    d->mFrom           = from;
    d->mCc             = cc;
    d->mTime           = time;
    d->mSubject        = subject;
    d->mBody           = body;
    d->mKMailSerialNum = kmailSerialNumber;
}

#ifndef KALARMCAL_USE_KRESOURCES
void AlarmText::setTodo(const KCalCore::Todo::Ptr& todo)
#else
void AlarmText::setTodo(const KCal::Todo* todo)
#endif
{
    d->clear();
    d->mType    = Private::Todo;
    d->mSubject = todo->summary();
    d->mBody    = todo->description();
    d->mTo      = todo->location();
    if (todo->hasDueDate())
    {
        KDateTime due = todo->dtDue(false);   // fetch the next due date
        if (todo->hasStartDate()  &&  todo->dtStart() != due)
        {
            d->mTime = todo->allDay() ? KGlobal::locale()->formatDate(due.date(), KLocale::ShortDate)
                                      : KGlobal::locale()->formatDateTime(due.dateTime());
        }
    }
}

/******************************************************************************
* Return the text for a text message alarm, in display format.
*/
QString AlarmText::displayText() const
{
    return d->displayText();
}

QString AlarmText::Private::displayText() const
{
    QString text;
    switch (mType)
    {
        case Email:
            // Format the email into a text alarm
            setUpTranslations();
            text = mFromPrefix + QLatin1Char('\t') + mFrom + QLatin1Char('\n');
            text += mToPrefix + QLatin1Char('\t') + mTo + QLatin1Char('\n');
            if (!mCc.isEmpty())
                text += mCcPrefix + QLatin1Char('\t') + mCc + QLatin1Char('\n');
            if (!mTime.isEmpty())
                text += mDatePrefix + QLatin1Char('\t') + mTime + QLatin1Char('\n');
            text += mSubjectPrefix + QLatin1Char('\t') + mSubject;
            if (!mBody.isEmpty())
            {
                text += QLatin1String("\n\n");
                text += mBody;
            }
            break;
        case Todo:
            // Format the todo into a text alarm
            setUpTranslations();
            if (!mSubject.isEmpty())
                text = mTitlePrefix + QLatin1Char('\t') + mSubject + QLatin1Char('\n');
            if (!mTo.isEmpty())
                text += mLocnPrefix + QLatin1Char('\t') + mTo + QLatin1Char('\n');
            if (!mTime.isEmpty())
                text += mDuePrefix + QLatin1Char('\t') + mTime + QLatin1Char('\n');
            if (!mBody.isEmpty())
            {
                if (!text.isEmpty())
                    text += QLatin1Char('\n');
                text += mBody;
            }
            break;
        default:
            break;
    }
    return !text.isEmpty() ? text : mBody;
}

QString AlarmText::to() const
{
    return (d->mType == Private::Email) ? d->mTo : QString();
}

QString AlarmText::from() const
{
    return (d->mType == Private::Email) ? d->mFrom : QString();
}

QString AlarmText::cc() const
{
    return (d->mType == Private::Email) ? d->mCc : QString();
}

QString AlarmText::time() const
{
    return (d->mType == Private::Email) ? d->mTime : QString();
}

QString AlarmText::subject() const
{
    return (d->mType == Private::Email) ? d->mSubject : QString();
}

QString AlarmText::body() const
{
    return (d->mType == Private::Email) ? d->mBody : QString();
}

QString AlarmText::summary() const
{
    return (d->mType == Private::Todo) ? d->mSubject : QString();
}

QString AlarmText::location() const
{
    return (d->mType == Private::Todo) ? d->mTo : QString();
}

QString AlarmText::due() const
{
    return (d->mType == Private::Todo) ? d->mTime : QString();
}

QString AlarmText::description() const
{
    return (d->mType == Private::Todo) ? d->mBody : QString();
}

/******************************************************************************
* Return whether there is any text.
*/
bool AlarmText::isEmpty() const
{
    if (!d->mBody.isEmpty())
        return false;
    if (d->mType != Private::Email)
        return true;
    return d->mFrom.isEmpty() && d->mTo.isEmpty() && d->mCc.isEmpty() && d->mTime.isEmpty() && d->mSubject.isEmpty();
}

bool AlarmText::isEmail() const
{
    return d->mType == Private::Email;
}

bool AlarmText::isScript() const
{
    return d->mType == Private::Script;
}

bool AlarmText::isTodo() const
{
    return d->mType == Private::Todo;
}

unsigned long AlarmText::kmailSerialNumber() const
{
    return d->mKMailSerialNum;
}

/******************************************************************************
* Return the alarm summary text for either single line or tooltip display.
* The maximum number of line returned is determined by 'maxLines'.
* If 'truncated' is non-null, it will be set true if the text returned has been
* truncated, other than to strip a trailing newline.
*/
QString AlarmText::summary(const KAEvent& event, int maxLines, bool* truncated)
{
    static const QRegExp localfile(QLatin1String("^file:/+"));
    QString text;
    switch (event.actionSubType())
    {
        case KAEvent::AUDIO:
            text = event.audioFile();
            if (localfile.indexIn(text) >= 0)
                text = text.mid(localfile.matchedLength() - 1);
            break;
        case KAEvent::EMAIL:
            text = event.emailSubject();
            break;
        case KAEvent::COMMAND:
            text = event.cleanText();
            if (localfile.indexIn(text) >= 0)
                text = text.mid(localfile.matchedLength() - 1);
            break;
        case KAEvent::FILE:
            text = event.cleanText();
            break;
        case KAEvent::MESSAGE:
        {
            text = event.cleanText();
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
                subject = Private::todoTitle(text);
                if (!subject.isEmpty())
                {
                    if (truncated)
                        *truncated = true;
                    return subject;
                }
            }
            break;
        }
    }
    if (truncated)
        *truncated = false;
    if (text.count(QLatin1Char('\n')) < maxLines)
        return text;
    int newline = -1;
    for (int i = 0;  i < maxLines;  ++i)
    {
        newline = text.indexOf(QLatin1Char('\n'), newline + 1);
        if (newline < 0)
            return text;       // not truncated after all !?!
    }
    if (newline == static_cast<int>(text.length()) - 1)
        return text.left(newline);    // text ends in newline
    if (truncated)
        *truncated = true;
    return text.left(newline + (maxLines <= 1 ? 0 : 1)) + QLatin1String("...");
}

/******************************************************************************
* Check whether a text is an email.
*/
bool AlarmText::checkIfEmail(const QString& text)
{
    const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    return Private::emailHeaderCount(lines);
}

/******************************************************************************
* Check whether a text is an email, and if so return its headers or optionally
* only its subject line.
* Reply = headers/subject line, or QString() if not the text of an email.
*/
QString AlarmText::emailHeaders(const QString& text, bool subjectOnly)
{
    const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    const int n = Private::emailHeaderCount(lines);
    if (!n)
        return QString();
    if (subjectOnly)
        return lines[n-1].mid(Private::mSubjectPrefix.length()).trimmed();
    QString h = lines[0];
    for (int i = 1;  i < n;  ++i)
    {
        h += QLatin1Char('\n');
        h += lines[i];
    }
    return h;
}

/******************************************************************************
* Translate an alarm calendar text to a display text.
* Translation is needed for email texts, since the alarm calendar stores
* untranslated email prefixes.
* 'email' is set to indicate whether it is an email text.
*/
QString AlarmText::fromCalendarText(const QString& text, bool& email)
{
    Private::initialise();
    const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    const int maxn = lines.count();
    if (maxn >= MAIL_MIN_LINES
    &&  lines[MAIL_FROM_LINE].startsWith(Private::mFromPrefixEn)
    &&  lines[MAIL_TO_LINE].startsWith(Private::mToPrefixEn))
    {
        int n = MAIL_CC_LINE;
        if (lines[MAIL_CC_LINE].startsWith(Private::mCcPrefixEn))
            ++n;
        if (maxn > n + 1
        &&  lines[n].startsWith(Private::mDatePrefixEn)
        &&  lines[n+1].startsWith(Private::mSubjectPrefixEn))
        {
            Private::setUpTranslations();
            QString dispText;
            dispText = Private::mFromPrefix + lines[MAIL_FROM_LINE].mid(Private::mFromPrefixEn.length()) + QLatin1Char('\n');
            dispText += Private::mToPrefix + lines[MAIL_TO_LINE].mid(Private::mToPrefixEn.length()) + QLatin1Char('\n');
            if (n > MAIL_CC_LINE)
                dispText += Private::mCcPrefix + lines[MAIL_CC_LINE].mid(Private::mCcPrefixEn.length()) + QLatin1Char('\n');
            dispText += Private::mDatePrefix + lines[n].mid(Private::mDatePrefixEn.length()) + QLatin1Char('\n');
            dispText += Private::mSubjectPrefix + lines[n+1].mid(Private::mSubjectPrefixEn.length());
            int i = text.indexOf(Private::mSubjectPrefixEn);
            i = text.indexOf(QLatin1Char('\n'), i);
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
* Return the text for a text message alarm, in alarm calendar format.
* (The prefix strings are untranslated in the calendar.)
*/
QString AlarmText::toCalendarText(const QString& text)
{
    Private::setUpTranslations();
    const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    const int maxn = lines.count();
    if (maxn >= MAIL_MIN_LINES
    &&  lines[MAIL_FROM_LINE].startsWith(Private::mFromPrefix)
    &&  lines[MAIL_TO_LINE].startsWith(Private::mToPrefix))
    {
        int n = MAIL_CC_LINE;
        if (lines[MAIL_CC_LINE].startsWith(Private::mCcPrefix))
            ++n;
        if (maxn > n + 1
        &&  lines[n].startsWith(Private::mDatePrefix)
        &&  lines[n+1].startsWith(Private::mSubjectPrefix))
        {
            // Format the email into a text alarm
            QString calText;
            calText = Private::mFromPrefixEn + lines[MAIL_FROM_LINE].mid(Private::mFromPrefix.length()) + QLatin1Char('\n');
            calText += Private::mToPrefixEn + lines[MAIL_TO_LINE].mid(Private::mToPrefix.length()) + QLatin1Char('\n');
            if (n > MAIL_CC_LINE)
                calText += Private::mCcPrefixEn + lines[MAIL_CC_LINE].mid(Private::mCcPrefix.length()) + QLatin1Char('\n');
            calText += Private::mDatePrefixEn + lines[n].mid(Private::mDatePrefix.length()) + QLatin1Char('\n');
            calText += Private::mSubjectPrefixEn + lines[n+1].mid(Private::mSubjectPrefix.length());
            int i = text.indexOf(Private::mSubjectPrefix);
            i = text.indexOf(QLatin1Char('\n'), i);
            if (i > 0)
                calText += text.mid(i);
            return calText;
        }
    }
    return text;
}

void AlarmText::Private::clear()
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

/******************************************************************************
* Set up messages used by executeDropEvent() and emailHeaders().
*/
void AlarmText::Private::setUpTranslations()
{
    initialise();
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
* Check whether a text is an email.
* Reply = number of email header lines, or 0 if not an email.
*/
int AlarmText::Private::emailHeaderCount(const QStringList& lines)
{
    setUpTranslations();
    const int maxn = lines.count();
    if (maxn >= MAIL_MIN_LINES
    &&  lines[MAIL_FROM_LINE].startsWith(mFromPrefix)
    &&  lines[MAIL_TO_LINE].startsWith(mToPrefix))
    {
        int n = MAIL_CC_LINE;
        if (lines[MAIL_CC_LINE].startsWith(mCcPrefix))
            ++n;
        if (maxn > n + 1
        &&  lines[n].startsWith(mDatePrefix)
        &&  lines[n+1].startsWith(mSubjectPrefix))
            return n+2;
    }
    return 0;
}

/******************************************************************************
* Return the Todo title line, if the text is for a Todo.
*/
QString AlarmText::Private::todoTitle(const QString& text)
{
    setUpTranslations();
    const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    int n;
    for (n = 0;  n < lines.count() && lines[n].contains(QLatin1Char('\t'));  ++n) ;
    if (!n  ||  n > 3)
        return QString();
    QString title;
    int i = 0;
    if (lines[i].startsWith(mTitlePrefix + QLatin1Char('\t')))
    {
        title = lines[i].mid(mTitlePrefix.length()).trimmed();
        ++i;
    }
    if (i < n  &&  lines[i].startsWith(mLocnPrefix + QLatin1Char('\t')))
        ++i;
    if (i < n  &&  lines[i].startsWith(mDuePrefix + QLatin1Char('\t')))
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

} // namespace KAlarmCal

// vim: et sw=4:
