/*
 *  groupbox.h  -  checkable group box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011, 2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QGroupBox>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A QGroupBox with read-only option.
 *
 *  The GroupBox class is a QGroupBox with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class GroupBox : public QGroupBox
{
        Q_OBJECT
        Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit GroupBox(QWidget* parent = nullptr);
        /** Constructor for a group box with a title text.
         *  @param title The title text.
         *  @param parent The parent object of this widget.
         */
        explicit GroupBox(const QString& title, QWidget* parent = nullptr);
        /** Sets whether the group box is read-only for the user.
         *  @param readOnly True to set the widget read-only, false to enable its action.
         */
        virtual void setReadOnly(bool readOnly);
        /** Returns true if the widget is read only. */
        virtual bool isReadOnly() const  { return mReadOnly; }
    protected:
        void         mousePressEvent(QMouseEvent*) override;
        void         mouseReleaseEvent(QMouseEvent*) override;
        void         mouseMoveEvent(QMouseEvent*) override;
        void         keyPressEvent(QKeyEvent*) override;
        void         keyReleaseEvent(QKeyEvent*) override;
    private:
        bool  mReadOnly {false};      // value cannot be changed
};


// vim: et sw=4:
