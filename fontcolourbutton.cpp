/*
 *  fontcolourbutton.cpp  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  Copyright © 2003-2008 by David Jarvie <djarvie@kde.org>
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

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <klineedit.h>
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

FontColourButton::FontColourButton(QWidget* parent)
	: QFrame(parent),
	  mReadOnly(false)
{
	setFrameStyle(NoFrame);
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setMargin(0);
	layout->setSpacing(KDialog::spacingHint());
	layout->setSizeConstraint(QLayout::SetNoConstraint);
	setLayout(layout);

	mButton = new PushButton(i18nc("@action:button", "Font && Color..."), this),
	mButton->setFixedSize(mButton->sizeHint());
	connect(mButton, SIGNAL(clicked()), SLOT(slotButtonPressed()));
	mButton->setWhatsThis(i18nc("@info:whatsthis", "Choose the font, and foreground and background color, for the alarm message."));
	layout->addWidget(mButton);

	// Font and colour sample display
	mSample = new KLineEdit(this);
	mSample->setMinimumHeight(qMax(mSample->fontMetrics().lineSpacing(), mButton->height()*3/2));
	mSample->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
	mSample->setText(i18n("The Quick Brown Fox Jumps Over The Lazy Dog"));
	mSample->setCursorPosition(0);
	mSample->setAlignment(Qt::AlignCenter);
	mSample->setWhatsThis(i18nc("@info:whatsthis",
	      "This sample text illustrates the current font and color settings. "
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
	QPalette pal = mSample->palette();
	pal.setColor(mSample->backgroundRole(), mBgColour);
	mSample->setPalette(pal);
}

void FontColourButton::setFgColour(const QColor& colour)
{
	mFgColour = colour;
	QPalette pal = mSample->palette();
	pal.setColor(mSample->foregroundRole(), mFgColour);
	mSample->setPalette(pal);
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Display a font and colour selection dialog and get the selections.
*/
void FontColourButton::slotButtonPressed()
{
	FontColourDlg dlg(mBgColour, mFgColour, mFont, mDefaultFont,
	                  i18nc("@title:window", "Choose Alarm Font & Color"), this);
	dlg.setReadOnly(mReadOnly);
	if (dlg.exec() == QDialog::Accepted)
	{
		mDefaultFont = dlg.defaultFont();
		mFont        = dlg.font();
		mSample->setFont(mFont);
		QPalette pal = mSample->palette();
		mBgColour    = dlg.bgColour();
		pal.setColor(mSample->backgroundRole(), mBgColour);
		mFgColour    = dlg.fgColour();
		pal.setColor(mSample->foregroundRole(), mFgColour);
		mSample->setPalette(pal);
		emit selected();
	}
}


/*=============================================================================
= Class FontColourDlg
= Font/colour selection dialog.
=============================================================================*/

FontColourDlg::FontColourDlg(const QColor& bgColour, const QColor& fgColour, const QFont& font,
                             bool defaultFont, const QString& caption, QWidget* parent)
	: KDialog(parent),
	  mReadOnly(false)
{
	setCaption(caption);
	setButtons(Ok|Cancel);
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->setMargin(0);
	layout->setSpacing(spacingHint());
	mChooser = new FontColourChooser(page, QStringList(), QString(), false, true, true);
	mChooser->setBgColour(bgColour);
	mChooser->setFgColour(fgColour);
	if (defaultFont)
		mChooser->setDefaultFont();
	else
		mChooser->setFont(font);
	layout->addWidget(mChooser);
	layout->addSpacing(KDialog::spacingHint());
	connect(this,SIGNAL(okClicked()),SLOT(slotOk()));
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
