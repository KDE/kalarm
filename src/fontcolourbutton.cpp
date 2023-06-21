/*
 *  fontcolourbutton.cpp  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fontcolourbutton.h"

#include "fontcolour.h"
#include "lib/autoqpointer.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QStyle>
#include <QDialogButtonBox>


/*=============================================================================
= Class FontColourButton
= Font/colour selection button.
=============================================================================*/

FontColourButton::FontColourButton(QWidget* parent)
    : PushButton(i18nc("@action:button", "Font && Color..."), parent)
{
    connect(this, &FontColourButton::clicked, this, &FontColourButton::slotButtonPressed);
    setToolTip(i18nc("@info:tooltip", "Set alarm message font and color"));
    setWhatsThis(i18nc("@info:whatsthis", "Choose the font, and foreground and background color, for the alarm message."));
}

void FontColourButton::setFont(const QFont& font, bool defaultFont)
{
    mDefaultFont = defaultFont;
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
        Q_EMIT selected(mFgColour, mBgColour);
    }
}


/*=============================================================================
= Class FontColourDlg
= Font/colour selection dialog.
=============================================================================*/

FontColourDlg::FontColourDlg(const QColor& bgColour, const QColor& fgColour, const QFont& font,
                             bool defaultFont, const QString& caption, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(caption);

    auto layout = new QVBoxLayout(this);
    mChooser = new FontColourChooser(this, QStringList(), QString(), true, true);
    mChooser->setBgColour(bgColour);
    mChooser->setFgColour(fgColour);
    mChooser->setFont(font, defaultFont);
    layout->addWidget(mChooser);
    layout->addSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(QDialogButtonBox::Ok);
    buttonBox->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &FontColourDlg::slotOk);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &FontColourDlg::reject);
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

// vim: et sw=4:

#include "moc_fontcolourbutton.cpp"
