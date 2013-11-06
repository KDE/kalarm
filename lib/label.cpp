/*
 *  label.cpp  -  label with radiobutton buddy option
 *  Program:  kalarm
 *  Copyright © 2004,2005 by David Jarvie <djarvie@kde.org>
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
#include "label.h"
#include <QRadioButton>


Label::Label(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent, f),
      mRadioButton(0),
      mFocusWidget(0)
{ }

Label::Label(const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f),
      mRadioButton(0),
      mFocusWidget(0)
{ }

Label::Label(QWidget* buddy, const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f),
      mRadioButton(0),
      mFocusWidget(0)
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
        disconnect(mRadioButton, SIGNAL(destroyed()), this, SLOT(buddyDead()));
    QWidget* w = bud;
    if (w)
    {
        while (w->focusProxy())
            w = w->focusProxy();
        if (!qobject_cast<QRadioButton*>(w))
            w = 0;
    }
    if (!w)
    {
        // The buddy widget isn't a radio button
        QLabel::setBuddy(bud);
        delete mFocusWidget;
        mFocusWidget = 0;
        mRadioButton = 0;
    }
    else
    {
        // The buddy widget is a radio button, so set a different buddy
        if (!mFocusWidget)
            mFocusWidget = new LabelFocusWidget(this);
        QLabel::setBuddy(mFocusWidget);
        mRadioButton = (QRadioButton*)bud;
        connect(mRadioButton, SIGNAL(destroyed()), this, SLOT(buddyDead()));
    }
}

void Label::buddyDead()
{
    delete mFocusWidget;
    mFocusWidget = 0;
    mRadioButton = 0;
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
#include "moc_label.cpp"
// vim: et sw=4:
