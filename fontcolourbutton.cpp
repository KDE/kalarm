/*
 *  fontcolourbutton.cpp  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  Copyright (c) 2003 - 2005 by David Jarvie <software@astrojar.org.uk>
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
#include <q3whatsthis.h>
//Added by qt3to4:
#include <QVBoxLayout>

#include <klocale.h>
#include <kdebug.h>

#include "fontcolour.h"
#include "fontcolourbutton.moc"


/*=============================================================================
= Class FontColourButton
= Font/colour selection buttong.
=============================================================================*/

FontColourButton::FontColourButton(QWidget* parent)
	: PushButton(i18n("Font && Co&lor..."), parent),
	  mReadOnly(false)
{
	connect(this, SIGNAL(clicked()), SLOT(slotButtonPressed()));
	Q3WhatsThis::add(this,
	      i18n("Choose the font, and foreground and background color, for the alarm message."));
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
		mBgColour    = dlg.bgColour();
		mFgColour    = dlg.fgColour();
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
		reject();
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
