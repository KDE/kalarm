/*
 *  fontcolourbutton.h  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  (C) 2003 by David Jarvie <software@astrojar.org.uk>
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef FONTCOLOURBUTTON_H
#define FONTCOLOURBUTTON_H

#include <qfont.h>
#include <qcolor.h>
#include <kdialogbase.h>
#include "pushbutton.h"

class FontColourChooser;


class FontColourButton : public PushButton
{
		Q_OBJECT
	public:
		FontColourButton(QWidget* parent = 0, const char* name = 0);
		void          setDefaultFont()                   { mDefaultFont = true; }
		void          setFont(const QFont& font)         { mFont = font;  mDefaultFont = false; }
		void          setBgColour(const QColor& colour)  { mBgColour = colour; }
		void          setFgColour(const QColor& colour)  { mFgColour = colour; }
		bool          defaultFont() const                { return mDefaultFont; }
		QFont         font() const                       { return mFont; }
		QColor        bgColour() const                   { return mBgColour; }
		QColor        fgColour() const                   { return mFgColour; }
		virtual void  setReadOnly(bool ro)               { mReadOnly = ro; }
		virtual bool  isReadOnly() const                 { return mReadOnly; }

	signals:
		void          selected();

	protected slots:
		void          slotButtonPressed();

	private:
		QColor    mBgColour, mFgColour;
		QFont     mFont;
		bool      mDefaultFont;
		bool      mReadOnly;
};


// Font and colour selection dialog displayed by the push button
class FontColourDlg : public KDialogBase
{
		Q_OBJECT
	public:
		FontColourDlg(const QColor& bg, const QColor& fg, const QFont&, bool defaultFont,
		              const QString& caption, QWidget* parent = 0, const char* name = 0);
		bool         defaultFont() const   { return mDefaultFont; }
		QFont        font() const          { return mFont; }
		QColor       bgColour() const      { return mBgColour; }
		QColor       fgColour() const      { return mFgColour; }
		void         setReadOnly(bool);
		bool         isReadOnly() const    { return mReadOnly; }

	protected slots:
		virtual void slotOk();

	private:
		FontColourChooser* mChooser;
		QColor             mBgColour, mFgColour;
		QFont              mFont;
		bool               mDefaultFont;
		bool               mReadOnly;
};

#endif // FONTCOLOURBUTTON_H
