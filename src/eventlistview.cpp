/*
 *  eventlistview.cpp  -  base class for widget showing list of alarms
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "eventlistview.h"

#include "find.h"
#include "resources/eventmodel.h"
#include "resources/resourcedatamodelbase.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QHeaderView>
#include <QMouseEvent>
#include <QToolTip>
#include <QApplication>


EventListView::EventListView(QWidget* parent)
    : QTreeView(parent)
{
    setRootIsDecorated(false);    // don't show expander icons for child-less items
    setAllColumnsShowFocus(true);
    setSelectionMode(ExtendedSelection);
    setSelectionBehavior(SelectRows);
    setTextElideMode(Qt::ElideRight);
    // Set default WhatsThis text to be displayed when no actual item is clicked on
    setWhatsThis(i18nc("@info:whatsthis", "List of scheduled alarms"));
    header()->setSortIndicatorShown(true);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &EventListView::sortChanged);
    header()->setSectionsClickable(true);
}

void EventListView::setModel(QAbstractItemModel* model)
{
    auto elm = qobject_cast<EventListModel*>(model);
    Q_ASSERT(elm);   // model must be derived from EventListModel

    QTreeView::setModel(model);
    connect(elm, &EventListModel::haveEventsStatus, this, &EventListView::initSections);
}

/******************************************************************************
* Return the source data model.
*/
EventListModel* EventListView::itemModel() const
{
    return static_cast<EventListModel*>(model());
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent EventListView::event(const QModelIndex& index) const
{
    return itemModel()->event(index);
}

KAEvent EventListView::event(int row) const
{
    return itemModel()->event(itemModel()->index(row, 0));
}

/******************************************************************************
* Select one event and make it the current item.
*/
void EventListView::select(const QModelIndex& index, bool scrollToIndex)
{
    selectionModel()->select(index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    if (scrollToIndex)
        scrollTo(index);
}

/******************************************************************************
* Return the single selected item.
* Reply = invalid if no items are selected, or if multiple items are selected.
*/
QModelIndex EventListView::selectedIndex() const
{
    const QModelIndexList list = selectionModel()->selectedRows();      //clazy:exclude=inefficient-qlist
    if (list.count() != 1)
        return {};
    return list[0];
}

/******************************************************************************
* Return the single selected event.
* Reply = null if no items are selected, or if multiple items are selected.
*/
KAEvent EventListView::selectedEvent() const
{
    const QModelIndexList list = selectionModel()->selectedRows();      //clazy:exclude=inefficient-qlist
    if (list.count() != 1)
        return {};
    const auto model = static_cast<const EventListModel*>(list[0].model());
    return model->event(list[0]);
}

/******************************************************************************
* Return the selected events.
*/
QList<KAEvent> EventListView::selectedEvents() const {
    QList<KAEvent> elist;
    const QModelIndexList ixlist = selectionModel()->selectedRows();      //clazy:exclude=inefficient-qlist
    int count = ixlist.count();
    if (count)
    {
        const auto model = static_cast<const EventListModel*>(ixlist[0].model());
        elist.reserve(count);
        for (int i = 0;  i < count;  ++i)
            elist += model->event(ixlist[i]);
    }
    return elist;
}

/******************************************************************************
* Called when the Find action is selected.
* Display the non-modal Find dialog.
*/
void EventListView::slotFind()
{
    if (!mFind)
    {
        mFind = new Find(this);
        connect(mFind, &Find::active, this, &EventListView::findActive);
    }
    mFind->display();
}

/******************************************************************************
* Called when the Find Next or Find Prev action is selected.
*/
void EventListView::findNext(bool forward)
{
    if (mFind)
        mFind->findNext(forward);
}

/******************************************************************************
* Called when the user has changed the sort order in a column.
*/
void EventListView::sortChanged(int column, Qt::SortOrder order)
{
    sortByColumn(column, order);
}

void EventListView::resizeEvent(QResizeEvent* se)
{
    QTreeView::resizeEvent(se);
    initSections();
}

/******************************************************************************
* Called when a ToolTip or WhatsThis event occurs.
*/
bool EventListView::viewportEvent(QEvent* e)
{
    if (e->type() == QEvent::ToolTip  &&  isActiveWindow())
    {
        auto he = static_cast<QHelpEvent*>(e);
        const QModelIndex index = indexAt(he->pos());
        QVariant value = model()->data(index, Qt::ToolTipRole);
        if (value.canConvert<QString>())
        {
            QString toolTip = value.toString();
            int i = toolTip.indexOf(QLatin1Char('\n'));
            if (i < 0)
            {
                auto m = qobject_cast<EventListModel*>(model());
                if (index.column() == ResourceDataModelBase::TextColumn
                ||  index.column() == ResourceDataModelBase::NameColumn
                ||  !m  ||  m->event(index).commandError() == KAEvent::CMD_NO_ERROR)
                {
                    // Single line tooltip. Only display it if the text column
                    // is truncated in the view display.
                    value = model()->data(index, Qt::FontRole);
                    const QFontMetrics fm(qvariant_cast<QFont>(value).resolve(listViewOptions().font));
                    const int textWidth = fm.boundingRect(toolTip).width() + 1;
                    const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
                    const int left = columnViewportPosition(index.column()) + margin;
                    const int right = left + textWidth;
                    if (left >= 0  &&  right <= width() - 2*frameWidth())
                        toolTip.clear();    // prevent any tooltip showing
                }
            }
            QToolTip::showText(he->globalPos(), toolTip, this);
            return true;
        }
    }
    return QTreeView::viewportEvent(e);
}

/******************************************************************************
* Called when a context menu event is requested by mouse or key.
*/
void EventListView::contextMenuEvent(QContextMenuEvent* e)
{
    Q_EMIT contextMenuRequested(e->globalPos());
}

QStyleOptionViewItem EventListView::listViewOptions() const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    return option;
}


bool EventListDelegate::editorEvent(QEvent* e, QAbstractItemModel* model, const QStyleOptionViewItem&, const QModelIndex& index)
{
    // Don't invoke the editor unless it's either a double click or,
    // if KDE is in single click mode and it's a left button release
    // with no other buttons pressed and no keyboard modifiers.
    switch (e->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
            return false;
        case QEvent::MouseButtonDblClick:
            break;
        case QEvent::MouseButtonRelease:
        {
            auto view = static_cast<EventListView*>(parent());
            if (!view->editOnSingleClick()
            ||  !view->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, nullptr, view))
                return false;
            auto me = static_cast<QMouseEvent*>(e);
            if (me->button() != Qt::LeftButton  ||  me->buttons()
            ||  me->modifiers() != Qt::NoModifier)
                return false;
            break;
        }
        default:
            break;
    }
    if (index.isValid())
    {
        qCDebug(KALARM_LOG) << "EventListDelegate::editorEvent";
        auto itemModel = qobject_cast<EventListModel*>(model);
        if (!itemModel)
            qCCritical(KALARM_LOG) << "EventListDelegate::editorEvent: Invalid cast to EventListModel*";
        else
        {
            KAEvent event = itemModel->event(index);
            edit(event, static_cast<EventListView*>(parent()));
            return true;
        }
    }
    return false;   // indicate that the event has not been handled
}

// vim: et sw=4:
