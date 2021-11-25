/*
 *  prefdlg.h  -  program preferences dialog
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KPageDialog>

class EditPrefTab;
class EmailPrefTab;
class ViewPrefTab;
class StorePrefTab;
class TimePrefTab;
class MiscPrefTab;
class StackedScrollGroup;


// The Preferences dialog
class KAlarmPrefDlg : public KPageDialog
{
    Q_OBJECT
public:
    static void display();
    ~KAlarmPrefDlg() override;
    QSize minimumSizeHint() const override;

protected:
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;

protected Q_SLOTS:
    virtual void slotOk();
    virtual void slotApply();
    virtual void slotHelp();
    virtual void slotDefault();
    virtual void slotCancel();
    void         slotTabChanged(KPageWidgetItem*);

private:
    KAlarmPrefDlg();
    void restore(bool defaults);
    void restoreTab();

    static KAlarmPrefDlg* mInstance;
    enum TabType { AnyTab, Misc, Time, Store, Edit, Email, View };
    static TabType        mLastTab;
    StackedScrollGroup* mTabScrollGroup;

    MiscPrefTab*        mMiscPage;
    TimePrefTab*        mTimePage;
    StorePrefTab*       mStorePage;
    EditPrefTab*        mEditPage;
    EmailPrefTab*       mEmailPage;
    ViewPrefTab*        mViewPage;

    KPageWidgetItem*    mMiscPageItem;
    KPageWidgetItem*    mTimePageItem;
    KPageWidgetItem*    mStorePageItem;
    KPageWidgetItem*    mEditPageItem;
    KPageWidgetItem*    mEmailPageItem;
    KPageWidgetItem*    mViewPageItem;

    bool                mShown {false};
    bool                mValid;
};

// vim: et sw=4:
