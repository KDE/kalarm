/*
 *  spinbox_p.h  -  private classes for SpinBox
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QStyle>


/*=============================================================================
= Class SpinBoxStyle
* Extra pair of spin buttons for SpinBox2.
* The widget is actually a whole spin box, but only the buttons are displayed.
=============================================================================*/

class SpinBoxStyle : public QStyle
{
    Q_OBJECT
public:
    SpinBoxStyle();

    void polish(QWidget* widget) override;
    void unpolish(QWidget* widget) override;

    void polish(QApplication* application) override;
    void unpolish(QApplication* application) override;

    void polish(QPalette& palette) override;

    QRect itemTextRect(const QFontMetrics& fm, const QRect& r,
                           int flags, bool enabled,
                           const QString& text) const override;

    QRect itemPixmapRect(const QRect& r, int flags, const QPixmap& pixmap) const override;

    void drawItemText(QPainter* painter, const QRect& rect,
                              int flags, const QPalette& pal, bool enabled,
                              const QString& text, QPalette::ColorRole textRole = QPalette::NoRole) const override;

    void drawItemPixmap(QPainter* painter, const QRect& rect,
                                int alignment, const QPixmap& pixmap) const override;

    QPalette standardPalette() const override;

    void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                               const QWidget* w = nullptr) const override;

    void drawControl(ControlElement element, const QStyleOption* opt, QPainter* p,
                             const QWidget* w = nullptr) const override;

    QRect subElementRect(SubElement subElement, const QStyleOption* option,
                                 const QWidget* widget = nullptr) const override;

    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex* opt, QPainter* p,
                                    const QWidget* widget = nullptr) const override;
    SubControl hitTestComplexControl(ComplexControl cc, const QStyleOptionComplex* opt,
                                             const QPoint& pt, const QWidget* widget = nullptr) const override;
    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex* opt,
                                 SubControl sc, const QWidget* widget = nullptr) const override;

    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                            const QWidget* widget = nullptr) const override;

    QSize sizeFromContents(ContentsType ct, const QStyleOption* opt,
                                   const QSize& contentsSize, const QWidget* w = nullptr) const override;

    int styleHint(StyleHint stylehint, const QStyleOption* opt = nullptr,
                          const QWidget* widget = nullptr, QStyleHintReturn* returnData = nullptr) const override;

    QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption* opt = nullptr,
                                   const QWidget* widget = nullptr) const override;

    QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption* option = nullptr,
                               const QWidget* widget = nullptr) const override;

    QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap& pixmap,
                                        const QStyleOption* opt) const override;

    int layoutSpacing(QSizePolicy::ControlType control1,
                              QSizePolicy::ControlType control2, Qt::Orientation orientation,
                              const QStyleOption* option = nullptr, const QWidget* widget = nullptr) const override;
};

// vim: et sw=4:
