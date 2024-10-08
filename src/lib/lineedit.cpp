/*
 *  lineedit.cpp  -  Line edit widget with extra drag and drop options
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lineedit.h"

#include "dragdrop.h"

#include <KContacts/VCardDrag>
#include <KCalUtils/ICalDrag>

#include <KUrlCompletion>
#include <KShell>

#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QRegularExpression>
using namespace Qt::Literals::StringLiterals;


/*=============================================================================
= Class LineEdit
= Line edit which accepts drag and drop of text, URLs and/or email addresses.
* It has an option to prevent its contents being selected when it receives
= focus.
=============================================================================*/
LineEdit::LineEdit(Type type, QWidget* parent)
    : KLineEdit(parent)
    , mType(type)
{
    init();
}

LineEdit::LineEdit(QWidget* parent)
    : KLineEdit(parent)
    , mType(Type::Text)
{
    init();
}

void LineEdit::init()
{
    if (mType == Type::Url)
    {
        setCompletionMode(KCompletion::CompletionShell);
        auto comp = new KUrlCompletion(KUrlCompletion::FileCompletion);
        comp->setReplaceHome(true);
        setCompletionObject(comp);
        setAutoDeleteCompletionObject(true);
    }
    else
        setCompletionMode(KCompletion::CompletionNone);
}

/******************************************************************************
*  Called when the line edit receives focus.
*  If 'noSelect' is true, prevent the contents being selected.
*/
void LineEdit::focusInEvent(QFocusEvent* e)
{
    QFocusEvent newe(QEvent::FocusIn, (mNoSelect ? Qt::OtherFocusReason : e->reason()));
    KLineEdit::focusInEvent(&newe);
    mNoSelect = false;
}

QString LineEdit::text() const
{
    if (mType == Type::Url)
        return KShell::tildeExpand(KLineEdit::text());
    return KLineEdit::text();
}

void LineEdit::setText(const QString& text)
{
    KLineEdit::setText(text);
    setCursorPosition(mSetCursorAtEnd ? text.length() : 0);
}

void LineEdit::dragEnterEvent(QDragEnterEvent* e)
{
    const QMimeData* data = e->mimeData();
    bool ok;
    if (KCalUtils::ICalDrag::canDecode(data))
        ok = false;   // don't accept "text/calendar" objects
    else
        ok = (data->hasText()
           || data->hasUrls()
           || (mType == Type::Emails && KContacts::VCardDrag::canDecode(data)));
    if (ok)
        e->acceptProposedAction();
    else
        e->ignore();
}

void LineEdit::dropEvent(QDropEvent* e)
{
    const QMimeData* data = e->mimeData();
    QString               txt;
    QString               newText;
    QStringList           newEmails;
    QList<QUrl>           files;
    KContacts::Addressee::List addrList;

    if (mType == Type::Emails
    &&  KContacts::VCardDrag::canDecode(data)  &&  KContacts::VCardDrag::fromMimeData(data, addrList))
    {
        // KAddressBook entries
        for (KContacts::Addressee::List::Iterator it = addrList.begin();  it != addrList.end();  ++it)
        {
            const QString em((*it).fullEmail());
            if (!em.isEmpty())
                newEmails.append(em);
        }
    }
    else if (!(files = data->urls()).isEmpty())
    {
        // URL(s)
        switch (mType)
        {
            case Type::Url:
                // URL entry field - ignore all but the first dropped URL
                setText(files.first().toDisplayString());    // replace any existing text
                break;
            case Type::Emails:
            {
                // Email entry field - ignore all but mailto: URLs
                const QString mailto = QStringLiteral("mailto");
                for (const QUrl& file : std::as_const(files))
                {
                    if (file.scheme() == mailto)
                        newEmails.append(file.path());
                }
                break;
            }
            case Type::Text:
                newText = files.first().toDisplayString();
                break;
        }
    }
    else if (DragDrop::dropPlainText(data, txt))
    {
        // Plain text
        if (mType == Type::Emails)
        {
            // Remove newlines from a list of email addresses, and allow an eventual mailto: scheme
            const QString mailto = QStringLiteral("mailto:");
            static const QRegularExpression regexp(QStringLiteral("[\r\n]+"));
            newEmails = txt.split(regexp, Qt::SkipEmptyParts);
            for (QStringList::Iterator it = newEmails.begin();  it != newEmails.end();  ++it)
            {
                if ((*it).startsWith(mailto))
                {
                    const QUrl url = QUrl::fromUserInput(*it);
                    *it = url.path();
                }
            }
        }
        else
        {
            const int newline = txt.indexOf('\n'_L1);
            newText = (newline >= 0) ? txt.left(newline) : txt;
        }
    }

    if (!newEmails.isEmpty())
    {
        newText = newEmails.join(','_L1);
        const int c = cursorPosition();
        if (c > 0)
            newText.prepend(','_L1);
        if (c < static_cast<int>(text().length()))
            newText.append(','_L1);
    }
    if (!newText.isEmpty())
        insert(newText);
}

#include "moc_lineedit.cpp"

// vim: et sw=4:
