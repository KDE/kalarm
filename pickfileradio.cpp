/*
 *  pickfileradio.cpp  -  radio button with an associated file picker
 *  Program:  kalarm
 *  Copyright © 2005,2009 by David Jarvie <djarvie@kde.org>
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
#include "pickfileradio.h"

#include "buttongroup.h"
#include "lineedit.h"

#include <QPushButton>
#include <QTimer>
#include <qdebug.h>


PickFileRadio::PickFileRadio(QPushButton* button, LineEdit* edit, const QString& text, ButtonGroup* group, QWidget* parent)
    : RadioButton(text, parent),
      mGroup(group),
      mEdit(0),
      mLastButton(0),
      mRevertButton(false)
{
    Q_ASSERT(parent);
    init(button, edit);
}

PickFileRadio::PickFileRadio(const QString& text, ButtonGroup* group, QWidget* parent)
    : RadioButton(text, parent),
      mGroup(group),
      mEdit(0),
      mButton(0),
      mLastButton(0),
      mRevertButton(false)
{
    Q_ASSERT(parent);
}

void PickFileRadio::init(QPushButton* button, LineEdit* edit)
{
    Q_ASSERT(button);
    if (mEdit)
        mEdit->disconnect(this);
    mEdit   = edit;
    mButton = button;
    mButton->setEnabled(false);
    connect(mButton, &QPushButton::clicked, this, &PickFileRadio::slotPickFile);
    if (mEdit)
    {
        mEdit->setEnabled(false);
        connect(mEdit, &LineEdit::textChanged, this, &PickFileRadio::fileChanged);
    }
    connect(mGroup, &ButtonGroup::buttonSet, this, &PickFileRadio::slotSelectionChanged);
    setReadOnly(RadioButton::isReadOnly());
}

void PickFileRadio::setReadOnly(bool ro)
{
    RadioButton::setReadOnly(ro);
    if (mButton)
    {
        if (mEdit)
            mEdit->setReadOnly(ro);
        if (ro)
            mButton->hide();
        else
            mButton->show();
    }
}

void PickFileRadio::setFile(const QString& file)
{
    mFile = file;
}

QString PickFileRadio::file() const
{
    return mEdit ? mEdit->text() : mFile;
}

/******************************************************************************
* Set the radio button enabled or disabled.
* Adjusts the enabled/disabled state of other controls appropriately.
*/
void PickFileRadio::setEnabled(bool enable)
{
    Q_ASSERT(mButton);
    RadioButton::setEnabled(enable);
    enable = enable  &&  mGroup->checkedButton() == this;
    if (enable)
    {
        if (!pickFileIfNone())
            enable = false;    // revert to previously selected type
    }
    mButton->setEnabled(enable);
    if (mEdit)
        mEdit->setEnabled(enable);
}

/******************************************************************************
* Called when the selected radio button changes.
*/
void PickFileRadio::slotSelectionChanged(QAbstractButton* button)
{
    if (button == mLastButton  ||  mRevertButton)
        return;
    if (mLastButton == this)
    {
        mButton->setEnabled(false);
        if (mEdit)
            mEdit->setEnabled(false);
    }
    else if (button == this)
    {
        if (!pickFileIfNone())
            return;    // revert to previously selected type
        mButton->setEnabled(true);
        if (mEdit)
            mEdit->setEnabled(true);
    }
    mLastButton = button;
}

/******************************************************************************
* Prompt for a file name if there is none currently entered.
*/
bool PickFileRadio::pickFileIfNone()
{
    if (mEdit)
        mFile = mEdit->text();
    if (!mFile.isEmpty())
        return true;
    return !slotPickFile().isEmpty();
}

/******************************************************************************
* Called when the file picker button is clicked.
* Reply = mFile, or null string if dialogue, and 'this', was deleted.
*/
QString PickFileRadio::slotPickFile()
{
    // To avoid crashes on application quit, we need to check whether the
    // dialogue, and hence this PickFileRadio, was deleted while active,
    // before accessing class members.
    QString file = pickFile();
    if (file.isNull())
        return file;   // 'this' is probably invalid now
    if (!file.isEmpty())
    {
        mFile = file;
        if (mEdit)
            mEdit->setText(mFile);
    }
    if (mFile.isEmpty())
    {
        // No file is selected, so revert to the previous radio button selection.
        // But wait a moment before setting the radio button, or it won't work.
        mRevertButton = true;   // prevent picker dialog popping up twice
        QTimer::singleShot(0, this, SLOT(setLastButton()));
    }
    return mFile;
}

/******************************************************************************
* Select the previously selected radio button in the group.
*/
void PickFileRadio::setLastButton()
{
    if (!mLastButton)
        setChecked(false);    // we don't know the previous selection, so just turn this button off
    else
        mLastButton->setChecked(true);
    mRevertButton = false;
}

// vim: et sw=4:
