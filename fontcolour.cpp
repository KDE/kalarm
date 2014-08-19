/*
 *  fontcolour.cpp  -  font and colour chooser widget
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarmapp.h"
#include "preferences.h"
#include "colourbutton.h"
#include "checkbox.h"
#include "fontcolour.h"

#include <kglobal.h>
#include <kfontchooser.h>
#include <kfontdialog.h>

#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>


FontColourChooser::FontColourChooser(QWidget *parent,
          const QStringList &fontList, const QString& frameLabel, bool fg, bool defaultFont, int visibleListSize)
    : QWidget(parent),
      mFgColourButton(0),
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
    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    topLayout->addLayout(hlayout);
    QVBoxLayout* colourLayout = new QVBoxLayout();
    colourLayout->setMargin(0);
    hlayout->addLayout(colourLayout);
    if (fg)
    {
        QWidget* box = new QWidget(page);    // to group widgets for QWhatsThis text
        colourLayout->addWidget(box);
        QHBoxLayout* boxHLayout = new QHBoxLayout(box);
        boxHLayout->setMargin(0);
        boxHLayout->setSpacing(KDialog::spacingHint()/2);

        QLabel* label = new QLabel(i18nc("@label:listbox", "Foreground color:"), box);
        boxHLayout->addWidget(label);
        boxHLayout->setStretchFactor(new QWidget(box), 0);
        mFgColourButton = new ColourButton(box);
        boxHLayout->addWidget(mFgColourButton);
        connect(mFgColourButton, SIGNAL(changed(QColor)), SLOT(setSampleColour()));
        label->setBuddy(mFgColourButton);
        box->setWhatsThis(i18nc("@info:whatsthis", "Select the alarm message foreground color"));
    }

    QWidget* box = new QWidget(page);    // to group widgets for QWhatsThis text
    colourLayout->addWidget(box);
    QHBoxLayout* boxHLayout = new QHBoxLayout(box);
    boxHLayout->setMargin(0);
    boxHLayout->setSpacing(KDialog::spacingHint()/2);

    QLabel* label = new QLabel(i18nc("@label:listbox", "Background color:"), box);
    boxHLayout->addWidget(label);
    boxHLayout->setStretchFactor(new QWidget(box), 0);
    mBgColourButton = new ColourButton(box);
    boxHLayout->addWidget(mBgColourButton);
    connect(mBgColourButton, SIGNAL(changed(QColor)), SLOT(setSampleColour()));
    label->setBuddy(mBgColourButton);
    box->setWhatsThis(i18nc("@info:whatsthis", "Select the alarm message background color"));
    hlayout->addStretch();

    if (defaultFont)
    {
        QHBoxLayout* layout = new QHBoxLayout();
        layout->setMargin(0);
        topLayout->addLayout(layout);
        mDefaultFont = new CheckBox(i18nc("@option:check", "Use default font"), page);
        mDefaultFont->setMinimumSize(mDefaultFont->sizeHint());
        connect(mDefaultFont, SIGNAL(toggled(bool)), SLOT(slotDefaultFontToggled(bool)));
        mDefaultFont->setWhatsThis(i18nc("@info:whatsthis", "Check to use the default font current at the time the alarm is displayed."));
        layout->addWidget(mDefaultFont);
        layout->addWidget(new QWidget(page));    // left adjust the widget
    }
    else
        mDefaultFont = 0;

    mFontChooser = new KFontChooser(page, KFontChooser::DisplayFrame, fontList, visibleListSize);
    mFontChooser->installEventFilter(this);   // for read-only mode
    QList<QWidget*> kids = mFontChooser->findChildren<QWidget*>();
    for (int i = 0, end = kids.count();  i < end;  ++i)
        kids[i]->installEventFilter(this);
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

bool FontColourChooser::eventFilter(QObject*, QEvent* e)
{
    if (mReadOnly)
    {
        switch (e->type())
        {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
                return true;   // prevent the event being handled
            default:
                break;
        }
    }
    return false;
}

void FontColourChooser::slotDefaultFontToggled(bool on)
{
    mFontChooser->setEnabled(!on);
}
#include "moc_fontcolour.cpp"

// vim: et sw=4:
