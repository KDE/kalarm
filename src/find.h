/*
 *  find.h  -  search facility
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QModelIndex>

class QCheckBox;
class KFindDialog;
class KFind;
class KSeparator;
class EventListView;


class Find : public QObject
{
    Q_OBJECT
public:
    explicit Find(EventListView* parent);
    ~Find() override;
    void        display();
    void        findNext(bool forward)     { findNext(forward, false, false); }

Q_SIGNALS:
    void        active(bool);

private Q_SLOTS:
    void        slotFind();
    void        slotKFindDestroyed()       { Q_EMIT active(false); }
    void        slotSelectionChanged();

private:
    void        findNext(bool forward, bool checkEnd, bool fromCurrent);
    QModelIndex nextItem(const QModelIndex&, bool forward) const;

    EventListView*     mListView;        // parent list view
    QPointer<KFindDialog>  mDialog;
    QCheckBox*         mArchived;
    QCheckBox*         mLive;
    KSeparator*        mActiveArchivedSep;
    QCheckBox*         mMessageType;
    QCheckBox*         mFileType;
    QCheckBox*         mCommandType;
    QCheckBox*         mEmailType;
    QCheckBox*         mAudioType;
    KFind*             mFind {nullptr};
    QStringList        mHistory;         // list of history items for Find dialog
    QString            mLastPattern;     // pattern used in last search
    QString            mStartID;         // ID of first alarm searched if 'from cursor' was selected
    long               mOptions {0};     // OR of find dialog options
    bool               mNoCurrentItem;   // there is no current item for the purposes of searching
    bool               mFound {false};   // true if any matches have been found
};

// vim: et sw=4:
