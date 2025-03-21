/*
 *  alarmtext.cpp  -  text/email alarm text conversion
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "alarmtext.h"

#include <KLocalizedString>

#include <QStringList>
#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
using namespace Qt::Literals::StringLiterals;

namespace
{
const int MAIL_FROM_LINE = 0;   // line number containing From in email text
const int MAIL_TO_LINE   = 1;   // line number containing To in email text
const int MAIL_CC_LINE   = 2;   // line number containing CC in email text
const int MAIL_MIN_LINES = 4;   // allow for From, To, no CC, Date, Subject
}

namespace KAlarmCal
{

class Q_DECL_HIDDEN AlarmText::Private
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

    QString          mBody, mFrom, mTo, mCc, mTime, mSubject;
    KAEvent::EmailId mEmailId;   // if email, message's Akonadi item ID, else -1
    Type             mType;
    bool             mIsEmail;
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
        mFromPrefixEn    = QStringLiteral("From:");
        mToPrefixEn      = QStringLiteral("To:");
        mCcPrefixEn      = QStringLiteral("Cc:");
        mDatePrefixEn    = QStringLiteral("Date:");
        mSubjectPrefixEn = QStringLiteral("Subject:");
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

void AlarmText::clear()
{
    d->clear();
}

void AlarmText::setText(const QString& text)
{
    d->clear();
    d->mBody = text;
    if (text.startsWith("#!"_L1))
        d->mType = Private::Script;
}

void AlarmText::setScript(const QString& text)
{
    setText(text);
    d->mType = Private::Script;
}

void AlarmText::setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
                         const QString& subject, const QString& body, KAEvent::EmailId emailId)
{
    d->clear();
    d->mType    = Private::Email;
    d->mTo      = to;
    d->mFrom    = from;
    d->mCc      = cc;
    d->mTime    = time;
    d->mSubject = subject;
    d->mBody    = body;
    d->mEmailId = emailId;
}

void AlarmText::setTodo(const KCalendarCore::Todo::Ptr& todo)
{
    d->clear();
    d->mType    = Private::Todo;
    d->mSubject = todo->summary();
    d->mBody    = todo->description();
    d->mTo      = todo->location();
    if (todo->hasDueDate())
    {
        const QDateTime dueTime = todo->dtDue(false);   // fetch the next due date
        if (todo->hasStartDate()  &&  todo->dtStart(true) != dueTime)
            d->mTime = todo->allDay() ? QLocale().toString(dueTime.date(), QLocale::ShortFormat)
                                      : QLocale().toString(dueTime, QLocale::ShortFormat);
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
            text = mFromPrefix + '\t'_L1 + mFrom + '\n'_L1;
            text += mToPrefix + '\t'_L1 + mTo + '\n'_L1;
            if (!mCc.isEmpty())
                text += mCcPrefix + '\t'_L1 + mCc + '\n'_L1;
            if (!mTime.isEmpty())
                text += mDatePrefix + '\t'_L1 + mTime + '\n'_L1;
            text += mSubjectPrefix + '\t'_L1 + mSubject;
            if (!mBody.isEmpty())
            {
                text += "\n\n"_L1;
                text += mBody;
            }
            break;
        case Todo:
            // Format the todo into a text alarm
            setUpTranslations();
            if (!mSubject.isEmpty())
                text = mTitlePrefix + '\t'_L1 + mSubject + '\n'_L1;
            if (!mTo.isEmpty())
                text += mLocnPrefix + '\t'_L1 + mTo + '\n'_L1;
            if (!mTime.isEmpty())
                text += mDuePrefix + '\t'_L1 + mTime + '\n'_L1;
            if (!mBody.isEmpty())
            {
                if (!text.isEmpty())
                    text += '\n'_L1;
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

KAEvent::EmailId AlarmText::emailId() const
{
    return d->mEmailId;
}

/******************************************************************************
* Return the alarm summary text for either single line or tooltip display.
* The maximum number of line returned is determined by 'maxLines'.
* If 'truncated' is non-null, it will be set true if the text returned has been
* truncated, other than to strip a trailing newline.
*/
QString AlarmText::summary(const KAEvent& event, int maxLines, bool* truncated)
{
    static const QRegularExpression re(QStringLiteral("^file:/+"));
    QString text;
    switch (event.actionSubType())
    {
        case KAEvent::SubAction::Audio:
        {
            text = event.audioFile();
            const QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch())
                text = text.mid(match.capturedEnd(0) - 1);
            break;
        }
        case KAEvent::SubAction::Email:
            text = event.emailSubject();
            break;
        case KAEvent::SubAction::Command:
        {
            text = event.cleanText();
            const QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch())
                text = text.mid(match.capturedEnd(0) - 1);
            break;
        }
        case KAEvent::SubAction::File:
            text = event.cleanText();
            break;
        case KAEvent::SubAction::Message:
        {
            text = event.cleanText();
            // If the message is the text of an email, return its headers or just subject line
            const QString headers = emailHeaders(text, (maxLines <= 1));
            if (!headers.isNull())
            {
                if (truncated)
                    *truncated = true;
                return headers;
            }
            if (maxLines == 1)
            {
                // If the message is the text of a todo, return either the
                // title/description or the whole text.
                const QString title = Private::todoTitle(text);
                if (!title.isEmpty())
                {
                    if (truncated)
                        *truncated = true;
                    return title;
                }
            }
            break;
        }
    }
    if (truncated)
        *truncated = false;
    if (text.count('\n'_L1) < maxLines)
        return text;
    int newline = -1;
    for (int i = 0;  i < maxLines;  ++i)
    {
        newline = text.indexOf('\n'_L1, newline + 1);
        if (newline < 0)
            return text;    // not truncated after all !?!
    }
    if (newline == static_cast<int>(text.length()) - 1)
        return text.left(newline);    // text ends in newline
    if (truncated)
        *truncated = true;
    return text.left(newline + (maxLines <= 1 ? 0 : 1)) + "..."_L1;
}

/******************************************************************************
* Check whether a text is an email.
*/
bool AlarmText::checkIfEmail(const QString& text)
{
    const QStringList lines = text.split('\n'_L1, Qt::SkipEmptyParts);
    return Private::emailHeaderCount(lines);
}

/******************************************************************************
* Check whether a text is an email, and if so return its headers or optionally
* only its subject line.
* Reply = headers/subject line, or QString() if not the text of an email.
*/
QString AlarmText::emailHeaders(const QString& text, bool subjectOnly)
{
    const QStringList lines = text.split('\n'_L1, Qt::SkipEmptyParts);
    const int n = Private::emailHeaderCount(lines);
    if (!n)
        return {};
    if (subjectOnly)
        return lines[n - 1].mid(Private::mSubjectPrefix.length()).trimmed();
    QString h = lines[0];
    for (int i = 1;  i < n;  ++i)
    {
        h += '\n'_L1;
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
    const QStringList lines = text.split('\n'_L1, Qt::SkipEmptyParts);
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
        &&  lines[n + 1].startsWith(Private::mSubjectPrefixEn))
        {
            Private::setUpTranslations();
            QString dispText;
            dispText = Private::mFromPrefix + lines[MAIL_FROM_LINE].mid(Private::mFromPrefixEn.length()) + '\n'_L1;
            dispText += Private::mToPrefix + lines[MAIL_TO_LINE].mid(Private::mToPrefixEn.length()) + '\n'_L1;
            if (n > MAIL_CC_LINE)
                dispText += Private::mCcPrefix + lines[MAIL_CC_LINE].mid(Private::mCcPrefixEn.length()) + '\n'_L1;
            dispText += Private::mDatePrefix + lines[n].mid(Private::mDatePrefixEn.length()) + '\n'_L1;
            dispText += Private::mSubjectPrefix + lines[n + 1].mid(Private::mSubjectPrefixEn.length());
            int i = text.indexOf(Private::mSubjectPrefixEn);
            i = text.indexOf('\n'_L1, i);
            if (i > 0)
                dispText += QStringView(text).mid(i);
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
    const QStringList lines = text.split('\n'_L1, Qt::SkipEmptyParts);
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
        &&  lines[n + 1].startsWith(Private::mSubjectPrefix))
        {
            // Format the email into a text alarm
            QString calText;
            calText = Private::mFromPrefixEn + lines[MAIL_FROM_LINE].mid(Private::mFromPrefix.length()) + '\n'_L1;
            calText += Private::mToPrefixEn + lines[MAIL_TO_LINE].mid(Private::mToPrefix.length()) + '\n'_L1;
            if (n > MAIL_CC_LINE)
                calText += Private::mCcPrefixEn + lines[MAIL_CC_LINE].mid(Private::mCcPrefix.length()) + '\n'_L1;
            calText += Private::mDatePrefixEn + lines[n].mid(Private::mDatePrefix.length()) + '\n'_L1;
            calText += Private::mSubjectPrefixEn + lines[n + 1].mid(Private::mSubjectPrefix.length());
            int i = text.indexOf(Private::mSubjectPrefix);
            i = text.indexOf('\n'_L1, i);
            if (i > 0)
                calText += QStringView(text).mid(i);
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
    mEmailId = -1;
}

/******************************************************************************
* Set up messages used by executeDropEvent() and emailHeaders().
*/
void AlarmText::Private::setUpTranslations()
{
    initialise();
    if (mFromPrefix.isNull())
    {
        mFromPrefix    = i18nc("@info 'From' email address", "From:");
        mToPrefix      = i18nc("@info Email addressee", "To:");
        mCcPrefix      = i18nc("@info Copy-to in email headers", "Cc:");
        mDatePrefix    = i18nc("@info", "Date:");
        mSubjectPrefix = i18nc("@info Email subject", "Subject:");
        // Todo prefixes
        mTitlePrefix   = i18nc("@info Todo calendar item's title field", "To-do:");
        mLocnPrefix    = i18nc("@info Todo calendar item's location field", "Location:");
        mDuePrefix     = i18nc("@info Todo calendar item's due date/time", "Due:");
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
        &&  lines[n + 1].startsWith(mSubjectPrefix))
            return n + 2;
    }
    return 0;
}

/******************************************************************************
* Return the Todo title line, if the text is for a Todo.
*/
QString AlarmText::Private::todoTitle(const QString& text)
{
    setUpTranslations();
    const QStringList lines = text.split('\n'_L1, Qt::SkipEmptyParts);
    int n;
    for (n = 0; n < lines.count() && lines[n].contains('\t'_L1); ++n) {}
    if (!n  ||  n > 3)
        return {};
    QString title;
    int i = 0;
    if (lines[i].startsWith(mTitlePrefix + '\t'_L1))
    {
        title = lines[i].mid(mTitlePrefix.length()).trimmed();
        ++i;
    }
    if (i < n  &&  lines[i].startsWith(mLocnPrefix + '\t'_L1))
        ++i;
    if (i < n  &&  lines[i].startsWith(mDuePrefix + '\t'_L1))
        ++i;
    if (i == n)
    {
        // It's a Todo text
        if (!title.isEmpty())
            return title;
        if (n < lines.count())
            return lines[n];
    }
    return {};
}

} // namespace KAlarmCal

// vim: et sw=4:
