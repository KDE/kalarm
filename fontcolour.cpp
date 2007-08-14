/*
 *  fontcolour.cpp  -  font and colour chooser widget
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <kglobal.h>
#include <klocale.h>
#include <kfontdialog.h>
#include <kcolordialog.h>
#include <khbox.h>

#include "kalarmapp.h"
#include "preferences.h"
#include "colourcombo.h"
#include "checkbox.h"
#include "fontcolour.moc"


FontColourChooser::FontColourChooser(QWidget *parent,
          const QStringList &fontList, const QString& frameLabel, bool editColours,
          bool fg, bool defaultFont, int visibleListSize)
	: QWidget(parent),
	  mFgColourButton(0),
	  mRemoveColourButton(0),
	  mColourList(Preferences::messageColours()),
	  mReadOnly(false)
{
	QVBoxLayout* topLayout = new QVBoxLayout(this);
	topLayout->setMargin(0);
	topLayout->setSpacing(KDialog::spacingHint());
	QWidget* page = this;
	if (!frameLabel.isNull())
	{
		page = new QGroupBox(frameLabel, this);
		topLayout->addWidget(page);
		topLayout = new QVBoxLayout(page);
		topLayout->setMargin(KDialog::marginHint());
		topLayout->setSpacing(KDialog::spacingHint());
	}
	if (fg)
	{
		QHBoxLayout* layout = new QHBoxLayout();
		layout->setMargin(0);
		topLayout->addLayout(layout);
		KHBox* box = new KHBox(page);    // to group widgets for QWhatsThis text
		box->setMargin(0);
		box->setSpacing(KDialog::spacingHint());
		layout->addWidget(box);

		QLabel* label = new QLabel(i18nc("@label:listbox", "&Foreground color:"), box);
		label->setMinimumSize(label->sizeHint());
		mFgColourButton = new ColourCombo(box);
		mFgColourButton->setMinimumSize(mFgColourButton->sizeHint());
		connect(mFgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
		label->setBuddy(mFgColourButton);
		box->setWhatsThis(i18nc("@info:whatsthis", "Select the alarm message foreground color"));
		layout->addStretch();
	}

	QHBoxLayout* layout = new QHBoxLayout();
	layout->setMargin(0);
	topLayout->addLayout(layout);
	KHBox* box = new KHBox(page);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	layout->addWidget(box);

	QLabel* label = new QLabel(i18nc("@label:listbox", "&Background color:"), box);
	label->setMinimumSize(label->sizeHint());
	mBgColourButton = new ColourCombo(box);
	mBgColourButton->setMinimumSize(mBgColourButton->sizeHint());
	connect(mBgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
	label->setBuddy(mBgColourButton);
	box->setWhatsThis(i18nc("@info:whatsthis", "Select the alarm message background color"));
	layout->addStretch();

	if (editColours)
	{
		layout = new QHBoxLayout();
		layout->setMargin(0);
		topLayout->addLayout(layout);
		QPushButton* button = new QPushButton(i18nc("@action:button", "Add Co&lor..."), page);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotAddColour()));
		button->setWhatsThis(i18nc("@info:whatsthis", "Choose a new color to add to the color selection list."));
		layout->addWidget(button);

		mRemoveColourButton = new QPushButton(i18nc("@action:button", "&Remove Color"), page);
		mRemoveColourButton->setFixedSize(mRemoveColourButton->sizeHint());
		connect(mRemoveColourButton, SIGNAL(clicked()), SLOT(slotRemoveColour()));
		mRemoveColourButton->setWhatsThis(i18nc("@info:whatsthis", "Remove the color currently shown in the background color chooser, from the color selection list."));
		layout->addWidget(mRemoveColourButton);
	}

	if (defaultFont)
	{
		layout = new QHBoxLayout();
		layout->setMargin(0);
		topLayout->addLayout(layout);
		mDefaultFont = new CheckBox(i18nc("@option:check", "Use &default font"), page);
		mDefaultFont->setMinimumSize(mDefaultFont->sizeHint());
		connect(mDefaultFont, SIGNAL(toggled(bool)), SLOT(slotDefaultFontToggled(bool)));
		mDefaultFont->setWhatsThis(i18nc("@info:whatsthis", "Check to use the default font current at the time the alarm is displayed."));
		layout->addWidget(mDefaultFont);
		layout->addWidget(new QWidget(page));    // left adjust the widget
	}
	else
		mDefaultFont = 0;

	mFontChooser = new KFontChooser(page, KFontChooser::DisplayFrame, fontList, visibleListSize);
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
		mRemoveColourButton->setEnabled(!mBgColourButton->isCustomColor());   // no deletion of custom colour
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
		return pal.color(QPalette::Active, QPalette::Text);
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
	if (!mBgColourButton->isCustomColor())
	{
		mColourList.remove(mBgColourButton->color());
		mBgColourButton->setColours(mColourList);
	}
}

