/*
 *  fontcolour.cpp  -  font and colour chooser widget
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#include <qobjectlist.h>
#include <qwidget.h>
#include <qgroupbox.h>
#include <qpushbutton.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kcolordialog.h>

#include "kalarmapp.h"
#include "preferences.h"
#include "colourcombo.h"
#include "checkbox.h"
#include "fontcolour.moc"


FontColourChooser::FontColourChooser(QWidget *parent, const char *name,
          bool onlyFixed, const QStringList &fontList,
          const QString& frameLabel, bool editColours, bool fg, bool defaultFont,
          int visibleListSize)
	: QWidget(parent, name),
	  mFgColourButton(0),
	  mRemoveColourButton(0),
	  mColourList(theApp()->preferences()->messageColours()),
	  mReadOnly(false)
{
	QVBoxLayout* topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	QWidget* page = this;
	if (!frameLabel.isNull())
	{
		page = new QGroupBox(frameLabel, this);
		topLayout->addWidget(page);
		topLayout = new QVBoxLayout(page, KDialog::marginHint(), KDialog::spacingHint());
		topLayout->addSpacing(fontMetrics().lineSpacing()/2);
	}
	if (fg)
	{
		QBoxLayout* layout = new QHBoxLayout(topLayout);
		QHBox* box = new QHBox(page);    // to group widgets for QWhatsThis text
		box->setSpacing(KDialog::spacingHint());
		layout->addWidget(box);

		QLabel* label = new QLabel(i18n("&Foreground color:"), box);
		label->setMinimumSize(label->sizeHint());
		mFgColourButton = new ColourCombo(box);
		mFgColourButton->setMinimumSize(mFgColourButton->sizeHint());
		connect(mFgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
		label->setBuddy(mFgColourButton);
		QWhatsThis::add(box, i18n("Select the alarm message foreground color"));
		layout->addStretch();
	}

	QBoxLayout* layout = new QHBoxLayout(topLayout);
	QHBox* box = new QHBox(page);    // to group widgets for QWhatsThis text
	box->setSpacing(KDialog::spacingHint());
	layout->addWidget(box);

	QLabel* label = new QLabel(i18n("&Background color:"), box);
	label->setMinimumSize(label->sizeHint());
	mBgColourButton = new ColourCombo(box);
	mBgColourButton->setMinimumSize(mBgColourButton->sizeHint());
	connect(mBgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
	label->setBuddy(mBgColourButton);
	QWhatsThis::add(box, i18n("Select the alarm message background color"));
	layout->addStretch();

	if (editColours)
	{
		layout = new QHBoxLayout(topLayout);
		QPushButton* button = new QPushButton(i18n("Add Co&lor..."), page);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotAddColour()));
		QWhatsThis::add(button, i18n("Choose a new color to add to the color selection list."));
		layout->addWidget(button);

		mRemoveColourButton = new QPushButton(i18n("&Remove Color"), page);
		mRemoveColourButton->setFixedSize(mRemoveColourButton->sizeHint());
		connect(mRemoveColourButton, SIGNAL(clicked()), SLOT(slotRemoveColour()));
		QWhatsThis::add(mRemoveColourButton,
		      i18n("Remove the color currently shown in the background color chooser, from the color selection list."));
		layout->addWidget(mRemoveColourButton);
	}

	if (defaultFont)
	{
		layout = new QHBoxLayout(topLayout);
		mDefaultFont = new CheckBox(i18n("Use &default font"), page);
		mDefaultFont->setMinimumSize(mDefaultFont->sizeHint());
		connect(mDefaultFont, SIGNAL(toggled(bool)), SLOT(slotDefaultFontToggled(bool)));
		QWhatsThis::add(mDefaultFont,
		      i18n("Check to use the default font current at the time the alarm is displayed."));
		layout->addWidget(mDefaultFont);
		layout->addWidget(new QWidget(page));    // left adjust the widget
	}
	else
		mDefaultFont = 0;

	mFontChooser = new KFontChooser(page, name, onlyFixed, fontList, false, visibleListSize);
	topLayout->addWidget(mFontChooser);

	slotDefaultFontToggled(false);
}

void FontColourChooser::setDefaultFont()
{
	if (mDefaultFont)
		mDefaultFont->setChecked(true);
}

void FontColourChooser::setFont(const QFont& font, bool onlyFixed)
{
	if (mDefaultFont)
		mDefaultFont->setChecked(false);
	mFontChooser->setFont(font, onlyFixed);
}

bool FontColourChooser::defaultFont() const
{
	return mDefaultFont ? mDefaultFont->isChecked() : false;
}

QFont FontColourChooser::font() const
{
	return (mDefaultFont && mDefaultFont->isChecked()) ? QFont() : mFontChooser->font();
}

void FontColourChooser::setBgColour(const QColor& colour)
{
	mBgColourButton->setColor(colour);
	mFontChooser->setBackgroundColor(colour);
}

void FontColourChooser::setSampleColour()
{
	QColor bg = mBgColourButton->color();
	mFontChooser->setBackgroundColor(bg);
	QColor fg = fgColour();
	mFontChooser->setColor(fg);
	if (mRemoveColourButton)
		mRemoveColourButton->setEnabled(!mBgColourButton->isCustomColour());   // no deletion of custom colour
}

QColor FontColourChooser::bgColour() const
{
	return mBgColourButton->color();
}

QColor FontColourChooser::fgColour() const
{
	if (mFgColourButton)
		return mFgColourButton->color();
	else
	{
		QColor bg = mBgColourButton->color();
		QPalette pal(bg, bg);
		return pal.color(QPalette::Active, QColorGroup::Text);
	}
}

QString FontColourChooser::sampleText() const
{
	return mFontChooser->sampleText();
}

void FontColourChooser::setSampleText(const QString& text)
{
	mFontChooser->setSampleText(text);
}

void FontColourChooser::setFgColour(const QColor& colour)
{
	if (mFgColourButton)
	{
		mFgColourButton->setColor(colour);
		mFontChooser->setColor(colour);
	}
}

void FontColourChooser::setReadOnly(bool ro)
{
	if (ro != mReadOnly)
	{
		mReadOnly = ro;
		if (mFgColourButton)
			mFgColourButton->setReadOnly(ro);
		mBgColourButton->setReadOnly(ro);
		mDefaultFont->setReadOnly(ro);
	}
}

void FontColourChooser::slotDefaultFontToggled(bool on)
{
	mFontChooser->setEnabled(!on);
}

void FontColourChooser::setColours(const ColourList& colours)
{
	mColourList = colours;
	mBgColourButton->setColours(mColourList);
	mFontChooser->setBackgroundColor(mBgColourButton->color());
}

void FontColourChooser::slotAddColour()
{
	QColor colour;
	if (KColorDialog::getColor(colour, this) == QDialog::Accepted)
	{
		mColourList.insert(colour);
		mBgColourButton->setColours(mColourList);
	}
}

void FontColourChooser::slotRemoveColour()
{
	if (!mBgColourButton->isCustomColour())
	{
		mColourList.remove(mBgColourButton->color());
		mBgColourButton->setColours(mColourList);
	}
}

