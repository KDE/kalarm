/*
 *  fontcolourbutton.cpp  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  Copyright © 2003-2005,2007,2008 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include <qcheckbox.h>
#include <qlayout.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <kdebug.h>

#include "fontcolour.h"
#include "preferences.h"
#include "pushbutton.h"
#include "fontcolourbutton.moc"


/*=============================================================================
= Class FontColourButton
= Font/colour selection button.
=============================================================================*/

FontColourButton::FontColourButton(QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mReadOnly(false)
{
	setFrameStyle(NoFrame);
	QHBoxLayout* layout = new QHBoxLayout(this, 0, KDialog::spacingHint());

	mButton = new PushButton(i18n("Font && Co&lor..."), this);
	mButton->setFixedSize(mButton->sizeHint());
	connect(mButton, SIGNAL(clicked()), SLOT(slotButtonPressed()));
	QWhatsThis::add(mButton,
	      i18n("Choose the font, and foreground and background color, for the alarm message."));
	layout->addWidget(mButton);

	// Font and colour sample display
	mSample = new QLineEdit(this);
	mSample->setMinimumHeight(QMAX(mSample->fontMetrics().lineSpacing(), mButton->height()*3/2));
	mSample->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
	mSample->setText(i18n("The Quick Brown Fox Jumps Over The Lazy Dog"));
	mSample->setCursorPosition(0);
	mSample->setAlignment(Qt::AlignCenter);
	QWhatsThis::add(mSample,
	      i18n("This sample text illustrates the current font and color settings. "
	           "You may edit it to test special characters."));
	layout->addWidget(mSample);
}

void FontColourButton::setDefaultFont()
{
	mDefaultFont = true;
	mSample->setFont(Preferences::messageFont());
}

void FontColourButton::setFont(const QFont& font)
{
	mDefaultFont = false;
	mFont = font;
	mSample->setFont(mFont);
}

void FontColourButton::setBgColour(const QColor& colour)
{
	mBgColour = colour;
	mSample->setPaletteBackgroundColor(mBgColour);
}

void FontColourButton::setFgColour(const QColor& colour)
{
	mFgColour = colour;
	mSample->setPaletteForegroundColor(mFgColour);
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Display a font and colour selection dialog and get the selections.
*/
void FontColourButton::slotButtonPressed()
{
	FontColourDlg dlg(mBgColour, mFgColour, mFont, mDefaultFont,
	                  i18n("Choose Alarm Font & Color"), this, "fontColourDlg");
	dlg.setReadOnly(mReadOnly);
	if (dlg.exec() == QDialog::Accepted)
	{
		mDefaultFont = dlg.defaultFont();
		mFont        = dlg.font();
		mSample->setFont(mFont);
		mBgColour    = dlg.bgColour();
		mSample->setPaletteBackgroundColor(mBgColour);
		mFgColour    = dlg.fgColour();
		mSample->setPaletteForegroundColor(mFgColour);
		emit selected();
	}
}


/*=============================================================================
= Class FontColourDlg
= Font/colour selection dialog.
=============================================================================*/

FontColourDlg::FontColourDlg(const QColor& bgColour, const QColor& fgColour, const QFont& font,
                             bool defaultFont, const QString& caption, QWidget* parent, const char* name)
	: KDialogBase(parent, name, true, caption, Ok|Cancel, Ok, false),
	  mReadOnly(false)
{
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page, 0, spacingHint());
	mChooser = new FontColourChooser(page, 0, false, QStringList(), QString::null, false, true, true);
	mChooser->setBgColour(bgColour);
	mChooser->setFgColour(fgColour);
	if (defaultFont)
		mChooser->setDefaultFont();
	else
		mChooser->setFont(font);
	layout->addWidget(mChooser);
	layout->addSpacing(KDialog::spacingHint());
}

/******************************************************************************
*  Called when the OK button is clicked.
*/
void FontColourDlg::slotOk()
{
	if (mReadOnly)
	{
		reject();
		return;
	}
	mDefaultFont = mChooser->defaultFont();
	mFont        = mChooser->font();
	mBgColour    = mChooser->bgColour();
	mFgColour    = mChooser->fgColour();
	accept();
}

void FontColourDlg::setReadOnly(bool ro)
{
	mReadOnly = ro;
	mChooser->setReadOnly(mReadOnly);
}
