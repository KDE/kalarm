/*
 *  fontcolourbutton.h  -  pushbutton widget to select a font and colour
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FONTCOLOURBUTTON_H
#define FONTCOLOURBUTTON_H

#include "lib/pushbutton.h"

#include <QDialog>
#include <QFont>
#include <QColor>

class FontColourChooser;

class FontColourButton : public PushButton
{
    Q_OBJECT
public:
    explicit FontColourButton(QWidget* parent = nullptr);
    void          setDefaultFont();
    void          setFont(const QFont&);
    void          setBgColour(const QColor& c) { mBgColour = c; }
    void          setFgColour(const QColor& c) { mFgColour = c; }
    bool          defaultFont() const    { return mDefaultFont; }
    QFont         font() const           { return mDefaultFont ? QFont() : mFont; }
    QColor        bgColour() const       { return mBgColour; }
    QColor        fgColour() const       { return mFgColour; }
    void          setReadOnly(bool ro, bool noHighlight = false) override
                             { mReadOnly = ro; PushButton::setReadOnly(ro, noHighlight); }
    bool          isReadOnly() const override     { return mReadOnly; }

Q_SIGNALS:
    /** Signal emitted whenever a font or colour has been selected. */
    void          selected(const QColor& fg, const QColor& bg);

protected Q_SLOTS:
    void          slotButtonPressed();

private:
    QColor      mBgColour, mFgColour;
    QFont       mFont;
    bool        mDefaultFont {true};
    bool        mReadOnly {false};
};


// Font and colour selection dialog displayed by the push button
class FontColourDlg : public QDialog
{
    Q_OBJECT
public:
    FontColourDlg(const QColor& bg, const QColor& fg, const QFont&, bool defaultFont,
                  const QString& caption, QWidget* parent = nullptr);
    bool         defaultFont() const   { return mDefaultFont; }
    QFont        font() const          { return mFont; }
    QColor       bgColour() const      { return mBgColour; }
    QColor       fgColour() const      { return mFgColour; }
    void         setReadOnly(bool);
    bool         isReadOnly() const    { return mReadOnly; }

protected Q_SLOTS:
    virtual void slotOk();

private:
    FontColourChooser* mChooser;
    QColor             mBgColour, mFgColour;
    QFont              mFont;
    bool               mDefaultFont;
    bool               mReadOnly {false};
};

#endif // FONTCOLOURBUTTON_H

// vim: et sw=4:
