/*
 *  lineedit.cpp  -  Line edit widget with extra drag and drop options
 *  Program:  kalarm
 *  (C) 2003 - 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"

#include <qregexp.h>
#include <qdragobject.h>

#include <kurldrag.h>
#include <kurlcompletion.h>

#include <libkdepim/maillistdrag.h>
#include <libkdepim/kvcarddrag.h>
#include <libkcal/icaldrag.h>

#include "lineedit.moc"


/*=============================================================================
= Class LineEdit
= Line edit which accepts drag and drop of text, URLs and/or email addresses.
* It has an option to prevent its contents being selected when it receives
= focus.
=============================================================================*/
LineEdit::LineEdit(Type type, QWidget* parent, const char* name)
	: KLineEdit(parent, name),
	  mType(type),
	  mNoSelect(false),
	  mSetCursorAtEnd(false)
{
	init();
}

LineEdit::LineEdit(QWidget* parent, const char* name)
	: KLineEdit(parent, name),
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
		KURLCompletion* comp = new KURLCompletion(KURLCompletion::FileCompletion);
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
	if (mNoSelect)
		QFocusEvent::setReason(QFocusEvent::Other);
	KLineEdit::focusInEvent(e);
	if (mNoSelect)
	{
		QFocusEvent::resetReason();
		mNoSelect = false;
	}
}

void LineEdit::setText(const QString& text)
{
	KLineEdit::setText(text);
	setCursorPosition(mSetCursorAtEnd ? text.length() : 0);
}

void LineEdit::dragEnterEvent(QDragEnterEvent* e)
{
	if (KCal::ICalDrag::canDecode(e))
		e->accept(false);   // don't accept "text/calendar" objects
	e->accept(QTextDrag::canDecode(e)
	       || KURLDrag::canDecode(e)
	       || mType != Url && KPIM::MailListDrag::canDecode(e)
	       || mType == Emails && KVCardDrag::canDecode(e));
}

void LineEdit::dropEvent(QDropEvent* e)
{
	QString               newText;
	QStringList           newEmails;
	QString               txt;
	KPIM::MailList        mailList;
	KURL::List            files;
	KABC::Addressee::List addrList;

	if (mType != Url
	&&  e->provides(KPIM::MailListDrag::format())
	&&  KPIM::MailListDrag::decode(e, mailList))
	{
		// KMail message(s) - ignore all but the first
		if (mailList.count())
		{
			if (mType == Emails)
				newText = mailList.first().from();
			else
				setText(mailList.first().subject());    // replace any existing text
		}
	}
	// This must come before KURLDrag
	else if (mType == Emails
	&&  KVCardDrag::canDecode(e)  &&  KVCardDrag::decode(e, addrList))
	{
		// KAddressBook entries
		for (KABC::Addressee::List::Iterator it = addrList.begin();  it != addrList.end();  ++it)
		{
			QString em((*it).fullEmail());
			if (!em.isEmpty())
				newEmails.append(em);
		}
	}
	else if (KURLDrag::decode(e, files)  &&  files.count())
	{
		// URL(s)
		switch (mType)
		{
			case Url:
				// URL entry field - ignore all but the first dropped URL
				setText(files.first().prettyURL());    // replace any existing text
				break;
			case Emails:
			{
				// Email entry field - ignore all but mailto: URLs
				QString mailto = QString::fromLatin1("mailto");
				for (KURL::List::Iterator it = files.begin();  it != files.end();  ++it)
				{
					if ((*it).protocol() == mailto)
						newEmails.append((*it).path());
				}
				break;
			}
			case Text:
				newText = files.first().prettyURL();
				break;
		}
	}
	else if (QTextDrag::decode(e, txt))
	{
		// Plain text
		if (mType == Emails)
		{
			// Remove newlines from a list of email addresses, and allow an eventual mailto: protocol
			QString mailto = QString::fromLatin1("mailto:");
			newEmails = QStringList::split(QRegExp("[\r\n]+"), txt);
			for (QStringList::Iterator it = newEmails.begin();  it != newEmails.end();  ++it)
			{
				if ((*it).startsWith(mailto))
				{
					KURL url(*it);
					*it = url.path();
				}
			}
		}
		else
		{
			int newline = txt.find('\n');
			newText = (newline >= 0) ? txt.left(newline) : txt;
		}
	}

	if (newEmails.count())
	{
		newText = newEmails.join(",");
		int c = cursorPosition();
		if (c > 0)
			newText.prepend(",");
		if (c < static_cast<int>(text().length()))
			newText.append(",");
	}
	if (!newText.isEmpty())
		insert(newText);
}
