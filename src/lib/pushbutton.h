/*
 *  pushbutton.h  -  push button with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PUSHBUTTON_H
#define PUSHBUTTON_H

#include <QPushButton>
class QMouseEvent;
class QKeyEvent;
class KGuiItem;
class QIcon;


/**
 *  @short A QPushButton with read-only option.
 *
 *  The PushButton class is a QPushButton with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class PushButton : public QPushButton
{
        Q_OBJECT
        Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit PushButton(QWidget* parent);
        /** Constructor for a push button which displays a text.
         *  @param gui    The text, icon etc. to show on the button.
         *  @param parent The parent object of this widget.
         */
        PushButton(const KGuiItem& gui, QWidget* parent);
        /** Constructor for a push button which displays a text.
         *  @param text The text to show on the button.
         *  @param parent The parent object of this widget.
         */
        PushButton(const QString& text, QWidget* parent);
        /** Constructor for a push button which displays an icon and a text.
         *  @param icon The icon to show on the button.
         *  @param text The text to show on the button.
         *  @param parent The parent object of this widget.
         */
        PushButton(const QIcon &icon, const QString& text, QWidget* parent);
        /** Sets whether the push button is read-only for the user.
         *  @param readOnly True to set the widget read-only, false to enable its action.
         *  @param noHighlight True to prevent the button being highlighted on mouse-over.
         */
        virtual void setReadOnly(bool readOnly, bool noHighlight = false);
        /** Returns true if the widget is read only. */
        virtual bool isReadOnly() const  { return mReadOnly; }
    protected:
        void         mousePressEvent(QMouseEvent*) override;
        void         mouseReleaseEvent(QMouseEvent*) override;
        void         mouseMoveEvent(QMouseEvent*) override;
        void         keyPressEvent(QKeyEvent*) override;
        void         keyReleaseEvent(QKeyEvent*) override;
        bool         event(QEvent*) override;
    private:
        Qt::FocusPolicy mFocusPolicy;          // default focus policy for the PushButton
        bool            mReadOnly {false};     // value cannot be changed
        bool            mNoHighlight {false};  // don't highlight on mouse hover, if read-only
};

#endif // PUSHBUTTON_H

// vim: et sw=4:
