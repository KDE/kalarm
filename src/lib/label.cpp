/*
 *  label.cpp  -  label with radiobutton buddy option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "label.h"

#include <QRadioButton>


Label::Label(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent, f)
{ }

Label::Label(const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f)
{ }

Label::Label(QWidget* buddy, const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f)
{
    Label::setBuddy(buddy);
}

/******************************************************************************
* Set a buddy widget.
* If it (or its focus proxy) is a radio button, create a focus widget.
* When the accelerator key is pressed, the focus widget then receives focus.
* That event triggers the selection of the radio button.
*/
void Label::setBuddy(QWidget* bud)
{
    if (mRadioButton)
        disconnect(mRadioButton, &QRadioButton::destroyed, this, &Label::buddyDead);
    QWidget* w = bud;
    if (w)
    {
        while (w->focusProxy())
            w = w->focusProxy();
        if (!qobject_cast<QRadioButton*>(w))
            w = nullptr;
    }
    if (!w)
    {
        // The buddy widget isn't a radio button
        QLabel::setBuddy(bud);
        delete mFocusWidget;
        mFocusWidget = nullptr;
        mRadioButton = nullptr;
    }
    else
    {
        // The buddy widget is a radio button, so set a different buddy
        if (!mFocusWidget)
            mFocusWidget = new LabelFocusWidget(this);
        QLabel::setBuddy(mFocusWidget);
        mRadioButton = static_cast<QRadioButton*>(bud);
        connect(mRadioButton, &QRadioButton::destroyed, this, &Label::buddyDead);
    }
}

void Label::buddyDead()
{
    delete mFocusWidget;
    mFocusWidget = nullptr;
    mRadioButton = nullptr;
}

/******************************************************************************
* Called when focus is transferred to the label's special focus widget.
* Transfer focus to the radio button and select it.
*/
void Label::activated()
{
    if (mFocusWidget  &&  mRadioButton)
    {
        mRadioButton->setFocus();
        mRadioButton->setChecked(true);
    }
}


/*=============================================================================
* Class: LabelFocusWidget
=============================================================================*/

LabelFocusWidget::LabelFocusWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::ClickFocus);
    setFixedSize(QSize(1,1));
}

void LabelFocusWidget::focusInEvent(QFocusEvent*)
{
    auto* parent = static_cast<Label*>(parentWidget());
    parent->activated();

}

// vim: et sw=4:
