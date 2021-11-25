/*
 *  radiobutton.h  -  radio button with focus widget and read-only options
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QRadioButton>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A QRadioButton with focus widget and read-only options.
 *
 *  The RadioButton class is a QRadioButton with the ability to transfer focus to
 *  another widget when checked, and with a read-only option.
 *
 *  Another widget may be specified as the focus widget for the radio button. Whenever
 *  the user clicks on the radio button so as to set its state to checked, focus is
 *  automatically transferred to the focus widget.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class RadioButton : public QRadioButton
{
    Q_OBJECT
public:
    /** Constructor.
     *  @param parent The parent object of this widget.
     */
    explicit RadioButton(QWidget* parent = nullptr);

    /** Constructor.
     *  @param text Text to display.
     *  @param parent The parent object of this widget.
     */
    explicit RadioButton(const QString& text, QWidget* parent = nullptr);

    /** Returns true if the widget is read only. */
    bool isReadOnly() const          { return mReadOnly; }

    /** Sets whether the radio button is read-only for the user. If read-only,
     *  its state cannot be changed by the user.
     *  @param readOnly True to set the widget read-only, false to set it read-write.
     */
    virtual void setReadOnly(bool readOnly);

    /** Returns the widget which receives focus when the button is clicked. */
    QWidget* focusWidget() const         { return mFocusWidget; }

    /** Specifies a widget to receive focus when the button is clicked.
     *  @param widget Widget to receive focus.
     *  @param enable If true, @p widget will be enabled before receiving focus. If
     *                false, the enabled state of @p widget will be left unchanged when
     *                the radio button is clicked.
     */
    void setFocusWidget(QWidget* widget, bool enable = true);

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;

protected Q_SLOTS:
    void     slotClicked();

private:
    Qt::FocusPolicy mFocusPolicy;           // default focus policy for the QRadioButton
    QWidget*        mFocusWidget {nullptr}; // widget to receive focus when button is clicked on
    bool            mFocusWidgetEnable;     // enable focus widget before setting focus
    bool            mReadOnly {false};      // value cannot be changed
};

// vim: et sw=4:
