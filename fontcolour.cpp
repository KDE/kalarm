/*
 *  fontcolour.cpp  -  font and colour chooser widget
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
		topLayout = new QVBoxLayout(page, KDialog::marginHint(), KDialog::spacingHint());
		topLayout->addSpacing(fontMetrics().lineSpacing()/2);
	}

	QGridLayout* grid = new QGridLayout(topLayout, (fg ? 3 : 2), 2);
	grid->addRowSpacing(0, KDialog::marginHint());
	int gridRow = 1;

	QLabel* label;
	if (fg)
	{
		label = new QLabel(i18n("Default foreground color:"), page);
		grid->addWidget(label, gridRow, 0, AlignRight);
		label->setFixedSize(label->sizeHint());
		m_fgColourButton = new ColourCombo(page);
		grid->addWidget(m_fgColourButton, gridRow, 1, AlignRight);
		connect(m_fgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
		++gridRow;
	}

	label = new QLabel(i18n("Default background color:"), page);
	label->setMinimumSize(label->sizeHint());
	grid->addWidget(label, gridRow, 0);
	m_bgColourButton = new ColourCombo(page);
	m_bgColourButton->setMinimumSize(m_bgColourButton->sizeHint());
	grid->addWidget(m_bgColourButton, gridRow, 1, AlignRight);
	connect(m_bgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));

	m_fontChooser = new KFontChooser(page, name, onlyFixed, fontList, false, visibleListSize);
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
		m_fontChooser->setColor(colour);
	}
}

void FontColourChooser::setBgColour(const QColor& colour)
{
	m_bgColourButton->setColour(colour);
	m_fontChooser->setBackgroundColor(colour);
}

void FontColourChooser::setSampleColour()
{
	QColor bg = m_bgColourButton->color();
	m_fontChooser->setBackgroundColor(bg);
	QColor fg = fgColour();
	m_fontChooser->setColor(fg);
}

QColor FontColourChooser::fgColour() const
{
	QColor bg = m_bgColourButton->color();
	QPalette pal(bg, bg);
	return pal.color(QPalette::Active, QColorGroup::Text);
}
