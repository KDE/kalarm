/*
 *  fontcolour.cpp  -  font and colour chooser widget
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#include <qobjectlist.h>
#include <qwidget.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>

#include <kdialog.h>
#include <kglobal.h>
#include <klocale.h>

#include "fontcolour.h"
#include "fontcolour.moc"


FontColourChooser::FontColourChooser(QWidget *parent, const char *name,
	       bool onlyFixed, const QStringList &fontList,
	       bool makeFrame, const QString& frameLabel, bool fg, int visibleListSize)
	: QWidget(parent, name)
{
	QVBoxLayout* topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	QWidget* page = this;
	if (makeFrame)
	{
		page = new QGroupBox(i18n(frameLabel), this);
		topLayout->addWidget(page);
		topLayout = new QVBoxLayout(page, KDialog::spacingHint(), KDialog::spacingHint());
	}

	QGridLayout* grid = new QGridLayout(page, (fg ? 3 : 2), 2, KDialog::spacingHint());
	topLayout->addLayout(grid);
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	int gridRow = 1;

	QLabel* label;
	if (fg)
	{
		label = new QLabel(i18n("Default foreground colour:"), page);
		grid->addWidget(label, gridRow, 0, AlignRight);
		label->setFixedSize(label->sizeHint());
		m_fgColourButton = new ColourCombo(page);
		grid->addWidget(m_fgColourButton, gridRow, 1, AlignRight);
		connect(m_fgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
		++gridRow;
	}

	label = new QLabel(i18n("Default background colour:"), page);
	grid->addWidget(label, gridRow, 0, AlignRight);
	m_bgColourButton = new ColourCombo(page);
	grid->addWidget(m_bgColourButton, gridRow, 1, AlignRight);
	connect(m_bgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));

	m_fontChooser = new FontChooser(page, name, onlyFixed, fontList, false, visibleListSize);
	topLayout->addWidget(m_fontChooser);
}

FontColourChooser::~FontColourChooser()
{
}

void FontColourChooser::setFgColour(const QColor& colour)
{
	if (m_fgColourButton)
	{
		m_fgColourButton->setColour(colour);
		m_fontChooser->setColor(colour, QPalette::Active, QColorGroup::Text);
		m_fontChooser->setColor(colour, QPalette::Inactive, QColorGroup::Text);
	}
}

void FontColourChooser::setBgColour(const QColor& colour)
{
	m_bgColourButton->setColour(colour);
	m_fontChooser->setColor(colour, QPalette::Active, QColorGroup::Base);
	m_fontChooser->setColor(colour, QPalette::Inactive, QColorGroup::Base);
}

void FontColourChooser::setSampleColour()
{
	QColor bg = m_bgColourButton->color();
	m_fontChooser->setColor(bg, QPalette::Active, QColorGroup::Base);
	m_fontChooser->setColor(bg, QPalette::Inactive, QColorGroup::Base);
	QColor fg = fgColour();
	m_fontChooser->setColor(fg, QPalette::Active, QColorGroup::Text);
	m_fontChooser->setColor(fg, QPalette::Inactive, QColorGroup::Text);
}

QColor FontColourChooser::fgColour() const
{
	QColor bg = m_bgColourButton->color();
	QPalette pal(bg, bg);
	return pal.color(QPalette::Active, QColorGroup::Text);
}
