/*
 *  specialactions.h  -  widget to specify special alarm actions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KAlarmCal/KAEvent>

#include <QDialog>
#include <QWidget>
#include <QPushButton>

using namespace KAlarmCal;

class QResizeEvent;
class QLineEdit;
class CheckBox;


class SpecialActionsButton : public QPushButton
{
        Q_OBJECT
    public:
        explicit SpecialActionsButton(bool enableCheckboxes, QWidget* parent = nullptr);
        void           setActions(const QString& pre, const QString& post, KAEvent::ExtraActionOptions);
        const QString& preAction() const      { return mPreAction; }
        const QString& postAction() const     { return mPostAction; }
        KAEvent::ExtraActionOptions options() const  { return mOptions; }
        virtual void   setReadOnly(bool ro)   { mReadOnly = ro; }
        virtual bool   isReadOnly() const     { return mReadOnly; }

    Q_SIGNALS:
        /** Signal emitted whenever the widget has been changed. */
        void           selected();

    protected Q_SLOTS:
        void           slotButtonPressed();

    private:
        QString  mPreAction;
        QString  mPostAction;
        KAEvent::ExtraActionOptions mOptions;
        bool     mEnableCheckboxes;
        bool     mReadOnly {false};
};


// Pre- and post-alarm actions widget
class SpecialActions : public QWidget
{
        Q_OBJECT
    public:
        explicit SpecialActions(bool enableCheckboxes, QWidget* parent = nullptr);
        void         setActions(const QString& pre, const QString& post, KAEvent::ExtraActionOptions);
        QString      preAction() const;
        QString      postAction() const;
        KAEvent::ExtraActionOptions options() const;
        void         setReadOnly(bool);
        bool         isReadOnly() const    { return mReadOnly; }

    private Q_SLOTS:
        void         slotPreActionChanged(const QString& text);

    private:
        QLineEdit*   mPreAction;
        QLineEdit*   mPostAction;
        CheckBox*    mCancelOnError;
        CheckBox*    mDontShowError;
        CheckBox*    mExecOnDeferral;
        bool         mEnableCheckboxes;   // enable checkboxes even if mPreAction is blank
        bool         mReadOnly {false};
};


// Pre- and post-alarm actions dialog displayed by the push button
class SpecialActionsDlg : public QDialog
{
        Q_OBJECT
    public:
        SpecialActionsDlg(const QString& preAction, const QString& postAction,
                          KAEvent::ExtraActionOptions, bool enableCheckboxes,
                          QWidget* parent = nullptr);
        QString        preAction() const     { return mActions->preAction(); }
        QString        postAction() const    { return mActions->postAction(); }
        KAEvent::ExtraActionOptions options() const  { return mActions->options(); }
        void           setReadOnly(bool ro)  { mActions->setReadOnly(ro); }
        bool           isReadOnly() const    { return mActions->isReadOnly(); }

    protected:
        void           resizeEvent(QResizeEvent*) override;

    protected Q_SLOTS:
        virtual void slotOk();

    private:
        SpecialActions* mActions;
};


// vim: et sw=4:
