/*
 *  label.h  -  label with radiobutton buddy option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLabel>
class QFocusEvent;
class QRadioButton;
class LabelFocusWidget;

/**
 *  @short A QLabel with option for a buddy radio button.
 *
 *  The Label class provides a text display, with special behaviour when a radio
 *  button is set as a buddy.
 *
 *  The Label object in effect acts as if it were part of the buddy radio button,
 *  in that when the label's accelerator key is pressed, the radio button receives
 *  focus and is switched on. When a non-radio button is specified as a buddy, the
 *  behaviour is the same as for QLabel.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class Label : public QLabel
{
    Q_OBJECT
    friend class LabelFocusWidget;

public:
    /** Constructs an empty label.
     *  @param parent The parent object of this widget.
     *  @param f    Flags. See QWidget constructor for details.
     */
    explicit Label(QWidget* parent, Qt::WindowFlags f = {});

    /** Constructs a label that displays @p text.
     *  @param text   Text string to display.
     *  @param parent The parent object of this widget.
     *  @param f      Flags. See QWidget constructor for details.
     */
    Label(const QString& text, QWidget* parent, Qt::WindowFlags f = {});

    /** Constructs a label, with a buddy widget, that displays @p text.
     *  @param buddy  Buddy widget which receives the keyboard focus when the
     *                label's accelerator key is pressed. If @p buddy is a radio
     *                button, @p buddy is in addition selected when the
     *                accelerator key is pressed.
     *  @param text   Text string to display.
     *  @param parent The parent object of this widget.
     *  @param f      Flags. See QWidget constructor for details.
     */
    Label(QWidget* buddy, const QString& text, QWidget* parent, Qt::WindowFlags f = {});

    /** Sets the label's buddy widget which receives the keyboard focus when the
     *  label's accelerator key is pressed. If @p buddy is a radio button,
     *  @p buddy is in addition selected when the accelerator key is pressed.
     */
    virtual void      setBuddy(QWidget* buddy);

private Q_SLOTS:
    void              buddyDead();

private:
    void              activated();
    QRadioButton*     mRadioButton {nullptr};  // buddy widget if it's a radio button, else 0
    LabelFocusWidget* mFocusWidget {nullptr};
};


// Private class for use by Label
class LabelFocusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LabelFocusWidget(QWidget* parent);

protected:
    void focusInEvent(QFocusEvent*) override;
};

// vim: et sw=4:
