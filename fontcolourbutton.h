/*
 *  fontcolourbutton.h  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  Copyright © 2003,2007 by David Jarvie <djarvie@kde.org>
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

#ifndef FONTCOLOURBUTTON_H
#define FONTCOLOURBUTTON_H

#include <qfont.h>
#include <qcolor.h>
#include <qframe.h>
#include <kdialogbase.h>

class QLineEdit;
class FontColourChooser;
class PushButton;


class FontColourButton : public QFrame
{
		Q_OBJECT
	public:
		FontColourButton(QWidget* parent = 0, const char* name = 0);
		void          setDefaultFont();
		void          setFont(const QFont&);
		void          setBgColour(const QColor&);
		void          setFgColour(const QColor&);
		bool          defaultFont() const    { return mDefaultFont; }
		QFont         font() const           { return mFont; }
		QColor        bgColour() const       { return mBgColour; }
		QColor        fgColour() const       { return mFgColour; }
		virtual void  setReadOnly(bool ro)   { mReadOnly = ro; }
		virtual bool  isReadOnly() const     { return mReadOnly; }

	signals:
		void          selected();

	protected slots:
		void          slotButtonPressed();

	private:
		PushButton* mButton;
		QColor      mBgColour, mFgColour;
		QFont       mFont;
		QLineEdit*  mSample;
		bool        mDefaultFont;
		bool        mReadOnly;
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
