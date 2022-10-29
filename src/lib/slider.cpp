/*
 *  slider.cpp  -  slider control with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "slider.h"

#include <QLabel>
#include <QMouseEvent>
#include <QLocale>


Slider::Slider(QWidget* parent)
    : QSlider(parent)
{ }

Slider::Slider(Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent)
{ }

Slider::Slider(int minval, int maxval, int pageStep, Qt::Orientation o, QWidget* parent)
    : QSlider(o, parent)
{
    setRange(minval, maxval);
    setPageStep(pageStep);
}

/******************************************************************************
* Set the read-only status. If read-only, the slider can be moved by the
* application, but not by the user.
*/
void Slider::setReadOnly(bool ro)
{
    mReadOnly = ro;
}

/******************************************************************************
* Set a label to contain the slider's value.
*/
void Slider::setValueLabel(QLabel* label, const QString& format, bool hideIfDisabled)
{
    if (label != mValueLabel)
        delete mValueLabel;    // delete any existing label
    mValueLabel = label;
    if (mValueLabel)
    {
        mValueLabel->setParent(this);
        mValueFormat = format;
        mValueLabelHide = hideIfDisabled;
        connect(this, &QAbstractSlider::valueChanged, this, &Slider::valueHasChanged);
        connect(mValueLabel, &QObject::destroyed, this, &Slider::valueLabelDestroyed);

        // Substitute any '%' character with the locale's percent symbol.
        for (int i = 0;  i < mValueFormat.size();  ++i)
        {
            if (mValueFormat.at(i) == QLatin1Char('%'))
            {
                if (i < mValueFormat.size() - 1  &&  mValueFormat.at(i + 1) == QLatin1Char('1'))
                {
                    // Convert "%1" to "%L1" to display the value in localised form.
                    mValueFormat.insert(++i, QLatin1Char('L'));
                    continue;
                }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                mValueFormat[i] = QLocale().percent();
#else
                mValueFormat.replace(i, 1, QLocale().percent());
#endif
            }
        }
    }
    else
        disconnect(this, &QAbstractSlider::valueChanged, this, &Slider::valueHasChanged);
}

/******************************************************************************
* Sets the visibility of the slider.
* This also sets the visibility of the value label.
*/
void Slider::setVisible(bool vis)
{
    QSlider::setVisible(vis);
    if (mValueLabel)
    {
        if (mValueLabelHide  &&  !isEnabled())
            vis = false;
        mValueLabel->setVisible(vis);
    }
}

/******************************************************************************
* Called when the slider's status has changed.
* If it is enabled or disabled, show or hide the value label if required.
*/
void Slider::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::EnabledChange)
    {
        if (mValueLabel  &&  mValueLabelHide  &&  isVisible())
            mValueLabel->setVisible(isEnabled());
    }
}

/******************************************************************************
* Called when the slider's value has changed.
*/
void Slider::valueHasChanged(int value)
{
    if (mValueLabel)
        mValueLabel->setText(mValueFormat.arg(value));
}

/******************************************************************************
* Called when the value label is destroyed.
*/
void Slider::valueLabelDestroyed(QObject* obj)
{
    if (obj == mValueLabel)
        mValueLabel = nullptr;
}

/******************************************************************************
* Event handlers to intercept events if in read-only mode.
* Any events which could change the slider value are discarded.
*/
void Slider::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    QSlider::mousePressEvent(e);
}

void Slider::mouseReleaseEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QSlider::mouseReleaseEvent(e);
}

void Slider::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        QSlider::mouseMoveEvent(e);
}

void Slider::keyPressEvent(QKeyEvent* e)
{
    if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
        QSlider::keyPressEvent(e);
}

void Slider::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        QSlider::keyReleaseEvent(e);
}

// vim: et sw=4:
