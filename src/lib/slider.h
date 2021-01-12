/*
 *  slider.h  -  slider control with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SLIDER_H
#define SLIDER_H

#include <QSlider>
class QLabel;
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A QSlider with read-only option and connection to a value label.
 *
 *  The Slider class is a QSlider with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class Slider : public QSlider
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
public:
    /** Constructor.
     *  @param parent The parent object of this widget.
     */
    explicit Slider(QWidget* parent = nullptr);

    /** Constructor.
     *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
     *  @param parent The parent object of this widget.
     */
    explicit Slider(Qt::Orientation orient, QWidget* parent = nullptr);

    /** Constructor.
     *  @param minValue The minimum value which the slider can have.
     *  @param maxValue The maximum value which the slider can have.
     *  @param pageStep The page step increment.
     *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
     *  @param parent The parent object of this widget.
     */
    Slider(int minValue, int maxValue, int pageStep, Qt::Orientation orient,
           QWidget* parent = nullptr);

    /** Returns true if the slider is read only. */
    bool         isReadOnly() const  { return mReadOnly; }

    /** Sets whether the slider is read-only for the user.
     *  @param readOnly True to set the widget read-only, false to set it read-write.
     */
    virtual void setReadOnly(bool readOnly);

    /** Set a label to display the slider's value.
     *  The label will be updated whenever the slider value changes.
     *  @param label           The label to display the slider's value.
     *  @param format          Format string for the value to display.
     *  @param hideIfDisabled  Hide the label if the slider is disabled.
     */
    void setValueLabel(QLabel* label, const QString& format = QStringLiteral("%1"), bool hideIfDisabled = false);

    void setVisible(bool) override;

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void changeEvent(QEvent*) override;

private Q_SLOTS:
    void valueHasChanged(int value);
    void valueLabelDestroyed(QObject*);

private:
    QLabel* mValueLabel {nullptr};  // label to display the slider's value
    QString mValueFormat;           // format used for mValueLabel
    bool    mValueLabelHide;        // hide mValueLabel if slider is disabled
    bool    mReadOnly {false};      // value cannot be changed by the user
};

#endif // SLIDER_H

// vim: et sw=4:
