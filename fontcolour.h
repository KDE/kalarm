/*
 *  fontcolour.h  -  font and colour chooser widget
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#ifndef FONTCOLOUR_H
#define FONTCOLOUR_H

#include <qwidget.h>
#include <qsize.h>

#include "fontchooser.h"
#include "colourcombo.h"


// General tab of the Preferences dialog
class FontColourChooser : public QWidget
{
	Q_OBJECT
public:
	FontColourChooser(QWidget *parent = 0L, const char *name = 0L,
	       bool onlyFixed = false,
	       const QStringList &fontList = QStringList(),
	       bool makeFrame = true, const QString& frameLabel = "Requested font",
	       bool fg = true, int visibleListSize=8 );
	~FontColourChooser();

	void setFont( const QFont& font, bool onlyFixed = false )  { m_fontChooser->setFont(font, onlyFixed); }
	QFont font() const  { return m_fontChooser->font(); }
	QColor fgColour() const;
	QColor bgColour() const    { return m_bgColourButton->color(); }
	void setFgColour(const QColor&);
	void setBgColour(const QColor&);
	void setCharset( const QString & charset )  { m_fontChooser->setCharset(charset); }
	QString charset() const  { return m_fontChooser->charset(); }
	QString sampleText() const { return m_fontChooser->sampleText(); }
	void setSampleText( const QString &text )  { m_fontChooser->setSampleText(text); }

private:
	ColourCombo*     m_fgColourButton;       // or null
	ColourCombo*     m_bgColourButton;
	FontChooser*     m_fontChooser;

private slots:
	void setSampleColour();
};

#endif
