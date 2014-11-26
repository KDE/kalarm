/*
 *  find.cpp  -  search facility
 *  Program:  kalarm
 *  Copyright © 2005-2013 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "find.h"

#include "alarmlistview.h"
#include "eventlistview.h"
#include "messagebox.h"
#include "preferences.h"
#include "config-kdepim.h"

#include <kalarmcal/kaevent.h>

#include <kfinddialog.h>
#include <kfind.h>
#include <kseparator.h>
#include <kwindowsystem.h>
#include <KLocalizedString>
#include <KDialog>

#include <QGroupBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QRegExp>
#include <qdebug.h>

using namespace KAlarmCal;

// KAlarm-specific options for Find dialog
enum {
    FIND_LIVE     = KFind::MinimumUserOption,
    FIND_ARCHIVED = KFind::MinimumUserOption << 1,
    FIND_MESSAGE  = KFind::MinimumUserOption << 2,
    FIND_FILE     = KFind::MinimumUserOption << 3,
    FIND_COMMAND  = KFind::MinimumUserOption << 4,
    FIND_EMAIL    = KFind::MinimumUserOption << 5,
    FIND_AUDIO    = KFind::MinimumUserOption << 6
};
static long FIND_KALARM_OPTIONS = FIND_LIVE | FIND_ARCHIVED | FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL | FIND_AUDIO;


Find::Find(EventListView* parent)
    : QObject(parent),
      mListView(parent),
      mDialog(0),
      mFind(0),
      mOptions(0),
      mFound(false)
{
    connect(mListView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(slotSelectionChanged()));
}

Find::~Find()
{
    delete mDialog;    // automatically set to 0
    delete mFind;
    mFind = 0;
}

void Find::slotSelectionChanged()
{
    if (mDialog)
        mDialog->setHasCursor(mListView->selectionModel()->currentIndex().isValid());
}

/******************************************************************************
* Display the Find dialog.
*/
void Find::display()
{
    if (!mOptions)
        // Set defaults the first time the Find dialog is activated
        mOptions = FIND_LIVE | FIND_ARCHIVED | FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL | FIND_AUDIO;
    bool noArchived = !Preferences::archivedKeepDays();
    bool showArchived = qobject_cast<AlarmListView*>(mListView)
                        && (static_cast<AlarmListModel*>(mListView->model())->eventTypeFilter() & CalEvent::ARCHIVED);
    if (noArchived  ||  !showArchived)      // these settings could change between activations
        mOptions &= ~FIND_ARCHIVED;

    if (mDialog)
    {
#if KDEPIM_HAVE_X11
        KWindowSystem::activateWindow(mDialog->winId());
#endif
    }
    else
    {
        mDialog = new KFindDialog(mListView, mOptions, mHistory, (mListView->selectionModel()->selectedRows().count() > 1));
        mDialog->setModal(false);
        mDialog->setObjectName(QLatin1String("FindDlg"));
        mDialog->setHasSelection(false);
        QWidget* kalarmWidgets = mDialog->findExtension();

        // Alarm types
        QVBoxLayout* layout = new QVBoxLayout(kalarmWidgets);
        layout->setMargin(0);
        layout->setSpacing(KDialog::spacingHint());
        QGroupBox* group = new QGroupBox(i18nc("@title:group", "Alarm Type"), kalarmWidgets);
        layout->addWidget(group);
        QGridLayout* grid = new QGridLayout(group);
        grid->setMargin(KDialog::marginHint());
        grid->setSpacing(KDialog::spacingHint());
        grid->setColumnStretch(1, 1);

        // Live & archived alarm selection
        mLive = new QCheckBox(i18nc("@option:check Alarm type", "Active"), group);
        mLive->setFixedSize(mLive->sizeHint());
        mLive->setWhatsThis(i18nc("@info:whatsthis", "Check to include active alarms in the search."));
        grid->addWidget(mLive, 1, 0, Qt::AlignLeft);

        mArchived = new QCheckBox(i18nc("@option:check Alarm type", "Archived"), group);
        mArchived->setFixedSize(mArchived->sizeHint());
        mArchived->setWhatsThis(i18nc("@info:whatsthis", "Check to include archived alarms in the search. "
                                     "This option is only available if archived alarms are currently being displayed."));
        grid->addWidget(mArchived, 1, 2, Qt::AlignLeft);

        mActiveArchivedSep = new KSeparator(Qt::Horizontal, kalarmWidgets);
        grid->addWidget(mActiveArchivedSep, 2, 0, 1, 3);

        // Alarm actions
        mMessageType = new QCheckBox(i18nc("@option:check Alarm action = text display", "Text"), group);
        mMessageType->setFixedSize(mMessageType->sizeHint());
        mMessageType->setWhatsThis(i18nc("@info:whatsthis", "Check to include text message alarms in the search."));
        grid->addWidget(mMessageType, 3, 0);

        mFileType = new QCheckBox(i18nc("@option:check Alarm action = file display", "File"), group);
        mFileType->setFixedSize(mFileType->sizeHint());
        mFileType->setWhatsThis(i18nc("@info:whatsthis", "Check to include file alarms in the search."));
        grid->addWidget(mFileType, 3, 2);

        mCommandType = new QCheckBox(i18nc("@option:check Alarm action", "Command"), group);
        mCommandType->setFixedSize(mCommandType->sizeHint());
        mCommandType->setWhatsThis(i18nc("@info:whatsthis", "Check to include command alarms in the search."));
        grid->addWidget(mCommandType, 4, 0);

        mEmailType = new QCheckBox(i18nc("@option:check Alarm action", "Email"), group);
        mEmailType->setFixedSize(mEmailType->sizeHint());
        mEmailType->setWhatsThis(i18nc("@info:whatsthis", "Check to include email alarms in the search."));
        grid->addWidget(mEmailType, 4, 2);

        mAudioType = new QCheckBox(i18nc("@option:check Alarm action", "Audio"), group);
        mAudioType->setFixedSize(mAudioType->sizeHint());
        mAudioType->setWhatsThis(i18nc("@info:whatsthis", "Check to include audio alarms in the search."));
        grid->addWidget(mAudioType, 5, 0);

        // Set defaults
        mLive->setChecked(mOptions & FIND_LIVE);
        mArchived->setChecked(mOptions & FIND_ARCHIVED);
        mMessageType->setChecked(mOptions & FIND_MESSAGE);
        mFileType->setChecked(mOptions & FIND_FILE);
        mCommandType->setChecked(mOptions & FIND_COMMAND);
        mEmailType->setChecked(mOptions & FIND_EMAIL);
        mAudioType->setChecked(mOptions & FIND_AUDIO);

        connect(mDialog.data(), &KFindDialog::okClicked, this, &Find::slotFind);
    }

    // Only display active/archived options if archived alarms are being kept
    if (noArchived)
    {
        mLive->hide();
        mArchived->hide();
        mActiveArchivedSep->hide();
    }
    else
    {
        mLive->show();
        mArchived->show();
        mActiveArchivedSep->show();
    }

    // Disable options where no displayed alarms match them
    bool live     = false;
    bool archived = false;
    bool text     = false;
    bool file     = false;
    bool command  = false;
    bool email    = false;
    bool audio    = false;
    int rowCount = mListView->model()->rowCount();
    for (int row = 0;  row < rowCount;  ++row)
    {
        KAEvent viewEvent = mListView->event(row);
        const KAEvent* event = &viewEvent;
        if (event->expired())
            archived = true;
        else
            live = true;
        switch (event->actionTypes())
        {
            case KAEvent::ACT_EMAIL:    email   = true;  break;
            case KAEvent::ACT_AUDIO:    audio   = true;  break;
            case KAEvent::ACT_COMMAND:  command = true;  break;
            case KAEvent::ACT_DISPLAY:
                if (event->actionSubType() == KAEvent::FILE)
                {
                    file = true;
                    break;
                }
                // fall through to ACT_DISPLAY_COMMAND
            case KAEvent::ACT_DISPLAY_COMMAND:
            default:
                text = true;
                break;
        }
    }
    mLive->setEnabled(live);
    mArchived->setEnabled(archived);
    mMessageType->setEnabled(text);
    mFileType->setEnabled(file);
    mCommandType->setEnabled(command);
    mEmailType->setEnabled(email);
    mAudioType->setEnabled(audio);

    mDialog->setHasCursor(mListView->selectionModel()->currentIndex().isValid());
    mDialog->show();
}

/******************************************************************************
* Called when the user requests a search by clicking the dialog OK button.
*/
void Find::slotFind()
{
    if (!mDialog)
        return;
    mHistory = mDialog->findHistory();    // save search history so that it can be displayed again
    mOptions = mDialog->options() & ~FIND_KALARM_OPTIONS;
    if ((mOptions & KFind::RegularExpression)  &&  !QRegExp(mDialog->pattern()).isValid())
        return;
    mOptions |= (mLive->isEnabled()        && mLive->isChecked()        ? FIND_LIVE : 0)
             |  (mArchived->isEnabled()    && mArchived->isChecked()    ? FIND_ARCHIVED : 0)
             |  (mMessageType->isEnabled() && mMessageType->isChecked() ? FIND_MESSAGE : 0)
             |  (mFileType->isEnabled()    && mFileType->isChecked()    ? FIND_FILE : 0)
             |  (mCommandType->isEnabled() && mCommandType->isChecked() ? FIND_COMMAND : 0)
             |  (mEmailType->isEnabled()   && mEmailType->isChecked()   ? FIND_EMAIL : 0)
             |  (mAudioType->isEnabled()   && mAudioType->isChecked()   ? FIND_AUDIO : 0);
    if (!(mOptions & (FIND_LIVE | FIND_ARCHIVED))
    ||  !(mOptions & (FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL | FIND_AUDIO)))
    {
        KAMessageBox::sorry(mDialog, i18nc("@info", "No alarm types are selected to search"));
        return;
    }

    // Supply KFind with only those options which relate to the text within alarms
    long options = mOptions & (KFind::WholeWordsOnly | KFind::CaseSensitive | KFind::RegularExpression);
    bool newFind = !mFind;
    bool newPattern = (mDialog->pattern() != mLastPattern);
    mLastPattern = mDialog->pattern();
    if (mFind)
    {
        mFind->resetCounts();
        mFind->setPattern(mLastPattern);
        mFind->setOptions(options);
    }
    else
    {
        mFind = new KFind(mLastPattern, options, mListView, mDialog);
        connect(mFind, &KFind::destroyed, this, &Find::slotKFindDestroyed);
        mFind->closeFindNextDialog();    // prevent 'Find Next' dialog appearing
    }

    // Set the starting point for the search
    mStartID.clear();
    mNoCurrentItem = newPattern;
    bool checkEnd = false;
    if (newPattern)
    {
        mFound = false;
        if (mOptions & KFind::FromCursor)
        {
            QModelIndex index = mListView->selectionModel()->currentIndex();
            if (index.isValid())
            {
                mStartID       = mListView->event(index).id();
                mNoCurrentItem = false;
                checkEnd = true;
            }
        }
    }

    // Execute the search
    findNext(true, checkEnd, false);
    if (mFind  &&  newFind)
        emit active(true);
}

/******************************************************************************
* Perform the search.
* If 'fromCurrent' is true, the search starts with the current search item;
* otherwise, it starts from the next item.
*/
void Find::findNext(bool forward, bool checkEnd, bool fromCurrent)
{
    QModelIndex index;
    if (!mNoCurrentItem)
        index = mListView->selectionModel()->currentIndex();
    if (!fromCurrent)
        index = nextItem(index, forward);

    // Search successive alarms until a match is found or the end is reached
    bool found = false;
    bool last = false;
    for ( ;  index.isValid() && !last;  index = nextItem(index, forward))
    {
        KAEvent viewEvent = mListView->event(index);
        const KAEvent* event = &viewEvent;
        if (!fromCurrent  &&  !mStartID.isNull()  &&  mStartID == event->id())
            last = true;    // we've wrapped round and reached the starting alarm again
        fromCurrent = false;
        bool live = !event->expired();
        if ((live  &&  !(mOptions & FIND_LIVE))
        ||  (!live  &&  !(mOptions & FIND_ARCHIVED)))
            continue;     // we're not searching this type of alarm
        switch (event->actionTypes())
        {
            case KAEvent::ACT_EMAIL:
                if (!(mOptions & FIND_EMAIL))
                    break;
                mFind->setData(event->emailAddresses(QLatin1String(", ")));
                found = (mFind->find() == KFind::Match);
                if (found)
                    break;
                mFind->setData(event->emailSubject());
                found = (mFind->find() == KFind::Match);
                if (found)
                    break;
                mFind->setData(event->emailAttachments().join(QLatin1String(", ")));
                found = (mFind->find() == KFind::Match);
                if (found)
                    break;
                mFind->setData(event->cleanText());
                found = (mFind->find() == KFind::Match);
                break;

            case KAEvent::ACT_AUDIO:
                if (!(mOptions & FIND_AUDIO))
                    break;
                mFind->setData(event->audioFile());
                found = (mFind->find() == KFind::Match);
                break;

            case KAEvent::ACT_COMMAND:
                if (!(mOptions & FIND_COMMAND))
                    break;
                mFind->setData(event->cleanText());
                found = (mFind->find() == KFind::Match);
                break;

            case KAEvent::ACT_DISPLAY:
                if (event->actionSubType() == KAEvent::FILE)
                {
                    if (!(mOptions & FIND_FILE))
                        break;
                    mFind->setData(event->cleanText());
                    found = (mFind->find() == KFind::Match);
                    break;
                }
                // fall through to ACT_DISPLAY_COMMAND
            case KAEvent::ACT_DISPLAY_COMMAND:
                if (!(mOptions & FIND_MESSAGE))
                    break;
                mFind->setData(event->cleanText());
                found = (mFind->find() == KFind::Match);
                break;
            default:
                break;
        }
        if (found)
            break;
    }

    // Process the search result
    mNoCurrentItem = !index.isValid();
    if (found)
    {
        // A matching alarm was found - highlight it and make it current
        mFound = true;
        QItemSelectionModel* sel = mListView->selectionModel();
        sel->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        sel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        mListView->scrollTo(index);
    }
    else
    {
        // No match was found
        if (mFound  ||  checkEnd)
        {
            QString msg = forward ? xi18nc("@info", "<para>End of alarm list reached.</para><para>Continue from the beginning?</para>")
                                  : xi18nc("@info", "<para>Beginning of alarm list reached.</para><para>Continue from the end?</para>");
            if (KAMessageBox::questionYesNo(mListView, msg, QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()) == KMessageBox::Yes)
            {
                mNoCurrentItem = true;
                findNext(forward, false, false);
                return;
            }
        }
        else
            mFind->displayFinalDialog();     // display "no match was found"
        mNoCurrentItem = false;    // restart from the currently highlighted alarm if Find Next etc selected
    }
}

/******************************************************************************
* Get the next alarm item to search.
*/
QModelIndex Find::nextItem(const QModelIndex& index, bool forward) const
{
    if (mOptions & KFind::FindBackwards)
        forward = !forward;
    if (!index.isValid())
    {
        QAbstractItemModel* model = mListView->model();
        if (forward)
            return model->index(0, 0);
        else
            return model->index(model->rowCount() - 1, 0);
    }
    if (forward)
        return mListView->indexBelow(index);
    else
        return mListView->indexAbove(index);
}
// vim: et sw=4:
