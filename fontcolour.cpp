/*
 *  fontcolour.cpp  -  font and colour chooser widget
 *  Program:  kalarm
 *  Copyright (c) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <qobject.h>
#include <qwidget.h>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <kglobal.h>
#include <klocale.h>
#include <kcolordialog.h>
#include <khbox.h>

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
		QHBoxLayout* layout = new QHBoxLayout(topLayout);
		layout->setMargin(0);
		KHBox* box = new KHBox(page);    // to group widgets for QWhatsThis text
		box->setMargin(0);
		box->setSpacing(KDialog::spacingHint());
		layout->addWidget(box);

		QLabel* label = new QLabel(i18n("&Foreground color:"), box);
		label->setMinimumSize(label->sizeHint());
		mFgColourButton = new ColourCombo(box);
		mFgColourButton->setMinimumSize(mFgColourButton->sizeHint());
		connect(mFgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
		label->setBuddy(mFgColourButton);
		box->setWhatsThis(i18n("Select the alarm message foreground color"));
		layout->addStretch();
	}

	QHBoxLayout* layout = new QHBoxLayout(topLayout);
	layout->setMargin(0);
	KHBox* box = new KHBox(page);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	layout->addWidget(box);

	QLabel* label = new QLabel(i18n("&Background color:"), box);
	label->setMinimumSize(label->sizeHint());
	mBgColourButton = new ColourCombo(box);
	mBgColourButton->setMinimumSize(mBgColourButton->sizeHint());
	connect(mBgColourButton, SIGNAL(activated(const QString&)), SLOT(setSampleColour()));
	label->setBuddy(mBgColourButton);
	box->setWhatsThis(i18n("Select the alarm message background color"));
	layout->addStretch();

	if (editColours)
	{
		layout = new QHBoxLayout(topLayout);
		layout->setMargin(0);
		QPushButton* button = new QPushButton(i18n("Add Co&lor..."), page);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotAddColour()));
		button->setWhatsThis(i18n("Choose a new color to add to the color selection list."));
		layout->addWidget(button);

		mRemoveColourButton = new QPushButton(i18n("&Remove Color"), page);
		mRemoveColourButton->setFixedSize(mRemoveColourButton->sizeHint());
		connect(mRemoveColourButton, SIGNAL(clicked()), SLOT(slotRemoveColour()));
		mRemoveColourButton->setWhatsThis(i18n("Remove the color currently shown in the background color chooser, from the color selection list."));
		layout->addWidget(mRemoveColourButton);
	}

	if (defaultFont)
	{
		layout = new QHBoxLayout(topLayout);
		layout->setMargin(0);
		mDefaultFont = new CheckBox(i18n("Use &default font"), page);
		mDefaultFont->setMinimumSize(mDefaultFont->sizeHint());
		connect(mDefaultFont, SIGNAL(toggled(bool)), SLOT(slotDefaultFontToggled(bool)));
		mDefaultFont->setWhatsThis(i18n("Check to use the default font current at the time the alarm is displayed."));
		layout->addWidget(mDefaultFont);
		layout->addWidget(new QWidget(page));    // left adjust the widget
	}
	else
		mDefaultFont = 0;

	mFontChooser = new KFontChooser(page, onlyFixed, fontList, false, visibleListSize);
	mFontChooser->setObjectName( name );
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

