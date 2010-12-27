/*
 *  specialactions.h  -  widget to specify special alarm actions
 *  Program:  kalarm
 *  Copyright © 2004-2010 by David Jarvie <djarvie@kde.org>
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

#ifndef SPECIALACTIONS_H
#define SPECIALACTIONS_H

#include <kdialog.h>
#include <QWidget>
#include <QPushButton>

class QResizeEvent;
class KLineEdit;
class CheckBox;


class SpecialActionsButton : public QPushButton
{
        Q_OBJECT
    public:
        explicit SpecialActionsButton(bool enableCheckboxes, QWidget* parent = 0);
        void           setActions(const QString& pre, const QString& post, bool cancelOnError, bool dontShowError);
        const QString& preAction() const      { return mPreAction; }
        const QString& postAction() const     { return mPostAction; }
        bool           cancelOnError() const  { return mCancelOnError; }
        bool           dontShowError() const  { return mDontShowError; }
        virtual void   setReadOnly(bool ro)   { mReadOnly = ro; }
        virtual bool   isReadOnly() const     { return mReadOnly; }

    signals:
        /** Signal emitted whenever the widget has been changed. */
        void           selected();

    protected slots:
        void           slotButtonPressed();

    private:
        QString  mPreAction;
        QString  mPostAction;
        bool     mCancelOnError;
        bool     mDontShowError;
        bool     mEnableCheckboxes;
        bool     mReadOnly;
};


// Pre- and post-alarm actions widget
class SpecialActions : public QWidget
{
        Q_OBJECT
    public:
        explicit SpecialActions(bool enableCheckboxes, QWidget* parent = 0);
        void         setActions(const QString& pre, const QString& post, bool cancelOnError, bool dontShowError);
        QString      preAction() const;
        QString      postAction() const;
        bool         cancelOnError() const;
        bool         dontShowError() const;
        void         setReadOnly(bool);
        bool         isReadOnly() const    { return mReadOnly; }

    private slots:
        void         slotPreActionChanged(const QString& text);

    private:
        KLineEdit*   mPreAction;
        KLineEdit*   mPostAction;
        CheckBox*    mCancelOnError;
        CheckBox*    mDontShowError;
        bool         mEnableCheckboxes;   // enable checkboxes even if mPreAction is blank
        bool         mReadOnly;
};


// Pre- and post-alarm actions dialog displayed by the push button
class SpecialActionsDlg : public KDialog
{
        Q_OBJECT
    public:
        SpecialActionsDlg(const QString& preAction, const QString& postAction,
                          bool cancelOnError, bool enableCheckboxes, bool dontShowError,
                          QWidget* parent = 0);
        QString      preAction() const     { return mActions->preAction(); }
        QString      postAction() const    { return mActions->postAction(); }
        bool         cancelOnError() const { return mActions->cancelOnError(); }
        bool         dontShowError() const { return mActions->dontShowError(); }
        void         setReadOnly(bool ro)  { mActions->setReadOnly(ro); }
        bool         isReadOnly() const    { return mActions->isReadOnly(); }

    protected:
        virtual void resizeEvent(QResizeEvent*);

    protected slots:
        virtual void slotOk();

    private:
        SpecialActions* mActions;
};

#endif // SPECIALACTIONS_H

// vim: et sw=4:
