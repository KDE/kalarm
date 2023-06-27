/*
 *  alarmlistview.cpp  -  widget showing list of alarms
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "alarmlistview.h"

#include "resources/resourcedatamodelbase.h"
#include "resources/eventmodel.h"

#include <KSharedConfig>
#include <KConfigGroup>

#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>

/*=============================================================================
* Item delegate to draw an icon centered in the column.
=============================================================================*/
class CentreDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    CentreDelegate(QWidget* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


AlarmListView::AlarmListView(const QByteArray& configGroup, QWidget* parent)
    : EventListView(parent)
    , mConfigGroup(configGroup)
{
    setEditOnSingleClick(true);
    setItemDelegateForColumn(AlarmListModel::TypeColumn, new CentreDelegate(this));
    connect(header(), &QHeaderView::sectionMoved, this, &AlarmListView::saveColumnsState);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &AlarmListView::headerContextMenuRequested);
    Preferences::connect(&Preferences::useAlarmNameChanged, this, &AlarmListView::useAlarmNameChanged);
}

/******************************************************************************
* Return which of the optional columns are currently shown.
* Note that the column order must be the same as in setColumnsVisible().
* Reply = array of 5 columns if not using alarm names;
*       = array of 6 columns if not using alarm names.
*/
QList<bool> AlarmListView::columnsVisible() const
{
    if (!model())
        return {};
    QList<bool> vis{ !header()->isSectionHidden(AlarmListModel::TimeColumn),
                     !header()->isSectionHidden(AlarmListModel::TimeToColumn),
                     !header()->isSectionHidden(AlarmListModel::RepeatColumn),
                     !header()->isSectionHidden(AlarmListModel::ColourColumn),
                     !header()->isSectionHidden(AlarmListModel::TypeColumn) };
    if (Preferences::useAlarmName())
        vis += !header()->isSectionHidden(AlarmListModel::TextColumn);
    return vis;
}

/******************************************************************************
* Set which of the optional columns are to be shown.
* 'show' = array of 5 columns if not using alarm names;
*        = array of 6 columns if not using alarm names.
* Note that the column order must be the same as in columnsVisible().
*/
void AlarmListView::setColumnsVisible(const QList<bool>& show)
{
    if (!model())
        return;
    const bool useName = Preferences::useAlarmName();
    QList<bool> vis{ true, false, true, true, true, !useName };
    const int colCount = useName ? 6 : 5;
    for (int i = 0, count = std::min<int>(colCount, show.count());  i < count;  ++i)
        vis[i] = show[i];
    header()->setSectionHidden(AlarmListModel::TimeColumn,   !vis[0]);
    header()->setSectionHidden(AlarmListModel::TimeToColumn, !vis[1]);
    header()->setSectionHidden(AlarmListModel::RepeatColumn, !vis[2]);
    header()->setSectionHidden(AlarmListModel::ColourColumn, !vis[3]);
    header()->setSectionHidden(AlarmListModel::TypeColumn,   !vis[4]);
    header()->setSectionHidden(AlarmListModel::NameColumn,   !useName);
    header()->setSectionHidden(AlarmListModel::TextColumn,   !vis[5]);
    setReplaceBlankName();
    setSortingEnabled(false);  // sortByColumn() won't work if sorting is already enabled!
    sortByColumn(vis[0] ? AlarmListModel::TimeColumn : AlarmListModel::TimeToColumn, Qt::AscendingOrder);
}

/******************************************************************************
* Initialize column settings and sizing.
*/
void AlarmListView::initSections()
{
    KConfigGroup config(KSharedConfig::openConfig(), mConfigGroup.constData());
    const QByteArray settings = config.readEntry("ListHead", QByteArray());
    if (!settings.isEmpty())
    {
        header()->restoreState(settings);
        const bool useName = Preferences::useAlarmName();
        header()->setSectionHidden(AlarmListModel::NameColumn, !useName);
        if (!useName)
        {
            header()->setSectionHidden(AlarmListModel::TextColumn, false);
            setReplaceBlankName();
        }
    }
    header()->setSectionsMovable(true);
    header()->setSectionResizeMode(AlarmListModel::TimeColumn, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(AlarmListModel::TimeToColumn, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(AlarmListModel::RepeatColumn, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(AlarmListModel::ColourColumn, QHeaderView::Fixed);
    header()->setSectionResizeMode(AlarmListModel::TypeColumn, QHeaderView::Fixed);
    header()->setSectionResizeMode(AlarmListModel::NameColumn, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(AlarmListModel::TextColumn, QHeaderView::Stretch);
    header()->setStretchLastSection(true);   // necessary to ensure ResizeToContents columns do resize to contents!
    const int minWidth = listViewOptions().fontMetrics.lineSpacing() * 3 / 4;
    header()->setMinimumSectionSize(minWidth);
    const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
    header()->resizeSection(AlarmListModel::ColourColumn, minWidth);
    header()->resizeSection(AlarmListModel::TypeColumn, AlarmListModel::iconWidth() + 2*margin + 2);
}

QStyleOptionViewItem AlarmListView::listViewOptions() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QTreeView::viewOptions();
#else
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    return option;
#endif
}

/******************************************************************************
* Called when the column order is changed.
* Save the new order for restoration on program restart.
*/
void AlarmListView::saveColumnsState()
{
    KConfigGroup config(KSharedConfig::openConfig(), mConfigGroup.constData());
    config.writeEntry("ListHead", header()->saveState());
    config.sync();
}

/******************************************************************************
* Called when a context menu is requested for the header.
* Allow the user to choose which columns to display.
*/
void AlarmListView::headerContextMenuRequested(const QPoint& pt)
{
    QAbstractItemModel* almodel = model();
    QMenu menu;
    const bool useName = Preferences::useAlarmName();
    for (int col = 0, count = header()->count();  col < count;  ++col)
    {
        const QString title = almodel->headerData(col, Qt::Horizontal, ResourceDataModelBase::ColumnTitleRole).toString();
        if (!title.isEmpty())
        {
            QAction* act = menu.addAction(title);
            act->setData(col);
            act->setCheckable(true);
            act->setChecked(!header()->isSectionHidden(col));
            // Always show Name column, but disabled.
            // If the alarm name feature is not enabled, this serves as a small
            // hint to the user that the name feature exists.
            if (col == AlarmListModel::NameColumn)
                act->setEnabled(false);    // don't allow name column to be hidden if name is used
            else if (col == AlarmListModel::TextColumn  &&  !useName)
                act->setEnabled(false);    // don't allow text column to be hidden if name not used
            else
                QObject::connect(act, &QAction::triggered,
                                 this, [this, &menu, act] { showHideColumn(menu, act); });   //clazy:exclude=lambda-in-connect
        }
    }
    enableTimeColumns(&menu);
    menu.exec(header()->mapToGlobal(pt));
}

/******************************************************************************
* Called when the 'use alarm name' setting has changed.
*/
void AlarmListView::useAlarmNameChanged(bool use)
{
    header()->setSectionHidden(AlarmListModel::NameColumn, !use);
    header()->setSectionHidden(AlarmListModel::TextColumn, use);
    setReplaceBlankName();
}

/******************************************************************************
* Show or hide a column according to the header context menu.
*/
void AlarmListView::showHideColumn(QMenu& menu, QAction* act)
{
    int col = act->data().toInt();
    if (col < 0  ||  col >= header()->count())
        return;
    bool show = act->isChecked();
    header()->setSectionHidden(col, !show);
    if (col == AlarmListModel::TimeColumn  ||  col == AlarmListModel::TimeToColumn)
        enableTimeColumns(&menu);
    if (col == AlarmListModel::TextColumn)
        setReplaceBlankName();
    saveColumnsState();
    Q_EMIT columnsVisibleChanged();
}

/******************************************************************************
* Set whether to replace a blank alarm name with the alarm text.
*/
void AlarmListView::setReplaceBlankName()
{
    bool textHidden = header()->isSectionHidden(AlarmListModel::TextColumn);
    auto almodel = qobject_cast<AlarmListModel*>(model());
    if (almodel)
        almodel->setReplaceBlankName(textHidden);
}

/******************************************************************************
* Disable Time or Time To in the context menu if the other one is not
* selected to be displayed, to ensure that at least one is always shown.
*/
void AlarmListView::enableTimeColumns(QMenu* menu)
{
    bool timeShown   = !header()->isSectionHidden(AlarmListModel::TimeColumn);
    bool timeToShown = !header()->isSectionHidden(AlarmListModel::TimeToColumn);
    const QList<QAction*> actions = menu->actions();
    if (!timeToShown)
    {
        header()->setSectionHidden(AlarmListModel::TimeColumn, false);
        for (QAction* act : actions)
        {
            if (act->data().toInt() == AlarmListModel::TimeColumn)
            {
                act->setEnabled(false);
                break;
            }
        }
    }
    else if (!timeShown)
    {
        header()->setSectionHidden(AlarmListModel::TimeToColumn, false);
        for (QAction* act : actions)
        {
            if (act->data().toInt() == AlarmListModel::TimeToColumn)
            {
                act->setEnabled(false);
                break;
            }
        }
    }
}


/******************************************************************************
* Draw the alarm's icon centered in the column.
*/
void CentreDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Draw the item background without any icon
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.icon = QIcon();
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, nullptr);

    // Draw the icon centered within item
    const QPixmap icon = qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));
    const QRect r = option.rect;
    const QPoint p = QPoint((r.width() - icon.width())/2, (r.height() - icon.height())/2);
    painter->drawPixmap(r.topLeft() + p, icon);
}

#include "alarmlistview.moc"

// vim: et sw=4:
