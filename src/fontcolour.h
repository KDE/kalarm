/*
 *  fontcolour.h  -  font and colour chooser widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001, 2003, 2008, 2009 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FONTCOLOUR_H
#define FONTCOLOUR_H

#include <KLocalizedString>
#include <QWidget>
#include <QStringList>

class KFontChooser;
class CheckBox;
class ColourButton;


class FontColourChooser : public QWidget
{
        Q_OBJECT
    public:
        explicit FontColourChooser(QWidget* parent = nullptr,
               const QStringList& fontList = QStringList(),
               const QString& frameLabel = i18n("Requested font"),
               bool fg = true, bool defaultFont = false, int visibleListSize = 8);

        void              setDefaultFont();
        void              setFont(const QFont&, bool onlyFixed = false);
        bool              defaultFont() const;
        QFont             font() const;
        QColor            fgColour() const;
        QColor            bgColour() const;
        void              setFgColour(const QColor&);
        void              setBgColour(const QColor&);
        QString           sampleText() const;
        void              setSampleText(const QString& text);
        bool              isReadOnly() const     { return mReadOnly; }
        void              setReadOnly(bool);
        bool              eventFilter(QObject*, QEvent*) override;

    private Q_SLOTS:
        void              setSampleColour();
        void              slotDefaultFontToggled(bool);

    private:
        ColourButton*    mFgColourButton {nullptr};
        ColourButton*    mBgColourButton;
        KFontChooser*    mFontChooser;
        CheckBox*        mDefaultFont {nullptr};
        bool             mReadOnly {false};
};

#endif

// vim: et sw=4:
