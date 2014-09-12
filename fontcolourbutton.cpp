/*
 *  fontcolourbutton.cpp  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  Copyright © 2003-2013 by David Jarvie <djarvie@kde.org>
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
#include "fontcolourbutton.h"

#include "autoqpointer.h"
#include "fontcolour.h"
#include "preferences.h"
#include "pushbutton.h"

#include <klineedit.h>
#include <KLocalizedString>

#include <QVBoxLayout>
#include <qdebug.h>


/*=============================================================================
= Class FontColourButton
= Font/colour selection button.
=============================================================================*/

FontColourButton::FontColourButton(QWidget* parent)
    : PushButton(i18nc("@action:button", "Font && Color..."), parent),
      mDefaultFont(true),
      mReadOnly(false)
{
    connect(this, &FontColourButton::clicked, this, &FontColourButton::slotButtonPressed);
    setWhatsThis(i18nc("@info:whatsthis", "Choose the font, and foreground and background color, for the alarm message."));
}

void FontColourButton::setDefaultFont()
{
    mDefaultFont = true;
}

void FontColourButton::setFont(const QFont& font)
{
    mDefaultFont = false;
    mFont = font;
}

/******************************************************************************
* Called when the OK button is clicked.
* Display a font and colour selection dialog and get the selections.
*/
void FontColourButton::slotButtonPressed()
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
        // deletion of FontColourButton, and on return from this function).
    AutoQPointer<FontColourDlg> dlg = new FontColourDlg(mBgColour, mFgColour, mFont, mDefaultFont,
                                             i18nc("@title:window", "Choose Alarm Font & Color"), this);
    dlg->setReadOnly(mReadOnly);
    if (dlg->exec() == QDialog::Accepted)
    {
        mDefaultFont = dlg->defaultFont();
        mFont        = dlg->font();
        mBgColour    = dlg->bgColour();
        mFgColour    = dlg->fgColour();
        emit selected(mFgColour, mBgColour);
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
    mChooser = new FontColourChooser(page, QStringList(), QString(), true, true);
    mChooser->setBgColour(bgColour);
    mChooser->setFgColour(fgColour);
    if (defaultFont)
        mChooser->setDefaultFont();
    else
        mChooser->setFont(font);
    layout->addWidget(mChooser);
    layout->addSpacing(KDialog::spacingHint());
    connect(this, &FontColourDlg::okClicked, this, &FontColourDlg::slotOk);
}

/******************************************************************************
* Called when the OK button is clicked.
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
#include "moc_fontcolourbutton.cpp"
// vim: et sw=4:
