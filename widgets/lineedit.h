/*
 *  lineedit.h  -  line edit widget with extra drag and drop options
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <klineedit.h>


/**
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
 *  @short Line edit widget with extra drag and drop options.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class LineEdit : public KLineEdit
{
		Q_OBJECT
	public:
		/** Types of drag and drop content which will be accepted.
		 *  @li Text   - the line edit contains general text. It accepts text, a URL
		 *               or an email from KMail (the subject line is used). If multiple
		 *               URLs or emails are dropped, only the first is used; the
		 *               rest are ignored.
		 *  @li Url    - the line edit contains a URL. It accepts text or a URL. If
		 *               multiple URLs are dropped, only the first URL is used; the
		 *               rest are ignored.
		 *  @li Emails - the line edit contains email addresses. It accepts text,
		 *               mailto: URLs, emails from KMail (the From address is used)
		 *               or vcard data (e.g. from KAddressBook). If multiple emails
		 *               are dropped, only the first is used; the rest are ignored.
		 *               
		 */
		enum Type { Text, Url, Emails };
		/** Constructor.
		 *  @param type The content type for the line edit.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		explicit LineEdit(Type type, QWidget* parent = 0, const char* name = 0);
		/** Constructs a line edit whose content type is Text.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		explicit LineEdit(QWidget* parent = 0, const char* name = 0);
		/** Prevents the line edit's contents being selected when the widget receives focus. */
		void         setNoSelect()   { mNoSelect = true; }
		/** Sets whether the cursor should be set at the beginning or end of the text when
		 *  @see setText() is called. */
		void         setCursorAtEnd(bool end = true)  { mSetCursorAtEnd = end; }
	public slots:
		/** Sets the contents of the line edit to be @p str. */
		virtual void setText(const QString& str);
	protected:
		virtual void focusInEvent(QFocusEvent*);
		virtual void dragEnterEvent(QDragEnterEvent*);
		virtual void dropEvent(QDropEvent*);
	private:
		void         init();

		Type  mType;
		bool  mNoSelect;
		bool  mSetCursorAtEnd;   // setText() should position cursor at end
};

#endif // LINEEDIT_H
