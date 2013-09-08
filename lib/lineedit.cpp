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
#include "lineedit.moc"

#include <libkdepim/misc/maillistdrag.h>
#include <kabc/vcarddrag.h>
#ifdef USE_AKONADI
#include <kcalutils/icaldrag.h>
#else
#include <kcal/icaldrag.h>
#endif

#include <kurl.h>
#include <kurlcompletion.h>
#include <kshell.h>

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
    if (mType == Url)
    {
        setCompletionMode(KGlobalSettings::CompletionShell);
        KUrlCompletion* comp = new KUrlCompletion(KUrlCompletion::FileCompletion);
        comp->setReplaceHome(true);
        setCompletionObject(comp);
        setAutoDeleteCompletionObject(true);
    }
    else
        setCompletionMode(KGlobalSettings::CompletionNone);
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
#ifdef USE_AKONADI
    if (KCalUtils::ICalDrag::canDecode(data))
#else
    if (KCal::ICalDrag::canDecode(data))
#endif
        ok = false;   // don't accept "text/calendar" objects
    else
        ok = (data->hasText()
           || KUrl::List::canDecode(data)
           || (mType != Url && KPIM::MailList::canDecode(data))
           || (mType == Emails && KABC::VCardDrag::canDecode(data)));
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
    KUrl::List            files;
    KABC::Addressee::List addrList;

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
    // This must come before KUrl
    else if (mType == Emails
    &&  KABC::VCardDrag::canDecode(data)  &&  KABC::VCardDrag::fromMimeData(data, addrList))
    {
        // KAddressBook entries
        for (KABC::Addressee::List::Iterator it = addrList.begin();  it != addrList.end();  ++it)
        {
            QString em((*it).fullEmail());
            if (!em.isEmpty())
                newEmails.append(em);
        }
    }
    else if (!(files = KUrl::List::fromMimeData(data)).isEmpty())
    {
        // URL(s)
        switch (mType)
        {
            case Url:
                // URL entry field - ignore all but the first dropped URL
                setText(files.first().prettyUrl());    // replace any existing text
                break;
            case Emails:
            {
                // Email entry field - ignore all but mailto: URLs
                QString mailto = QLatin1String("mailto");
                for (KUrl::List::Iterator it = files.begin();  it != files.end();  ++it)
                {
                    if ((*it).protocol() == mailto)
                        newEmails.append((*it).path());
                }
                break;
            }
            case Text:
                newText = files.first().prettyUrl();
                break;
        }
    }
    else if (data->hasText())
    {
        // Plain text
        QString txt = data->text();
        if (mType == Emails)
        {
            // Remove newlines from a list of email addresses, and allow an eventual mailto: protocol
            QString mailto = QLatin1String("mailto:");
            newEmails = txt.split(QRegExp(QLatin1String("[\r\n]+")), QString::SkipEmptyParts);
            for (QStringList::Iterator it = newEmails.begin();  it != newEmails.end();  ++it)
            {
                if ((*it).startsWith(mailto))
                {
                    KUrl url(*it);
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
        newText = newEmails.join(QLatin1String(","));
        int c = cursorPosition();
        if (c > 0)
            newText.prepend(QLatin1String(","));
        if (c < static_cast<int>(text().length()))
            newText.append(QLatin1String(","));
    }
    if (!newText.isEmpty())
        insert(newText);
}

// vim: et sw=4:
