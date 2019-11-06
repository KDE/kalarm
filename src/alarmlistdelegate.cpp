/*
 *  alarmlistdelegate.cpp  -  handles editing and display of alarm list
 *  Program:  kalarm
 *  Copyright © 2007-2011,2018 David Jarvie <djarvie@kde.org>
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

#include "alarmlistdelegate.h"

#include "akonadimodel.h"
#include "functions.h"
#include "kalarm_debug.h"

#include <kalarmcal/kacalendar.h>

#include <kcolorscheme.h>

#include <QPainter>


void AlarmListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    if (index.isValid())
    {
        if (opt.state & QStyle::State_Selected
        &&  !index.data(CalendarDataModel::EnabledRole).toBool())
        {
            // Make the text colour for selected disabled alarms
            // distinguishable from enabled alarms.
            KColorScheme::adjustForeground(opt.palette, KColorScheme::InactiveText, QPalette::HighlightedText, KColorScheme::Selection);
        }
        switch (index.column())
        {
            case AlarmListModel::TimeColumn:
            {
                const QString str = index.data(CalendarDataModel::TimeDisplayRole).toString();
                // Need to pad out spacing to align times without leading zeroes
                int i = str.indexOf(QLatin1Char('~'));    // look for indicator of a leading zero to be omitted
                if (i >= 0)
                {
                    painter->save();
                    opt.displayAlignment = Qt::AlignRight;
                    const QVariant value = index.data(Qt::ForegroundRole);
                    if (value.isValid())
                        opt.palette.setColor(QPalette::Text, value.value<QColor>());
                    QRect displayRect;
                    {
                        QString str0 = str;
                        str0[i] = QLatin1Char('0');
                        QRect r = opt.rect;
                        r.setWidth(INT_MAX/256);
                        displayRect = textRectangle(painter, r, opt.font, str0);
                        displayRect = QStyle::alignedRect(opt.direction, opt.displayAlignment,
                                                          displayRect.size().boundedTo(opt.rect.size()),
                                                          opt.rect);
                    }
                    const QString date = str.left(i);
                    const QString time = str.mid(i + 1);
                    if (i > 0)
                    {
                        opt.displayAlignment = Qt::AlignLeft;
                        drawDisplay(painter, opt, displayRect, date);
                        opt.displayAlignment = Qt::AlignRight;
                    }
                    drawDisplay(painter, opt, displayRect, time);
                    painter->restore();
                    return;
                }
                break;
            }
            case AlarmListModel::ColourColumn:
            {
                const KAEvent event = static_cast<const ItemListModel*>(index.model())->event(index);
                if (event.isValid()  &&  event.commandError() != KAEvent::CMD_NO_ERROR)
                {
                    opt.font.setBold(true);
                    opt.font.setStyleHint(QFont::Serif);
                    opt.font.setPointSize(opt.rect.height() - 2);
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
                return QSize(h * 3 / 4, h);
            }
        }
    }
    return QItemDelegate::sizeHint(option, index);
}

void AlarmListDelegate::edit(KAEvent* event, EventListView* view)
{
    KAlarm::editAlarm(event, static_cast<AlarmListView*>(view));   // edit alarm (view-only mode if archived or read-only)
}

// vim: et sw=4:
