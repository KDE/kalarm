/*
 *  combobox.h  -  combo box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002, 2005-2007 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KComboBox>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A KComboBox with read-only option.
 *
 *  The ComboBox class is a KComboBox with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ComboBox : public KComboBox
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit ComboBox(QWidget* parent = nullptr);
        /** Returns true if the widget is read only. */
        bool         isReadOnly() const          { return mReadOnly; }
        /** Sets whether the combo box is read-only for the user. If read-only,
         *  its state cannot be changed by the user.
         *  @param readOnly True to set the widget read-only, false to set it read-write.
         */
        virtual void setReadOnly(bool readOnly);
    protected:
        void         mousePressEvent(QMouseEvent*) override;
        void         mouseReleaseEvent(QMouseEvent*) override;
        void         mouseMoveEvent(QMouseEvent*) override;
        void         keyPressEvent(QKeyEvent*) override;
        void         keyReleaseEvent(QKeyEvent*) override;
    private:
        bool    mReadOnly {false};      // value cannot be changed
};


// vim: et sw=4:
