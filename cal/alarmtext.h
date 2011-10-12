/*
 *  alarmtext.h  -  text/email alarm text conversion
 *  Program:  kalarm
 *  Copyright © 2004,2005,2008-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef ALARMTEXT_H
#define ALARMTEXT_H

#include "kalarm_cal_export.h"

#ifdef USE_AKONADI
#include <kcalcore/todo.h>
#else
namespace KCal { class Todo; }
#endif
#include <QString>

class QStringList;

namespace KAlarm
{

class KAEvent;

/**
 * @short Parses email, todo and script alarm texts.
 *
 * Parses email, todo and script texts, enabling drag and drop of these items
 * to be recognised and interpreted.
 *
 * - Email texts should contain headers (To, From, etc.) in normal RFC format.
 * - Todos should be in iCalendar format.
 * - Scripts are assumed if the alarm text starts with '#!'.
 *
 * @author David Jarvie <djarvie@kde.org>
 */

class KALARM_CAL_EXPORT AlarmText
{
    public:
        explicit AlarmText(const QString& text = QString());
        AlarmText(const AlarmText& other);
        ~AlarmText();
        AlarmText& operator=(const AlarmText& other);

        /** Set the alarm text.
         *  If @p text starts with '#!', it is flagged as a script, else plain text.
         */
        void setText(const QString& text);
        /** Set the instance contents to be a script. */
        void setScript(const QString& text);
        /** Set the instance contents to be an email. */
        void setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
                      const QString& subject, const QString& body, unsigned long kmailSerialNumber = 0);
#ifdef USE_AKONADI
        /** Set the instance contents to be a todo. */
        void setTodo(const KCalCore::Todo::Ptr& todo);
#else
        /** Set the instance contents to be a todo. */
        void setTodo(const KCal::Todo* todo);
#endif

        /** Return the text for a text message alarm, in display format.
         *  - An email is returned as a sequence of headers followed by the message body.
         *  - A todo is returned as a subject, location and due date followed by any text.
         *  - A script or plain text is returned without interpretation.
         */
        QString displayText() const;
        /** Return the 'To' header parameter for an email alarm.
         *  @return 'from' value, or empty if not an email text.
         */
        QString to() const;
        /** Return the 'From' header parameter for an email alarm.
         *  @return 'from' value, or empty if not an email text.
         */
        QString from() const;
        /** Return the 'Cc' header parameter for an email alarm.
         *  @return 'cc' value, or empty if not an email text.
         */
        QString cc() const;
        /** Return the 'Date' header parameter for an email alarm.
         *  @return 'date' value, or empty if not an email text.
         */
        QString time() const;
        /** Return the 'Subject' header parameter for an email alarm.
         *  @return 'subject' value, or empty if not an email text.
         */
        QString subject() const;
        /** Return the email message body.
         *  @return message body, or empty if not an email text.
         */
        QString body() const;

        /** Return the summary text for a todo.
         *  @return summary text, or empty if not a todo.
         */
        QString summary() const;
        /** Return the location text for a todo.
         *  @return location text, or empty if not a todo.
         */
        QString location() const;
        /** Return the due date text for a todo.
         *  @return due date text, or empty if not a todo.
         */
        QString due() const;
        /** Return the description text for a todo.
         *  @return description text, or empty if not a todo.
         */
        QString description() const;

        /** Return whether there is any text. */
        bool isEmpty() const;
        /** Return whether the instance contains the text of an email. */
        bool isEmail() const;
        /** Return whether the instance contains the text of a script. */
        bool isScript() const;
        /** Return whether the instance contains the text of a todo. */
        bool isTodo() const;

        /** Return the kmail serial number of an email.
         *  @return serial number, or 0 if none.
         */
        unsigned long kmailSerialNumber() const;

        /** Return the alarm summary text for either single line or tooltip display.
         *  @param event      event whose summary text is to be returned
         *  @param maxLines   the maximum number of lines returned
         *  @param truncated  if non-null, points to a variable which will be set true
         *                    if the text returned has been truncated, other than to
         *                    strip a trailing newline, or false otherwise.
         */
        static QString summary(const KAEvent& event, int maxLines = 1, bool* truncated = 0);

        /** Return whether a text is an email, with at least To and From headers.
         *  @param text  text to check.
         */
        static bool checkIfEmail(const QString& text);

        /** Check whether a text is an email (with at least To and From headers),
         *  and if so return its headers or optionally only its subject line.
         *  @param text         text to check
         *  @param subjectOnly  true to only return the subject line,
         *                      false to return all headers.
         *  @return headers/subject line, or QString() if not the text of an email.
         */
        static QString emailHeaders(const QString& text, bool subjectOnly);

        /** Translate an alarm calendar text to a display text.
         *  Translation is needed for email texts, since the alarm calendar stores
         *  untranslated email prefixes.
         *  @param text   text to translate
         *  @param email  updated to indicate whether it is an email text.
         */
        static QString fromCalendarText(const QString& text, bool& email);

        /** Return the text for an alarm message text, in alarm calendar format.
         *  (The prefix strings are untranslated in the calendar.)
         *  @param text  alarm message text.
         */
        static QString toCalendarText(const QString& text);

    private:
        //@cond PRIVATE
        class Private;
        Private* const d;
        //@endcond
};

} // namespace KAlarm

#endif // ALARMTEXT_H

// vim: et sw=4:
