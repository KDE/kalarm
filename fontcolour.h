/*
 *  fontcolour.h  -  font and colour chooser widget
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
 */

#ifndef FONTCOLOUR_H
#define FONTCOLOUR_H

#include <qwidget.h>
#include <qsize.h>

#include <kfontdialog.h>

#include "colourcombo.h"


// General tab of the Preferences dialog
class FontColourChooser : public QWidget
{
	Q_OBJECT
public:
	FontColourChooser(QWidget* parent = 0L, const char* name = 0L,
	       bool onlyFixed = false,
	       const QStringList& fontList = QStringList(),
	       bool makeFrame = true, const QString& frameLabel = i18n("Requested font"),
	       bool fg = true, int visibleListSize=8);
	~FontColourChooser();

	void setFont(const QFont& font, bool onlyFixed = false)  { m_fontChooser->setFont(font, onlyFixed); }
	QFont font() const                       { return m_fontChooser->font(); }
	QColor fgColour() const;
	QColor bgColour() const                  { return m_bgColourButton->color(); }
	void setFgColour(const QColor&);
	void setBgColour(const QColor&);
#if QT_VERSION < 300
	void setCharset(const QString& charset)  { m_fontChooser->setCharset(charset); }
	QString charset() const                  { return m_fontChooser->charset(); }
#endif
	QString sampleText() const               { return m_fontChooser->sampleText(); }
	void setSampleText(const QString& text)  { m_fontChooser->setSampleText(text); }

private:
	ColourCombo*     m_fgColourButton;       // or null
	ColourCombo*     m_bgColourButton;
	KFontChooser*    m_fontChooser;

private slots:
	void setSampleColour();
};

#endif
