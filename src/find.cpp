/*
 *  find.cpp  -  search facility
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "find.h"

#include "alarmlistview.h"
#include "eventlistview.h"
#include "preferences.h"
#include "resources/eventmodel.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/kaevent.h"
#include "config-kalarm.h"

#include <KFindDialog>
#include <KFind>
#include <KSeparator>
#include <KLocalizedString>
#if ENABLE_X11
#include <KX11Extras>
#endif

#include <QGroupBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QRegularExpression>

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
    : QObject(parent)
    , mListView(parent)
    , mDialog(nullptr)
{
    connect(mListView->selectionModel(), &QItemSelectionModel::currentChanged, this, &Find::slotSelectionChanged);
}

Find::~Find()
{
    delete mDialog;    // automatically set to null
    delete mFind;
    mFind = nullptr;
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
    {
        // Set defaults the first time the Find dialog is activated
        mOptions = FIND_LIVE | FIND_ARCHIVED | FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL | FIND_AUDIO;
    }
    bool noArchived = !Preferences::archivedKeepDays();
    bool showArchived = qobject_cast<AlarmListView*>(mListView)
                        && (static_cast<AlarmListModel*>(mListView->model())->eventTypeFilter() & CalEvent::ARCHIVED);
    if (noArchived  ||  !showArchived)      // these settings could change between activations
        mOptions &= ~FIND_ARCHIVED;

    if (mDialog)
    {
#if ENABLE_X11
        KX11Extras::activateWindow(mDialog->winId());
#endif
    }
    else
    {
        mDialog = new KFindDialog(mListView, mOptions, mHistory, (mListView->selectionModel()->selectedRows().count() > 1));
        mDialog->setModal(false);
        mDialog->setObjectName(QStringLiteral("FindDlg"));
        mDialog->setHasSelection(false);
        QWidget* kalarmWidgets = mDialog->findExtension();

        // Alarm types
        auto layout = new QVBoxLayout(kalarmWidgets);
        layout->setContentsMargins(0, 0, 0, 0);
        QGroupBox* group = new QGroupBox(i18nc("@title:group", "Alarm Type"), kalarmWidgets);
        layout->addWidget(group);
        auto grid = new QGridLayout(group);
        grid->setColumnStretch(1, 1);

        // Live & archived alarm selection
        mLive = new QCheckBox(i18nc("@option:check Alarm type", "Active"), group);
        mLive->setWhatsThis(i18nc("@info:whatsthis", "Check to include active alarms in the search."));
        grid->addWidget(mLive, 1, 0, Qt::AlignLeft);

        mArchived = new QCheckBox(i18nc("@option:check Alarm type", "Archived"), group);
        mArchived->setWhatsThis(i18nc("@info:whatsthis", "Check to include archived alarms in the search. "
                                     "This option is only available if archived alarms are currently being displayed."));
        grid->addWidget(mArchived, 1, 2, Qt::AlignLeft);

        mActiveArchivedSep = new KSeparator(Qt::Horizontal, kalarmWidgets);
        grid->addWidget(mActiveArchivedSep, 2, 0, 1, 3);

        // Alarm actions
        mMessageType = new QCheckBox(i18nc("@option:check Alarm action = text display", "Text"), group);
        mMessageType->setWhatsThis(i18nc("@info:whatsthis", "Check to include text message alarms in the search."));
        grid->addWidget(mMessageType, 3, 0);

        mFileType = new QCheckBox(i18nc("@option:check Alarm action = file display", "File"), group);
        mFileType->setWhatsThis(i18nc("@info:whatsthis", "Check to include file alarms in the search."));
        grid->addWidget(mFileType, 3, 2);

        mCommandType = new QCheckBox(i18nc("@option:check Alarm action", "Command"), group);
        mCommandType->setWhatsThis(i18nc("@info:whatsthis", "Check to include command alarms in the search."));
        grid->addWidget(mCommandType, 4, 0);

        mEmailType = new QCheckBox(i18nc("@option:check Alarm action", "Email"), group);
        mEmailType->setWhatsThis(i18nc("@info:whatsthis", "Check to include email alarms in the search."));
        grid->addWidget(mEmailType, 4, 2);

        mAudioType = new QCheckBox(i18nc("@option:check Alarm action", "Audio"), group);
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
    const int rowCount = mListView->model()->rowCount();
    for (int row = 0;  row < rowCount;  ++row)
    {
        const KAEvent viewEvent = mListView->event(row);
        const KAEvent* event = &viewEvent;
        if (event->expired())
            archived = true;
        else
            live = true;
        switch (event->actionTypes())
        {
            case KAEvent::Action::Email:    email   = true;  break;
            case KAEvent::Action::Audio:    audio   = true;  break;
            case KAEvent::Action::Command:  command = true;  break;
            case KAEvent::Action::Display:
                if (event->actionSubType() == KAEvent::SubAction::File)
                {
                    file = true;
                    break;
                }
                // fall through to DisplayCommand
                Q_FALLTHROUGH();
            case KAEvent::Action::DisplayCommand:
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
    if ((mOptions & KFind::RegularExpression)  &&  !QRegularExpression(mDialog->pattern()).isValid())
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
        KAMessageBox::error(mDialog, i18nc("@info", "No alarm types are selected to search"));
        return;
    }

    // Supply KFind with only those options which relate to the text within alarms
    const long options = mOptions & (KFind::WholeWordsOnly | KFind::CaseSensitive | KFind::RegularExpression);
    const bool newFind = !mFind;
    const bool newPattern = (mDialog->pattern() != mLastPattern);
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
            const QModelIndex index = mListView->selectionModel()->currentIndex();
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
        Q_EMIT active(true);
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
        const KAEvent viewEvent = mListView->event(index);
        const KAEvent* event = &viewEvent;
        if (!fromCurrent  &&  !mStartID.isNull()  &&  mStartID == event->id())
            last = true;    // we've wrapped round and reached the starting alarm again
        fromCurrent = false;
        const bool live = !event->expired();
        if ((live  &&  !(mOptions & FIND_LIVE))
        ||  (!live  &&  !(mOptions & FIND_ARCHIVED)))
            continue;     // we're not searching this type of alarm
        switch (event->actionTypes())
        {
            case KAEvent::Action::Email:
                if (!(mOptions & FIND_EMAIL))
                    break;
                mFind->setData(event->emailAddresses(QStringLiteral(", ")));
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

            case KAEvent::Action::Audio:
                if (!(mOptions & FIND_AUDIO))
                    break;
                mFind->setData(event->audioFile());
                found = (mFind->find() == KFind::Match);
                break;

            case KAEvent::Action::Command:
                if (!(mOptions & FIND_COMMAND))
                    break;
                mFind->setData(event->cleanText());
                found = (mFind->find() == KFind::Match);
                break;

            case KAEvent::Action::Display:
                if (event->actionSubType() == KAEvent::SubAction::File)
                {
                    if (!(mOptions & FIND_FILE))
                        break;
                    mFind->setData(event->cleanText());
                    found = (mFind->find() == KFind::Match);
                    break;
                }
                // fall through to DisplayCommand
                Q_FALLTHROUGH();
            case KAEvent::Action::DisplayCommand:
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
            const QString msg = forward ? xi18nc("@info", "<para>End of alarm list reached.</para><para>Continue from the beginning?</para>")
                                        : xi18nc("@info", "<para>Beginning of alarm list reached.</para><para>Continue from the end?</para>");
            if (KAMessageBox::questionYesNo(mListView, msg, QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()) == KMessageBox::ButtonCode::PrimaryAction)
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

#include "moc_find.cpp"

// vim: et sw=4:
