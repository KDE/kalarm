/*
 *  alarmtext.h  -  text/email alarm text conversion
 *  This file is part of kalarmprivate library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcal_export.h"

#include <Akonadi/Item>
#include <KCalendarCore/Todo>
#include <QString>

namespace KAlarmCal
{

class KAEvent;

/**
 * @short Parses email, todo and script alarm texts.
 *
 * This class parses email, todo and script texts, enabling drag and drop of
 * these items to be recognised and interpreted. It also holds plain alarm
 * texts.
 *
 * - Email texts must contain headers (To, From, etc.) in normal RFC format.
 * - Todos should be in iCalendar format.
 * - Scripts are assumed if the alarm text starts with '#!'.
 *
 * @author David Jarvie <djarvie@kde.org>
 */

class KALARMCAL_EXPORT AlarmText
{
public:
    /** Constructor which sets the alarm text.
     *  If @p text starts with '#!', it is flagged as a script, else plain text.
     *  @param text alarm text to set
     */
    explicit AlarmText(const QString& text = QString());

    AlarmText(const AlarmText& other);
    ~AlarmText();
    AlarmText& operator=(const AlarmText& other);

    /** Initialise the instance to an empty state. */
    void clear();

    /** Set the alarm text.
     *  If @p text starts with '#!', it is flagged as a script, else plain text.
     *  @param text alarm text to set
     */
    void setText(const QString& text);

    /** Set the instance contents to be a script.
     *  @param text text of script to set
     */
    void setScript(const QString& text);

    /** Set the instance contents to be an email.
     *  @param to       'To' header parameter
     *  @param from     'From' header parameter
     *  @param cc       'Cc' header parameter
     *  @param time     'Date' header parameter
     *  @param subject  'Subject' header parameter
     *  @param body     email body text
     *  @param itemId   Akonadi item ID of the email.
     */
    void setEmail(const QString& to, const QString& from, const QString& cc, const QString& time,
                  const QString& subject, const QString& body, Akonadi::Item::Id itemId = -1);

    /** Set the instance contents to be a todo.
     *  @param todo Todo instance to set as the text
     */
    void setTodo(const KCalendarCore::Todo::Ptr& todo);

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

    /** Return whether the instance has any contents. */
    bool isEmpty() const;

    /** Return whether the instance contains the text of an email. */
    bool isEmail() const;

    /** Return whether the instance contains the text of a script. */
    bool isScript() const;

    /** Return whether the instance contains the text of a todo. */
    bool isTodo() const;

    /** Return the Akonadi item ID of an email.
     *  @return Item ID, or -1 if none.
     */
    Akonadi::Item::Id akonadiItemId() const;

    /** Return the alarm summary text for either single line or tooltip display.
     *  @param event      event whose summary text is to be returned
     *  @param maxLines   the maximum number of lines returned
     *  @param truncated  if non-null, points to a variable which will be set true
     *                    if the text returned has been truncated, other than to
     *                    strip a trailing newline, or false otherwise
     */
    static QString summary(const KAEvent& event, int maxLines = 1, bool* truncated = nullptr);

    /** Return whether a text is an email, with at least To and From headers.
     *  @param text  text to check
     */
    static bool checkIfEmail(const QString& text);

    /** Check whether a text is an email (with at least To and From headers),
     *  and if so return its headers or optionally only its subject line.
     *  @param text         text to check
     *  @param subjectOnly  true to only return the subject line,
     *                      false to return all headers
     *  @return headers/subject line, or QString() if not the text of an email.
     */
    static QString emailHeaders(const QString& text, bool subjectOnly);

    /** Translate an alarm calendar text to a display text.
     *  Translation is needed for email texts, since the alarm calendar stores
     *  untranslated email prefixes.
     *  @param text   text to translate
     *  @param email  updated to indicate whether it is an email text
     */
    static QString fromCalendarText(const QString& text, bool& email);

    /** Return the text for an alarm message text, in alarm calendar format.
     *  (The prefix strings are untranslated in the calendar.)
     *  @param text  alarm message text
     */
    static QString toCalendarText(const QString& text);

private:
    //@cond PRIVATE
    class Private;
    Private* const d;
    //@endcond
};

} // namespace KAlarmCal

// vim: et sw=4:
