/*
 *  fontcolour.h  -  font and colour chooser widget
 *  Program:  kalarm
 *  Copyright © 2001,2003,2008,2009 by David Jarvie <djarvie@kde.org>
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
        explicit FontColourChooser(QWidget* parent = 0,
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
        virtual bool      eventFilter(QObject*, QEvent*);

    private slots:
        void              setSampleColour();
        void              slotDefaultFontToggled(bool);

    private:
        ColourButton*    mFgColourButton;       // or null
        ColourButton*    mBgColourButton;
        KFontChooser*    mFontChooser;
        CheckBox*        mDefaultFont;          // or null
        bool             mReadOnly;
};

#endif

// vim: et sw=4:
