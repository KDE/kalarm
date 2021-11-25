/*
 *  lineedit.h  -  line edit widget with extra drag and drop options
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KLineEdit>

class QDragEnterEvent;
class QFocusEvent;
class QDropEvent;


/**
 *  @short Line edit widget with extra drag and drop options.
 *
 *  The LineEdit class is a line edit widget which accepts specified types of drag and
 *  drop content.
 *
 *  The widget will always accept drag and drop of text, except text/calendar mime type,
 *  and of URLs. It will accept additional mime types depending on its configuration:
 *  Text type accepts email address lists.
 *  Email type accepts email address lists and VCard data (e.g. from KAddressBook).
 *
 *  The class also provides an option to prevent its contents being selected when the
 *  widget receives focus.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class LineEdit : public KLineEdit
{
    Q_OBJECT
public:
    /** Types of drag and drop content which will be accepted.
     *  @li Text   - the line edit contains general text. It accepts text or a
     *               URL. If multiple URLs or emails are dropped, only the
     *               first is used; the rest are ignored.
     *  @li Url    - the line edit contains a URL. It accepts text or a URL. If
     *               multiple URLs are dropped, only the first URL is used; the
     *               rest are ignored.
     *  @li Emails - the line edit contains email addresses. It accepts text,
     *               mailto: URLs, emails from KMail (the From address is used)
     *               or vcard data (e.g. from KAddressBook). If multiple emails
     *               are dropped, only the first is used; the rest are ignored.
     */
    enum Type { Text, Url, Emails };

    /** Constructor.
     *  @param type The content type for the line edit.
     *  @param parent The parent object of this widget.
     */
    explicit LineEdit(Type type, QWidget* parent = nullptr);

    /** Constructs a line edit whose content type is Text.
     *  @param parent The parent object of this widget.
     */
    explicit LineEdit(QWidget* parent = nullptr);

    /** Return the entered text.
     *  If the type is Url, tilde expansion is performed.
     */
    QString text() const;

    /** Prevents the line edit's contents being selected when the widget receives focus. */
    void setNoSelect()   { mNoSelect = true; }

    /** Sets whether the cursor should be set at the beginning or end of the text when
     *  setText() is called.
     */
    void setCursorAtEnd(bool end = true)  { mSetCursorAtEnd = end; }

public Q_SLOTS:
    /** Sets the contents of the line edit to be @p str. */
    void setText(const QString& str) override;

protected:
    void focusInEvent(QFocusEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    void init();

    Type  mType;
    bool  mNoSelect {false};
    bool  mSetCursorAtEnd {false};  // setText() should position cursor at end
};

// vim: et sw=4:
