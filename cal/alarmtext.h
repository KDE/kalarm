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
class KAEvent;


class KALARM_CAL_EXPORT AlarmText
{
    public:
        explicit AlarmText(const QString& text = QString());
        AlarmText(const AlarmText& other);
        ~AlarmText();
        AlarmText& operator=(const AlarmText& other);
        void           setText(const QString&);
        void           setScript(const QString& text);
        void           setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
                                const QString& subject, const QString& body, unsigned long kmailSerialNumber = 0);
#ifdef USE_AKONADI
        void           setTodo(const KCalCore::Todo::Ptr&);
#else
        void           setTodo(const KCal::Todo*);
#endif
        /** Return the text for a text message alarm, in display format. */
        QString        displayText() const;
        /** Return the 'To' header parameter. */
        QString        to() const;
        /** Return the 'From' header parameter. */
        QString        from() const;
        /** Return the 'Cc' header parameter. */
        QString        cc() const;
        /** Return the 'Date' header parameter. */
        QString        time() const;
        /** Return the 'Subject' header parameter. */
        QString        subject() const;
        /** Return the email message body.
         *  @return message body, or empty string if not email type.
         */
        QString        body() const;
        // Todo data
        QString        summary() const;
        QString        location() const;
        QString        due() const;
        QString        description() const;

        /** Return whether there is any text. */
        bool           isEmpty() const;
        bool           isEmail() const;
        bool           isScript() const;
        bool           isTodo() const;
        unsigned long  kmailSerialNumber() const;

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
        static bool    checkIfEmail(const QString& text);

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

#endif // ALARMTEXT_H

// vim: et sw=4:
