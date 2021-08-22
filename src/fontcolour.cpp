/*
 *  fontcolour.cpp  -  font and colour chooser widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fontcolour.h"

#include "kalarmapp.h"
#include "preferences.h"
#include "lib/colourbutton.h"
#include "lib/checkbox.h"

#include <KFontChooser>
#include <kwidgetsaddons_version.h>

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>


FontColourChooser::FontColourChooser(QWidget* parent, const QStringList& fontList,
           const QString& frameLabel, bool fg, bool defaultFont, int visibleListSize)
    : QWidget(parent)
{
    auto topLayout = new QVBoxLayout(this);
    QWidget* page = this;
    if (!frameLabel.isNull())
    {
        page = new QGroupBox(frameLabel, this);
        topLayout->addWidget(page);
        topLayout = new QVBoxLayout(page);
    }
    else
        topLayout->setContentsMargins(0, 0, 0, 0);
    auto hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addLayout(hlayout);
    auto colourLayout = new QVBoxLayout();
    colourLayout->setContentsMargins(0, 0, 0, 0);
    hlayout->addLayout(colourLayout);
    if (fg)
    {
        QWidget* box = new QWidget(page);    // to group widgets for QWhatsThis text
        colourLayout->addWidget(box);
        auto boxHLayout = new QHBoxLayout(box);
        boxHLayout->setContentsMargins(0, 0, 0, 0);

        QLabel* label = new QLabel(i18nc("@label:listbox", "Foreground color:"), box);
        boxHLayout->addWidget(label);
        boxHLayout->setStretchFactor(new QWidget(box), 0);
        mFgColourButton = new ColourButton(box);
        boxHLayout->addWidget(mFgColourButton);
        connect(mFgColourButton, &ColourButton::changed, this, &FontColourChooser::setSampleColour);
        label->setBuddy(mFgColourButton);
        box->setWhatsThis(i18nc("@info:whatsthis", "Select the alarm message foreground color"));
    }

    QWidget* box = new QWidget(page);    // to group widgets for QWhatsThis text
    colourLayout->addWidget(box);
    auto boxHLayout = new QHBoxLayout(box);
    boxHLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* label = new QLabel(i18nc("@label:listbox", "Background color:"), box);
    boxHLayout->addWidget(label);
    boxHLayout->setStretchFactor(new QWidget(box), 0);
    mBgColourButton = new ColourButton(box);
    boxHLayout->addWidget(mBgColourButton);
    connect(mBgColourButton, &ColourButton::changed, this, &FontColourChooser::setSampleColour);
    label->setBuddy(mBgColourButton);
    box->setWhatsThis(i18nc("@info:whatsthis", "Select the alarm message background color"));
    hlayout->addStretch();

    if (defaultFont)
    {
        auto layout = new QHBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        topLayout->addLayout(layout);
        mDefaultFont = new CheckBox(i18nc("@option:check", "Use default font"), page);
        mDefaultFont->setMinimumSize(mDefaultFont->sizeHint());
        connect(mDefaultFont, &CheckBox::toggled, this, &FontColourChooser::slotDefaultFontToggled);
        mDefaultFont->setWhatsThis(i18nc("@info:whatsthis", "Check to use the default font current at the time the alarm is displayed."));
        layout->addWidget(mDefaultFont);
        layout->addWidget(new QWidget(page));    // left adjust the widget
    }

#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 86, 0)
    mFontChooser = new KFontChooser(KFontChooser::DisplayFrame, page);
    mFontChooser->setFontListItems(fontList);
    mFontChooser->setMinVisibleItems(visibleListSize);
#else
    mFontChooser = new KFontChooser(page, KFontChooser::DisplayFrame, fontList, visibleListSize);
#endif

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

// vim: et sw=4:
