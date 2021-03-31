/*
 *  colourbutton.h  -  colour selection button
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KColorButton>


/**
 *  @short A colour selection button with read-only option.
 *
 *  The ColourButton class is a KColorButton with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ColourButton : public KColorButton
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit ColourButton(QWidget* parent = nullptr);
        /** Returns the selected colour. */
        QColor       colour() const              { return color(); }
        /** Sets the selected colour to @p c. */
        void         setColour(const QColor& c)  { setColor(c); }
        /** Returns true if the widget is read only. */
        bool         isReadOnly() const          { return mReadOnly; }
        /** Sets whether the button can be changed by the user.
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
        bool         mReadOnly {false};        // value cannot be changed
};


// vim: et sw=4:
