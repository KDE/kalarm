/*
 *  fontcolour.h  -  font and colour chooser widget
 *  Program:  kalarm
 *  (C) 2001, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

class QCheckBox;
class ColourCombo;


class FontColourChooser : public QWidget
{
	Q_OBJECT
public:
	FontColourChooser(QWidget* parent = 0, const char* name = 0,
	       bool onlyFixed = false,
	       const QStringList& fontList = QStringList(),
	       const QString& frameLabel = i18n("Requested font"),
	       bool fg = true, bool defaultFont = false, int visibleListSize = 8);

	void     setDefaultFont();
	void     setFont(const QFont&, bool onlyFixed = false);
	bool     defaultFont() const;
	QFont    font() const;
	QColor   fgColour() const;
	QColor   bgColour() const;
	void     setFgColour(const QColor&);
	void     setBgColour(const QColor&);
	QString  sampleText() const;
	void     setSampleText(const QString& text);

private slots:
	void     setSampleColour();
	void     slotDefaultFontToggled(bool);

private:
	ColourCombo*     mFgColourButton;       // or null
	ColourCombo*     mBgColourButton;
	KFontChooser*    mFontChooser;
	QCheckBox*       mDefaultFont;          // or null
};

#endif
