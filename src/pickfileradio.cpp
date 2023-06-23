/*
 *  pickfileradio.cpp  -  radio button with an associated file picker
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pickfileradio.h"

#include "lib/buttongroup.h"
#include "lib/lineedit.h"

#include <QPushButton>
#include <QTimer>


PickFileRadio::PickFileRadio(QPushButton* button, LineEdit* edit, const QString& text, ButtonGroup* group, QWidget* parent)
    : RadioButton(text, parent)
    , mGroup(group)
{
    Q_ASSERT(parent);
    init(button, edit);
}

PickFileRadio::PickFileRadio(const QString& text, ButtonGroup* group, QWidget* parent)
    : RadioButton(text, parent)
    , mGroup(group)
    , mButton(nullptr)
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
    PickFileRadio::setReadOnly(RadioButton::isReadOnly());   // avoid calling virtual method from constructor
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
    QString file;
    if (!doPickFile(file))
       return false;
    return !mFile.isEmpty();
}

/******************************************************************************
* Called when the file picker button is clicked.
*/
void PickFileRadio::slotPickFile()
{
    QString file;
    doPickFile(file);
}

/******************************************************************************
* Called when the file picker button is clicked.
* @param file  Updated with the file which was selected, or empty if no file
*              was selected.
* Reply = true if 'file' value can be used.
*       = false if the dialogue was deleted while visible (indicating that
*         the parent widget was probably also deleted).
*/
bool PickFileRadio::doPickFile(QString& file)
{
    // To avoid crashes on application quit, we need to check whether the
    // dialogue, and hence this PickFileRadio, was deleted while active,
    // before accessing class members.
    file.clear();
    if (!pickFile(file))
        return false;   // 'this' is probably invalid now
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
        QTimer::singleShot(0, this, &PickFileRadio::setLastButton);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
    file = mFile;
    return true;
}

/******************************************************************************
* Select the previously selected radio button in the group.
*/
void PickFileRadio::setLastButton()
{
    if (!mLastButton)
        setChecked(false);   // we don't know the previous selection, so just turn this button off
    else
        mLastButton->setChecked(true);
    mRevertButton = false;
}

#include "moc_pickfileradio.cpp"

// vim: et sw=4:
