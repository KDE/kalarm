/*
 *  alarmlistdelegate.cpp  -  handles editing and display of alarm list
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "alarmlistdelegate.h"

#include "functions.h"
#include "resources/eventmodel.h"
#include "resources/resourcedatamodelbase.h"

#include <KColorScheme>

#include <QPainter>


void AlarmListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    if (index.isValid())
    {
        if (opt.state & QStyle::State_Selected
        &&  !index.data(ResourceDataModelBase::EnabledRole).toBool())
        {
            // Make the text colour for selected disabled alarms
            // distinguishable from enabled alarms.
            KColorScheme::adjustForeground(opt.palette, KColorScheme::InactiveText, QPalette::HighlightedText, KColorScheme::Selection);
        }
        switch (index.column())
        {
            case AlarmListModel::TimeColumn:
            {
                const QString str = index.data(ResourceDataModelBase::TimeDisplayRole).toString();
                // Need to pad out spacing to align times without leading zeroes
                const int i = str.indexOf(QLatin1Char('~'));    // look for indicator of a leading zero to be omitted
                if (i >= 0)
                {
                    painter->save();
                    opt.displayAlignment = Qt::AlignLeft;
                    const QVariant value = index.data(Qt::ForegroundRole);
                    if (value.isValid())
                        opt.palette.setColor(QPalette::Text, value.value<QColor>());
                    drawBackground(painter, opt, index);
                    const QString time = str.mid(i + 1);
                    QRect timeRect;
                    if (i > 0)
                    {
                        QString str0 = str;
                        str0[i] = QLatin1Char('0');
                        QRect displayRect = textRect(str0, painter, opt);
                        timeRect = textRect(time, painter, opt);
                        timeRect.moveRight(displayRect.right());
                        drawDisplay(painter, opt, displayRect, str.left(i));   // date
                    }
                    else
                        timeRect = textRect(time, painter, opt);
                    drawDisplay(painter, opt, timeRect, time);
                    painter->restore();
                    return;
                }
                break;
            }
            case AlarmListModel::ColourColumn:
            {
                const KAEvent event = static_cast<const EventListModel*>(index.model())->event(index);
                if (event.isValid())
                {
                    if (event.commandError() == KAEvent::CmdErr::None)
                    {
                        if (event.actionTypes() & KAEvent::Action::Display)
                            opt.palette.setColor(QPalette::Highlight, event.bgColour());
                    }
                    else
                    {
                        opt.font.setBold(true);
                        opt.font.setStyleHint(QFont::Serif);
                        opt.font.setPointSize(opt.rect.height() - 2);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    QItemDelegate::paint(painter, opt, index);
}

QSize AlarmListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.isValid())
    {
        switch (index.column())
        {
            case AlarmListModel::ColourColumn:
            {
                int h = option.fontMetrics.lineSpacing();
                return {h * 3 / 4, h};
            }
        }
    }
    return QItemDelegate::sizeHint(option, index);
}

void AlarmListDelegate::edit(KAEvent& event, EventListView* view)
{
    KAlarm::editAlarm(event, static_cast<AlarmListView*>(view));   // edit alarm (view-only mode if archived or read-only)
}

QRect AlarmListDelegate::textRect(const QString& text, QPainter* painter, const QStyleOptionViewItem& opt) const
{
    QRect displayRect;
    QRect r = opt.rect;
    r.setWidth(INT_MAX/256);
    displayRect = textRectangle(painter, r, opt.font, text);
    displayRect = QStyle::alignedRect(opt.direction, opt.displayAlignment,
                                      displayRect.size().boundedTo(opt.rect.size()),
                                      opt.rect);
    return displayRect;
}

// vim: et sw=4:
