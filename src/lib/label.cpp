/*
 *  label.cpp  -  label with radiobutton buddy option
 *  Program:  kalarm
 *  Copyright © 2004-2019 David Jarvie <djarvie@kde.org>
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
    setBuddy(buddy);
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
        mRadioButton = (QRadioButton*)bud;
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
    Label* parent = (Label*)parentWidget();
    parent->activated();

}

// vim: et sw=4:
