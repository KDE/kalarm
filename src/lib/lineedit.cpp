/*
 *  lineedit.cpp  -  Line edit widget with extra drag and drop options
 *  Program:  kalarm
 *  Copyright © 2003-2005,2009-2010 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "lineedit.h"

#include <Libkdepim/MaillistDrag>
#include <kcontacts/vcarddrag.h>
#include <KCalUtils/kcalutils/icaldrag.h>

#include <kurlcompletion.h>
#include <kshell.h>

#include <QUrl>
#include <QRegExp>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFocusEvent>


/*=============================================================================
= Class LineEdit
= Line edit which accepts drag and drop of text, URLs and/or email addresses.
* It has an option to prevent its contents being selected when it receives
= focus.
=============================================================================*/
LineEdit::LineEdit(Type type, QWidget* parent)
    : KLineEdit(parent),
      mType(type),
      mNoSelect(false),
      mSetCursorAtEnd(false)
{
    init();
}

LineEdit::LineEdit(QWidget* parent)
    : KLineEdit(parent),
      mType(Text),
      mNoSelect(false),
      mSetCursorAtEnd(false)
{
    init();
}

void LineEdit::init()
{
    setAcceptDrops(false);
    if (mType == Url)
    {
        setCompletionMode(KCompletion::CompletionShell);
        KUrlCompletion* comp = new KUrlCompletion(KUrlCompletion::FileCompletion);
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
    if (mType == Url)
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
           || (mType != Url && KPIM::MailList::canDecode(data))
           || (mType == Emails && KContacts::VCardDrag::canDecode(data)));
    if (ok)
        e->accept(rect());
    else
        e->ignore(rect());
}

void LineEdit::dropEvent(QDropEvent* e)
{
    const QMimeData* data = e->mimeData();
    QString               newText;
    QStringList           newEmails;
    QList<QUrl>           files;
    KContacts::Addressee::List addrList;

    if (mType != Url
    &&  KPIM::MailList::canDecode(data))
    {
        KPIM::MailList mailList = KPIM::MailList::fromMimeData(data);
        // KMail message(s) - ignore all but the first
        if (mailList.count())
        {
            if (mType == Emails)
                newText = mailList.first().from();
            else
                setText(mailList.first().subject());    // replace any existing text
        }
    }
    // This must come before QUrl
    else if (mType == Emails
    &&  KContacts::VCardDrag::canDecode(data)  &&  KContacts::VCardDrag::fromMimeData(data, addrList))
    {
        // KAddressBook entries
        for (KContacts::Addressee::List::Iterator it = addrList.begin();  it != addrList.end();  ++it)
        {
            QString em((*it).fullEmail());
            if (!em.isEmpty())
                newEmails.append(em);
        }
    }
    else if (!(files = data->urls()).isEmpty())
    {
        // URL(s)
        switch (mType)
        {
            case Url:
                // URL entry field - ignore all but the first dropped URL
                setText(files.first().toDisplayString());    // replace any existing text
                break;
            case Emails:
            {
                // Email entry field - ignore all but mailto: URLs
                QString mailto = QStringLiteral("mailto");
                for (QList<QUrl>::Iterator it = files.begin();  it != files.end();  ++it)
                {
                    if ((*it).scheme() == mailto)
                        newEmails.append((*it).path());
                }
                break;
            }
            case Text:
                newText = files.first().toDisplayString();
                break;
        }
    }
    else if (data->hasText())
    {
        // Plain text
        QString txt = data->text();
        if (mType == Emails)
        {
            // Remove newlines from a list of email addresses, and allow an eventual mailto: scheme
            QString mailto = QStringLiteral("mailto:");
            newEmails = txt.split(QRegExp(QLatin1String("[\r\n]+")), QString::SkipEmptyParts);
            for (QStringList::Iterator it = newEmails.begin();  it != newEmails.end();  ++it)
            {
                if ((*it).startsWith(mailto))
                {
                    QUrl url = QUrl::fromUserInput(*it);
                    *it = url.path();
                }
            }
        }
        else
        {
            int newline = txt.indexOf(QLatin1Char('\n'));
            newText = (newline >= 0) ? txt.left(newline) : txt;
        }
    }

    if (newEmails.count())
    {
        newText = newEmails.join(QLatin1Char(','));
        int c = cursorPosition();
        if (c > 0)
            newText.prepend(QLatin1Char(','));
        if (c < static_cast<int>(text().length()))
            newText.append(QLatin1Char(','));
    }
    if (!newText.isEmpty())
        insert(newText);
}

// vim: et sw=4:
