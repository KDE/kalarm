/*
 *  fontcolour.h  -  font and colour chooser widget
 *  Program:  kalarm
 *  (C) 2001, 2003 by David Jarvie <software@astrojar.org.uk>
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef FONTCOLOUR_H
#define FONTCOLOUR_H

#include <kdeversion.h>
#include <qwidget.h>
#include <qstringlist.h>
#if KDE_VERSION >= 290
#include <kfontdialog.h>
#else
#include "fontchooser.h"
#endif
#include "colourlist.h"

class ColourCombo;
class QPushButton;
class CheckBox;


class FontColourChooser : public QWidget
{
	Q_OBJECT
public:
	FontColourChooser(QWidget* parent = 0, const char* name = 0,
	       bool onlyFixed = false,
	       const QStringList& fontList = QStringList(),
	       const QString& frameLabel = i18n("Requested font"),
	       bool editColours = false, bool fg = true, bool defaultFont = false,
	       int visibleListSize = 8);

	void              setDefaultFont();
	void              setFont(const QFont&, bool onlyFixed = false);
	bool              defaultFont() const;
	QFont             font() const;
	QColor            fgColour() const;
	QColor            bgColour() const;
	const ColourList& colours() const   { return mColourList; }
	void              setFgColour(const QColor&);
	void              setBgColour(const QColor&);
	void              setColours(const ColourList&);
	QString           sampleText() const;
	void              setSampleText(const QString& text);
	bool              isReadOnly() const     { return mReadOnly; }
	void              setReadOnly(bool);

private slots:
	void              setSampleColour();
	void              slotDefaultFontToggled(bool);
	void              slotAddColour();
	void              slotRemoveColour();

private:
	ColourCombo*     mFgColourButton;       // or null
	ColourCombo*     mBgColourButton;
	QPushButton*     mRemoveColourButton;
	KFontChooser*    mFontChooser;
	CheckBox*        mDefaultFont;          // or null
	ColourList       mColourList;
	bool             mReadOnly;
};

#endif
