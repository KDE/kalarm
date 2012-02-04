/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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
#include "messagewin_p.moc"
#include "messagewin.moc"

#include "alarmcalendar.h"
#include "autoqpointer.h"
#ifdef USE_AKONADI
#include "collectionmodel.h"
#endif
#include "deferdlg.h"
#include "desktop.h"
#include "editdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"
#include "pushbutton.h"
#include "shellprocess.h"
#include "synchtimer.h"

#include "kspeechinterface.h"

#include <kstandarddirs.h>
#include <kaction.h>
#include <kstandardguiitem.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <ktextbrowser.h>
#include <ksystemtimezone.h>
#include <kglobalsettings.h>
#include <kmimetype.h>
#include <ktextedit.h>
#include <kwindowsystem.h>
#include <kio/netaccess.h>
#include <knotification.h>
#include <kpushbutton.h>
#include <ksqueezedtextlabel.h>
#include <phonon/mediaobject.h>
#include <phonon/audiooutput.h>
#include <phonon/volumefadereffect.h>
#include <kdebug.h>
#include <ktoolinvocation.h>
#ifdef Q_WS_X11
#include <netwm.h>
#include <QX11Info>
#endif

#include <QScrollBar>
#include <QtDBus/QtDBus>
#include <QFile>
#include <QFileInfo>
#include <QCheckBox>
#include <QLabel>
#include <QPalette>
#include <QTimer>
#include <QPixmap>
#include <QByteArray>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QMutexLocker>

#include <stdlib.h>
#include <string.h>

#ifdef USE_AKONADI
using namespace KCalCore;
#else
using namespace KCal;
#endif
using namespace KAlarmCal;

#ifdef Q_WS_X11
enum FullScreenType { NoFullScreen = 0, FullScreen = 1, FullScreenActive = 2 };
static FullScreenType haveFullScreenWindow(int screen);
static FullScreenType findFullScreenWindows(const QVector<QRect>& screenRects, QVector<FullScreenType>& screenTypes);
#endif

#ifdef KMAIL_SUPPORTED
#include "kmailinterface.h"
static const char* KMAIL_DBUS_SERVICE   = "org.kde.kmail";
static const char* KMAIL_DBUS_PATH      = "/KMail";
#endif

// The delay for enabling message window buttons if a zero delay is
// configured, i.e. the windows are placed far from the cursor.
static const int proximityButtonDelay = 1000;    // (milliseconds)
static const int proximityMultiple = 10;         // multiple of button height distance from cursor for proximity

// A text label widget which can be scrolled and copied with the mouse
class MessageText : public KTextEdit
{
    public:
        MessageText(QWidget* parent = 0)
            : KTextEdit(parent),
              mNewLine(false)
        {
            setReadOnly(true);
            setFrameStyle(NoFrame);
            setLineWrapMode(NoWrap);
        }
        int scrollBarHeight() const     { return horizontalScrollBar()->height(); }
        int scrollBarWidth() const      { return verticalScrollBar()->width(); }
        void setBackgroundColour(const QColor& c)
        {
            QPalette pal = viewport()->palette();
            pal.setColor(viewport()->backgroundRole(), c);
            viewport()->setPalette(pal);
        }
        virtual QSize sizeHint() const
        {
            QSizeF docsize = document()->size();
            return QSize(static_cast<int>(docsize.width() + 0.99) + verticalScrollBar()->width(),
                         static_cast<int>(docsize.height() + 0.99) + horizontalScrollBar()->height());
        }
        bool newLine() const       { return mNewLine; }
        void setNewLine(bool nl)   { mNewLine = nl; }
    private:
        bool mNewLine;
};


// Basic flags for the window
static const Qt::WFlags          WFLAGS       = Qt::WindowStaysOnTopHint;
static const Qt::WFlags          WFLAGS2      = Qt::WindowContextHelpButtonHint;
static const Qt::WidgetAttribute WidgetFlags  = Qt::WA_DeleteOnClose;

// Error message bit masks
enum {
    ErrMsg_Speak     = 0x01,
    ErrMsg_AudioFile = 0x02
};


QList<MessageWin*> MessageWin::mWindowList;
QMap<QString, unsigned> MessageWin::mErrorMessages;
// There can only be one audio thread at a time: trying to play multiple
// sound files simultaneously would result in a cacophony, and besides
// that, Phonon currently crashes...
QPointer<AudioThread> MessageWin::mAudioThread;
MessageWin*           MessageWin::mAudioOwner = 0;

/******************************************************************************
* Construct the message window for the specified alarm.
* Other alarms in the supplied event may have been updated by the caller, so
* the whole event needs to be stored for updating the calendar file when it is
* displayed.
*/
MessageWin::MessageWin(const KAEvent* event, const KAAlarm& alarm, int flags)
    : MainWindowBase(0, static_cast<Qt::WFlags>(WFLAGS | WFLAGS2 | ((flags & ALWAYS_HIDE) || getWorkAreaAndModal() ? Qt::WindowType(0) : Qt::X11BypassWindowManagerHint))),
      mMessage(event->cleanText()),
      mFont(event->font()),
      mBgColour(event->bgColour()),
      mFgColour(event->fgColour()),
#ifdef USE_AKONADI
      mEventItemId(event->itemId()),
#endif
      mEventID(event->id()),
      mAudioFile(event->audioFile()),
      mVolume(event->soundVolume()),
      mFadeVolume(event->fadeVolume()),
      mFadeSeconds(qMin(event->fadeSeconds(), 86400)),
      mDefaultDeferMinutes(event->deferDefaultMinutes()),
      mAlarmType(alarm.type()),
      mAction(event->actionSubType()),
#ifdef KMAIL_SUPPORTED
      mKMailSerialNumber(event->kmailSerialNumber()),
#else
      mKMailSerialNumber(0),
#endif
      mCommandError(event->commandError()),
      mRestoreHeight(0),
      mAudioRepeatPause(event->repeatSoundPause()),
      mConfirmAck(event->confirmAck()),
      mNoDefer(true),
      mInvalid(false),
      mEvent(*event),
      mOriginalEvent(*event),
#ifdef USE_AKONADI
      mCollection(AlarmCalendar::resources()->collectionForEvent(mEventItemId)),
#else
      mResource(AlarmCalendar::resources()->resourceForEvent(mEventID)),
#endif
      mTimeLabel(0),
      mRemainingText(0),
      mEditButton(0),
      mDeferButton(0),
      mSilenceButton(0),
      mKMailButton(0),
      mCommandText(0),
      mDontShowAgainCheck(0),
      mEditDlg(0),
      mDeferDlg(0),
      mAlwaysHide(flags & ALWAYS_HIDE),
      mErrorWindow(false),
      mInitialised(false),
      mNoPostAction(alarm.type() & KAAlarm::REMINDER_ALARM),
      mRecreating(false),
      mBeep(event->beep()),
      mSpeak(event->speak()),
      mRescheduleEvent(!(flags & NO_RESCHEDULE)),
      mShown(false),
      mPositioning(false),
      mNoCloseConfirm(false),
      mDisableDeferral(false)
{
    kDebug() << "event";
    setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags));
    setWindowModality(Qt::WindowModal);
    setObjectName("MessageWin");    // used by LikeBack
    if (alarm.type() & KAAlarm::REMINDER_ALARM)
    {
        if (event->reminderMinutes() < 0)
        {
            event->previousOccurrence(alarm.dateTime(false).effectiveKDateTime(), mDateTime, false);
            if (!mDateTime.isValid()  &&  event->repeatAtLogin())
                mDateTime = alarm.dateTime().addSecs(event->reminderMinutes() * 60);
        }
        else
            mDateTime = event->mainDateTime(true);
    }
    else
        mDateTime = alarm.dateTime(true);
    if (!(flags & (NO_INIT_VIEW | ALWAYS_HIDE)))
    {
#ifdef USE_AKONADI
        bool readonly = AlarmCalendar::resources()->eventReadOnly(mEventItemId);
#else
        bool readonly = AlarmCalendar::resources()->eventReadOnly(mEventID);
#endif
        mShowEdit = !mEventID.isEmpty()  &&  !readonly;
        mNoDefer  = readonly || (flags & NO_DEFER) || alarm.repeatAtLogin();
        initView();
    }
    // Set to save settings automatically, but don't save window size.
    // File alarm window size is saved elsewhere.
    setAutoSaveSettings(QLatin1String("MessageWin"), false);
    mWindowList.append(this);
    if (event->autoClose())
        mCloseTime = alarm.dateTime().effectiveDateTime().addSecs(event->lateCancel() * 60);
    if (mAlwaysHide)
    {
        hide();
        displayComplete();    // play audio, etc.
    }
}

/******************************************************************************
* Display an error message window.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
void MessageWin::showError(const KAEvent& event, const DateTime& alarmDateTime,
                           const QStringList& errmsgs, const QString& dontShowAgain)
{
    if (!dontShowAgain.isEmpty()
    &&  KAlarm::dontShowErrors(event.id(), dontShowAgain))
        return;

    // Don't pile up duplicate error messages for the same alarm
    for (int i = 0, end = mWindowList.count();  i < end;  ++i)
    {
        MessageWin* w = mWindowList[i];
        if (w->mErrorWindow  &&  w->mEventID == event.id()
        &&  w->mErrorMsgs == errmsgs  &&  w->mDontShowAgain == dontShowAgain)
            return;
    }

    (new MessageWin(&event, alarmDateTime, errmsgs, dontShowAgain))->show();
}

/******************************************************************************
* Construct the message window for a specified error message.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
MessageWin::MessageWin(const KAEvent* event, const DateTime& alarmDateTime,
                       const QStringList& errmsgs, const QString& dontShowAgain)
    : MainWindowBase(0, WFLAGS | WFLAGS2),
      mMessage(event->cleanText()),
      mDateTime(alarmDateTime),
#ifdef USE_AKONADI
      mEventItemId(event->itemId()),
#endif
      mEventID(event->id()),
      mAlarmType(KAAlarm::MAIN_ALARM),
      mAction(event->actionSubType()),
      mKMailSerialNumber(0),
      mCommandError(KAEvent::CMD_NO_ERROR),
      mErrorMsgs(errmsgs),
      mDontShowAgain(dontShowAgain),
      mRestoreHeight(0),
      mConfirmAck(false),
      mShowEdit(false),
      mNoDefer(true),
      mInvalid(false),
      mEvent(*event),
      mOriginalEvent(*event),
#ifndef USE_AKONADI
      mResource(0),
#endif
      mTimeLabel(0),
      mRemainingText(0),
      mEditButton(0),
      mDeferButton(0),
      mSilenceButton(0),
      mKMailButton(0),
      mCommandText(0),
      mDontShowAgainCheck(0),
      mEditDlg(0),
      mDeferDlg(0),
      mAlwaysHide(false),
      mErrorWindow(true),
      mInitialised(false),
      mNoPostAction(true),
      mRecreating(false),
      mRescheduleEvent(false),
      mShown(false),
      mPositioning(false),
      mNoCloseConfirm(false),
      mDisableDeferral(false)
{
    kDebug() << "errmsg";
    setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags));
    setWindowModality(Qt::WindowModal);
    setObjectName("ErrorWin");    // used by LikeBack
    getWorkAreaAndModal();
    initView();
    mWindowList.append(this);
}

/******************************************************************************
* Construct the message window for restoration by session management.
* The window is initialised by readProperties().
*/
MessageWin::MessageWin()
    : MainWindowBase(0, WFLAGS),
      mTimeLabel(0),
      mRemainingText(0),
      mEditButton(0),
      mDeferButton(0),
      mSilenceButton(0),
      mKMailButton(0),
      mCommandText(0),
      mDontShowAgainCheck(0),
      mEditDlg(0),
      mDeferDlg(0),
      mAlwaysHide(false),
      mErrorWindow(false),
      mInitialised(false),
      mRecreating(false),
      mRescheduleEvent(false),
      mShown(false),
      mPositioning(false),
      mNoCloseConfirm(false),
      mDisableDeferral(false)
{
    kDebug() << "restore";
    setAttribute(WidgetFlags);
    setWindowModality(Qt::WindowModal);
    setObjectName("RestoredMsgWin");    // used by LikeBack
    getWorkAreaAndModal();
    mWindowList.append(this);
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageWin::~MessageWin()
{
    kDebug() << mEventID;
    if (mAudioOwner == this  &&  mAudioThread)
        mAudioThread->quit();
    mErrorMessages.remove(mEventID);
    mWindowList.removeAll(this);
    if (!mRecreating)
    {
        if (!mNoPostAction  &&  !mEvent.postAction().isEmpty())
            theApp()->alarmCompleted(mEvent);
        if (!instanceCount(true))
            theApp()->quitIf();   // no visible windows remain - check whether to quit
    }
}

/******************************************************************************
* Construct the message window.
*/
void MessageWin::initView()
{
    bool reminder = (!mErrorWindow  &&  (mAlarmType & KAAlarm::REMINDER_ALARM));
    int leading = fontMetrics().leading();
    setCaption((mAlarmType & KAAlarm::REMINDER_ALARM) ? i18nc("@title:window", "Reminder") : i18nc("@title:window", "Message"));
    QWidget* topWidget = new QWidget(this);
    setCentralWidget(topWidget);
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->setMargin(KDialog::marginHint());
    topLayout->setSpacing(KDialog::spacingHint());

    QPalette labelPalette = palette();
    labelPalette.setColor(backgroundRole(), labelPalette.color(QPalette::Window));

    // Show the alarm date/time, together with a reminder text where appropriate.
    // Alarm date/time: display time zone if not local time zone.
    mTimeLabel = new QLabel(topWidget);
    mTimeLabel->setText(dateTimeToDisplay());
    mTimeLabel->setFrameStyle(QFrame::StyledPanel);
    mTimeLabel->setPalette(labelPalette);
    mTimeLabel->setAutoFillBackground(true);
    topLayout->addWidget(mTimeLabel, 0, Qt::AlignHCenter);
    mTimeLabel->setWhatsThis(i18nc("@info:whatsthis", "The scheduled date/time for the message (as opposed to the actual time of display)."));

    if (mDateTime.isValid())
    {
        // Reminder
        if (reminder)
        {
            QString s = i18nc("@info", "Reminder");
            QRegExp re("^(<[^>]+>)*");
            re.indexIn(s);
            s.insert(re.matchedLength(), mTimeLabel->text() + "<br/>");
            mTimeLabel->setText(s);
            mTimeLabel->setAlignment(Qt::AlignHCenter);
        }
    }
    else
        mTimeLabel->hide();

    if (!mErrorWindow)
    {
        // It's a normal alarm message window
        switch (mAction)
        {
            case KAEvent::FILE:
            {
                // Display the file name
                KSqueezedTextLabel* label = new KSqueezedTextLabel(mMessage, topWidget);
                label->setFrameStyle(QFrame::StyledPanel);
                label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
                label->setPalette(labelPalette);
                label->setAutoFillBackground(true);
                label->setWhatsThis(i18nc("@info:whatsthis", "The file whose contents are displayed below"));
                topLayout->addWidget(label, 0, Qt::AlignHCenter);

                // Display contents of file
                bool opened = false;
                bool dir = false;
                QString tmpFile;
                KUrl url(mMessage);
                if (KIO::NetAccess::download(url, tmpFile, MainWindow::mainMainWindow()))
                {
                    QFile qfile(tmpFile);
                    QFileInfo info(qfile);
                    if (!(dir = info.isDir()))
                    {
                        opened = true;
                        KTextBrowser* view = new KTextBrowser(topWidget);
                        view->setFrameStyle(QFrame::NoFrame);
                        view->setWordWrapMode(QTextOption::NoWrap);
                        QPalette pal = view->viewport()->palette();
                        pal.setColor(view->viewport()->backgroundRole(), mBgColour);
                        view->viewport()->setPalette(pal);
                        view->setTextColor(mFgColour);
                        view->setCurrentFont(mFont);
                        KMimeType::Ptr mime = KMimeType::findByUrl(url);
                        if (mime->is("application/octet-stream"))
                            mime = KMimeType::findByFileContent(tmpFile);
                        switch (KAlarm::fileType(mime))
                        {
                            case KAlarm::Image:
                                view->setHtml("<img source=\"" + tmpFile + "\">");
                                break;
                            case KAlarm::TextFormatted:
                                view->QTextBrowser::setSource(tmpFile);   //krazy:exclude=qclasses
                                break;
                            default:
                            {
                                // Assume a plain text file
                                if (qfile.open(QIODevice::ReadOnly))
                                {
                                    QTextStream str(&qfile);

                                    view->setPlainText(str.readAll());
                                    qfile.close();
                                }
                                break;
                            }
                        }
                        view->setMinimumSize(view->sizeHint());
                        topLayout->addWidget(view);

                        // Set the default size to 20 lines square.
                        // Note that after the first file has been displayed, this size
                        // is overridden by the user-set default stored in the config file.
                        // So there is no need to calculate an accurate size.
                        int h = 20*view->fontMetrics().lineSpacing() + 2*view->frameWidth();
                        view->resize(QSize(h, h).expandedTo(view->sizeHint()));
                        view->setWhatsThis(i18nc("@info:whatsthis", "The contents of the file to be displayed"));
                    }
                    KIO::NetAccess::removeTempFile(tmpFile);
                }
                if (!opened)
                {
                    // File couldn't be opened
                    bool exists = KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, MainWindow::mainMainWindow());
                    mErrorMsgs += dir ? i18nc("@info", "File is a folder") : exists ? i18nc("@info", "Failed to open file") : i18nc("@info", "File not found");
                }
                break;
            }
            case KAEvent::MESSAGE:
            {
                // Message label
                // Using MessageText instead of QLabel allows scrolling and mouse copying
                MessageText* text = new MessageText(topWidget);
                text->setAutoFillBackground(true);
                text->setBackgroundColour(mBgColour);
                text->setTextColor(mFgColour);
                text->setCurrentFont(mFont);
                text->insertPlainText(mMessage);
                int lineSpacing = text->fontMetrics().lineSpacing();
                QSize s = text->sizeHint();
                int h = s.height();
                text->setMaximumHeight(h + text->scrollBarHeight());
                text->setMinimumHeight(qMin(h, lineSpacing*4));
                text->setMaximumWidth(s.width() + text->scrollBarWidth());
                text->setWhatsThis(i18nc("@info:whatsthis", "The alarm message"));
                int vspace = lineSpacing/2;
                int hspace = lineSpacing - KDialog::marginHint();
                topLayout->addSpacing(vspace);
                topLayout->addStretch();
                // Don't include any horizontal margins if message is 2/3 screen width
                if (text->sizeHint().width() >= KAlarm::desktopWorkArea(mScreenNumber).width()*2/3)
                    topLayout->addWidget(text, 1, Qt::AlignHCenter);
                else
                {
                    QHBoxLayout* layout = new QHBoxLayout();
                    layout->addSpacing(hspace);
                    layout->addWidget(text, 1, Qt::AlignHCenter);
                    layout->addSpacing(hspace);
                    topLayout->addLayout(layout);
                }
                if (!reminder)
                    topLayout->addStretch();
                break;
            }
            case KAEvent::COMMAND:
            {
                mCommandText = new MessageText(topWidget);
                mCommandText->setBackgroundColour(mBgColour);
                mCommandText->setTextColor(mFgColour);
                mCommandText->setCurrentFont(mFont);
                topLayout->addWidget(mCommandText);
                mCommandText->setWhatsThis(i18nc("@info:whatsthis", "The output of the alarm's command"));
                theApp()->execCommandAlarm(mEvent, mEvent.alarm(mAlarmType), this, SLOT(readProcessOutput(ShellProcess*)));
                break;
            }
            case KAEvent::EMAIL:
            default:
                break;
        }

        if (reminder  &&  mEvent.reminderMinutes() > 0)
        {
            // Advance reminder: show remaining time until the actual alarm
            mRemainingText = new QLabel(topWidget);
            mRemainingText->setFrameStyle(QFrame::Box | QFrame::Raised);
            mRemainingText->setMargin(leading);
            mRemainingText->setPalette(labelPalette);
            mRemainingText->setAutoFillBackground(true);
            if (mDateTime.isDateOnly()  ||  KDateTime::currentLocalDate().daysTo(mDateTime.date()) > 0)
            {
                setRemainingTextDay();
                MidnightTimer::connect(this, SLOT(setRemainingTextDay()));    // update every day
            }
            else
            {
                setRemainingTextMinute();
                MinuteTimer::connect(this, SLOT(setRemainingTextMinute()));   // update every minute
            }
            topLayout->addWidget(mRemainingText, 0, Qt::AlignHCenter);
            topLayout->addSpacing(KDialog::spacingHint());
            topLayout->addStretch();
        }
    }
    else
    {
        // It's an error message
        switch (mAction)
        {
            case KAEvent::EMAIL:
            {
                // Display the email addresses and subject.
                QFrame* frame = new QFrame(topWidget);
                frame->setFrameStyle(QFrame::Box | QFrame::Raised);
                frame->setWhatsThis(i18nc("@info:whatsthis", "The email to send"));
                topLayout->addWidget(frame, 0, Qt::AlignHCenter);
                QGridLayout* grid = new QGridLayout(frame);
                grid->setMargin(KDialog::marginHint());
                grid->setSpacing(KDialog::spacingHint());

                QLabel* label = new QLabel(i18nc("@info Email addressee", "To:"), frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 0, 0, Qt::AlignLeft);
                label = new QLabel(mEvent.emailAddresses("\n"), frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 0, 1, Qt::AlignLeft);

                label = new QLabel(i18nc("@info Email subject", "Subject:"), frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 1, 0, Qt::AlignLeft);
                label = new QLabel(mEvent.emailSubject(), frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 1, 1, Qt::AlignLeft);
                break;
            }
            case KAEvent::COMMAND:
            case KAEvent::FILE:
            case KAEvent::MESSAGE:
            default:
                // Just display the error message strings
                break;
        }
    }

    if (!mErrorMsgs.count())
    {
        topWidget->setAutoFillBackground(true);
        QPalette palette = topWidget->palette();
        palette.setColor(topWidget->backgroundRole(), mBgColour);
        topWidget->setPalette(palette);
    }
    else
    {
        setCaption(i18nc("@title:window", "Error"));
        QHBoxLayout* layout = new QHBoxLayout();
        layout->setMargin(2*KDialog::marginHint());
        layout->addStretch();
        topLayout->addLayout(layout);
        QLabel* label = new QLabel(topWidget);
        label->setPixmap(DesktopIcon("dialog-error"));
        label->setFixedSize(label->sizeHint());
        layout->addWidget(label, 0, Qt::AlignRight);
        QVBoxLayout* vlayout = new QVBoxLayout();
        layout->addLayout(vlayout);
        for (QStringList::Iterator it = mErrorMsgs.begin();  it != mErrorMsgs.end();  ++it)
        {
            label = new QLabel(*it, topWidget);
            label->setFixedSize(label->sizeHint());
            vlayout->addWidget(label, 0, Qt::AlignLeft);
        }
        layout->addStretch();
        if (!mDontShowAgain.isEmpty())
        {
            mDontShowAgainCheck = new QCheckBox(i18nc("@option:check", "Do not display this error message again for this alarm"), topWidget);
            mDontShowAgainCheck->setFixedSize(mDontShowAgainCheck->sizeHint());
            topLayout->addWidget(mDontShowAgainCheck, 0, Qt::AlignLeft);
        }
    }

    QGridLayout* grid = new QGridLayout();
    grid->setColumnStretch(0, 1);     // keep the buttons right-adjusted in the window
    topLayout->addLayout(grid);
    int gridIndex = 1;

    // Close button
    mOkButton = new PushButton(KStandardGuiItem::close(), topWidget);
    // Prevent accidental acknowledgement of the message if the user is typing when the window appears
    mOkButton->clearFocus();
    mOkButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
    mOkButton->setFixedSize(mOkButton->sizeHint());
    connect(mOkButton, SIGNAL(clicked()), SLOT(slotOk()));
    grid->addWidget(mOkButton, 0, gridIndex++, Qt::AlignHCenter);
    mOkButton->setWhatsThis(i18nc("@info:whatsthis", "Acknowledge the alarm"));

    if (mShowEdit)
    {
        // Edit button
        mEditButton = new PushButton(i18nc("@action:button", "&Edit..."), topWidget);
        mEditButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
        mEditButton->setFixedSize(mEditButton->sizeHint());
        connect(mEditButton, SIGNAL(clicked()), SLOT(slotEdit()));
        grid->addWidget(mEditButton, 0, gridIndex++, Qt::AlignHCenter);
        mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the alarm."));
    }

    // Defer button
    mDeferButton = new PushButton(i18nc("@action:button", "&Defer..."), topWidget);
    mDeferButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
    mDeferButton->setFixedSize(mDeferButton->sizeHint());
    connect(mDeferButton, SIGNAL(clicked()), SLOT(slotDefer()));
    grid->addWidget(mDeferButton, 0, gridIndex++, Qt::AlignHCenter);
    mDeferButton->setWhatsThis(i18nc("@info:whatsthis", "<para>Defer the alarm until later.</para>"
                                    "<para>You will be prompted to specify when the alarm should be redisplayed.</para>"));

    if (mNoDefer)
        mDeferButton->hide();
    else
        setDeferralLimit(mEvent);    // ensure that button is disabled when alarm can't be deferred any more

    if (!mAudioFile.isEmpty()  &&  (mVolume || mFadeVolume > 0))
    {
        // Silence button to stop sound repetition
        QPixmap pixmap = MainBarIcon("media-playback-stop");
        mSilenceButton = new PushButton(topWidget);
        mSilenceButton->setIcon(KIcon(pixmap));
        grid->addWidget(mSilenceButton, 0, gridIndex++, Qt::AlignHCenter);
        mSilenceButton->setToolTip(i18nc("@info:tooltip", "Stop sound"));
        mSilenceButton->setWhatsThis(i18nc("@info:whatsthis", "Stop playing the sound"));
        // To avoid getting in a mess, disable the button until sound playing has been set up
        mSilenceButton->setEnabled(false);
    }

    KIconLoader iconLoader;
    if (mKMailSerialNumber)
    {
        // KMail button
        QPixmap pixmap = iconLoader.loadIcon(QLatin1String("internet-mail"), KIconLoader::MainToolbar);
        mKMailButton = new PushButton(topWidget);
        mKMailButton->setIcon(KIcon(pixmap));
        connect(mKMailButton, SIGNAL(clicked()), SLOT(slotShowKMailMessage()));
        grid->addWidget(mKMailButton, 0, gridIndex++, Qt::AlignHCenter);
        mKMailButton->setToolTip(i18nc("@info:tooltip Locate this email in KMail", "Locate in <application>KMail</application>"));
        mKMailButton->setWhatsThis(i18nc("@info:whatsthis", "Locate and highlight this email in <application>KMail</application>"));
    }

    // KAlarm button
    QPixmap pixmap = iconLoader.loadIcon(KGlobal::mainComponent().aboutData()->appName(), KIconLoader::MainToolbar);
    mKAlarmButton = new PushButton(topWidget);
    mKAlarmButton->setIcon(KIcon(pixmap));
    connect(mKAlarmButton, SIGNAL(clicked()), SLOT(displayMainWindow()));
    grid->addWidget(mKAlarmButton, 0, gridIndex++, Qt::AlignHCenter);
    mKAlarmButton->setToolTip(i18nc("@info:tooltip", "Activate <application>KAlarm</application>"));
    mKAlarmButton->setWhatsThis(i18nc("@info:whatsthis", "Activate <application>KAlarm</application>"));

    int butsize = mKAlarmButton->sizeHint().height();
    if (mSilenceButton)
        butsize = qMax(butsize, mSilenceButton->sizeHint().height());
    if (mKMailButton)
        butsize = qMax(butsize, mKMailButton->sizeHint().height());
    mKAlarmButton->setFixedSize(butsize, butsize);
    if (mSilenceButton)
        mSilenceButton->setFixedSize(butsize, butsize);
    if (mKMailButton)
        mKMailButton->setFixedSize(butsize, butsize);

    // Disable all buttons initially, to prevent accidental clicking on if they happen to be
    // under the mouse just as the window appears.
    mOkButton->setEnabled(false);
    if (mDeferButton->isVisible())
        mDeferButton->setEnabled(false);
    if (mEditButton)
        mEditButton->setEnabled(false);
    if (mKMailButton)
        mKMailButton->setEnabled(false);
    mKAlarmButton->setEnabled(false);

    topLayout->activate();
    setMinimumSize(QSize(grid->sizeHint().width() + 2*KDialog::marginHint(), sizeHint().height()));
    bool modal = !(windowFlags() & Qt::X11BypassWindowManagerHint);
    unsigned long wstate = (modal ? NET::Modal : 0) | NET::Sticky | NET::StaysOnTop;
    WId winid = winId();
    KWindowSystem::setState(winid, wstate);
    KWindowSystem::setOnAllDesktops(winid, true);

    mInitialised = true;   // the window's widgets have been created
}

/******************************************************************************
* Return the number of message windows, optionally excluding always-hidden ones.
*/
int MessageWin::instanceCount(bool excludeAlwaysHidden)
{
    int count = mWindowList.count();
    if (excludeAlwaysHidden)
    {
        foreach (MessageWin* win, mWindowList)
        {
            if (win->mAlwaysHide)
                --count;
        }
    }
    return count;
}

bool MessageWin::hasDefer() const
{
    return mDeferButton && mDeferButton->isVisible();
}

/******************************************************************************
* Show the Defer button when it was previously hidden.
*/
void MessageWin::showDefer()
{
    if (mDeferButton)
    {
        mNoDefer = false;
        mDeferButton->show();
        setDeferralLimit(mEvent);    // ensure that button is disabled when alarm can't be deferred any more
        resize(sizeHint());
    }
}

/******************************************************************************
* Convert a reminder window into a normal alarm window.
*/
void MessageWin::cancelReminder(const KAEvent& event, const KAAlarm& alarm)
{
    if (!mInitialised)
        return;
    mDateTime = alarm.dateTime(true);
    mNoPostAction = false;
    mAlarmType = alarm.type();
    if (event.autoClose())
        mCloseTime = alarm.dateTime().effectiveDateTime().addSecs(event.lateCancel() * 60);
    setCaption(i18nc("@title:window", "Message"));
    mTimeLabel->setText(dateTimeToDisplay());
    if (mRemainingText)
        mRemainingText->hide();
    MidnightTimer::disconnect(this, SLOT(setRemainingTextDay()));
    MinuteTimer::disconnect(this, SLOT(setRemainingTextMinute()));
    setMinimumHeight(0);
    centralWidget()->layout()->activate();
    setMinimumHeight(sizeHint().height());
    resize(sizeHint());
}

/******************************************************************************
* Show the alarm's trigger time.
* This is assumed to have previously been hidden.
*/
void MessageWin::showDateTime(const KAEvent& event, const KAAlarm& alarm)
{
    if (!mTimeLabel)
        return;
    mDateTime = (alarm.type() & KAAlarm::REMINDER_ALARM) ? event.mainDateTime(true) : alarm.dateTime(true);
    if (mDateTime.isValid())
    {
        mTimeLabel->setText(dateTimeToDisplay());
        mTimeLabel->show();
    }
}

/******************************************************************************
* Get the trigger time to display.
*/
QString MessageWin::dateTimeToDisplay()
{
    QString tm;
    if (mDateTime.isValid())
    {
        if (mDateTime.isDateOnly())
            tm = KGlobal::locale()->formatDate(mDateTime.date(), KLocale::ShortDate);
        else
        {
            bool showZone = false;
            if (mDateTime.timeType() == KDateTime::UTC
            ||  (mDateTime.timeType() == KDateTime::TimeZone && !mDateTime.isLocalZone()))
            {
                // Display time zone abbreviation if it's different from the local
                // zone. Note that the iCalendar time zone might represent the local
                // time zone in a slightly different way from the system time zone,
                // so the zone comparison above might not produce the desired result.
                QString tz = mDateTime.kDateTime().toString(QString::fromLatin1("%Z"));
                KDateTime local = mDateTime.kDateTime();
                local.setTimeSpec(KDateTime::Spec::LocalZone());
                showZone = (local.toString(QString::fromLatin1("%Z")) != tz);
            }
            tm = KGlobal::locale()->formatDateTime(mDateTime.kDateTime(), KLocale::ShortDate, KLocale::DateTimeFormatOptions(showZone ? KLocale::TimeZone : 0));
        }
    }
    return tm;
}

/******************************************************************************
* Set the remaining time text in a reminder window.
* Called at the start of every day (at the user-defined start-of-day time).
*/
void MessageWin::setRemainingTextDay()
{
    QString text;
    int days = KDateTime::currentLocalDate().daysTo(mDateTime.date());
    if (days <= 0  &&  !mDateTime.isDateOnly())
    {
        // The alarm is due today, so start refreshing every minute
        MidnightTimer::disconnect(this, SLOT(setRemainingTextDay()));
        setRemainingTextMinute();
        MinuteTimer::connect(this, SLOT(setRemainingTextMinute()));   // update every minute
    }
    else
    {
        if (days <= 0)
            text = i18nc("@info", "Today");
        else if (days % 7)
            text = i18ncp("@info", "Tomorrow", "in %1 days' time", days);
        else
            text = i18ncp("@info", "in 1 week's time", "in %1 weeks' time", days/7);
    }
    mRemainingText->setText(text);
}

/******************************************************************************
* Set the remaining time text in a reminder window.
* Called on every minute boundary.
*/
void MessageWin::setRemainingTextMinute()
{
    QString text;
    int mins = (KDateTime::currentUtcDateTime().secsTo(mDateTime.effectiveKDateTime()) + 59) / 60;
    if (mins < 60)
        text = i18ncp("@info", "in 1 minute's time", "in %1 minutes' time", (mins > 0 ? mins : 0));
    else if (mins % 60 == 0)
        text = i18ncp("@info", "in 1 hour's time", "in %1 hours' time", mins/60);
    else
    {
        QString hourText = i18ncp("@item:intext inserted into 'in ... %1 minute's time' below", "1 hour", "%1 hours", mins/60);
        text = i18ncp("@info '%2' is the previous message '1 hour'/'%1 hours'", "in %2 1 minute's time", "in %2 %1 minutes' time", mins%60, hourText);
    }
    mRemainingText->setText(text);
}

/******************************************************************************
* Called when output is available from the command which is providing the text
* for this window. Add the output and resize the window to show it.
*/
void MessageWin::readProcessOutput(ShellProcess* proc)
{
    QByteArray data = proc->readAll();
    if (!data.isEmpty())
    {
        // Strip any trailing newline, to avoid showing trailing blank line
        // in message window.
        if (mCommandText->newLine())
            mCommandText->append("\n");
        int nl = data.endsWith('\n') ? 1 : 0;
        mCommandText->setNewLine(nl);
        mCommandText->insertPlainText(QString::fromLocal8Bit(data.data(), data.length() - nl));
        resize(sizeHint());
    }
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageWin::saveProperties(KConfigGroup& config)
{
    if (mShown  &&  !mErrorWindow  &&  !mAlwaysHide)
    {
        config.writeEntry("EventID", mEventID);
#ifdef USE_AKONADI
        config.writeEntry("EventItemID", mEventItemId);
#endif
        config.writeEntry("AlarmType", static_cast<int>(mAlarmType));
        if (mAlarmType == KAAlarm::INVALID_ALARM)
            kError() << "Invalid alarm: id=" << mEventID << ", alarm count=" << mEvent.alarmCount();
        config.writeEntry("Message", mMessage);
        config.writeEntry("Type", static_cast<int>(mAction));
        config.writeEntry("Font", mFont);
        config.writeEntry("BgColour", mBgColour);
        config.writeEntry("FgColour", mFgColour);
        config.writeEntry("ConfirmAck", mConfirmAck);
        if (mDateTime.isValid())
        {
//TODO: Write KDateTime when it becomes possible
            config.writeEntry("Time", mDateTime.effectiveDateTime());
            config.writeEntry("DateOnly", mDateTime.isDateOnly());
            QString zone;
            if (mDateTime.isUtc())
                zone = QLatin1String("UTC");
            else
            {
                KTimeZone tz = mDateTime.timeZone();
                if (tz.isValid())
                    zone = tz.name();
            }
            config.writeEntry("TimeZone", zone);
        }
        if (mCloseTime.isValid())
            config.writeEntry("Expiry", mCloseTime);
        if (mAudioRepeatPause >= 0  &&  mSilenceButton  &&  mSilenceButton->isEnabled())
        {
            // Only need to restart sound file playing if it's being repeated
            config.writePathEntry("AudioFile", mAudioFile);
            config.writeEntry("Volume", static_cast<int>(mVolume * 100));
            config.writeEntry("AudioPause", mAudioRepeatPause);
        }
        config.writeEntry("Speak", mSpeak);
        config.writeEntry("Height", height());
        config.writeEntry("DeferMins", mDefaultDeferMinutes);
        config.writeEntry("NoDefer", mNoDefer);
        config.writeEntry("NoPostAction", mNoPostAction);
        config.writeEntry("KMailSerial", static_cast<qulonglong>(mKMailSerialNumber));
        config.writeEntry("CmdErr", static_cast<int>(mCommandError));
        config.writeEntry("DontShowAgain", mDontShowAgain);
    }
    else
        config.writeEntry("Invalid", true);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(const KConfigGroup& config)
{
    mInvalid             = config.readEntry("Invalid", false);
    mEventID             = config.readEntry("EventID");
#ifdef USE_AKONADI
    mEventItemId         = config.readEntry("EventItemID", Akonadi::Item::Id(-1));
#endif
    mAlarmType           = static_cast<KAAlarm::Type>(config.readEntry("AlarmType", 0));
    if (mAlarmType == KAAlarm::INVALID_ALARM)
    {
        mInvalid = true;
        kError() << "Invalid alarm: id=" << mEventID;
    }
    mMessage             = config.readEntry("Message");
    mAction              = static_cast<KAEvent::SubAction>(config.readEntry("Type", 0));
    mFont                = config.readEntry("Font", QFont());
    mBgColour            = config.readEntry("BgColour", QColor(Qt::white));
    mFgColour            = config.readEntry("FgColour", QColor(Qt::black));
    mConfirmAck          = config.readEntry("ConfirmAck", false);
    QDateTime invalidDateTime;
    QDateTime dt         = config.readEntry("Time", invalidDateTime);
    QString zone         = config.readEntry("TimeZone");
    if (zone.isEmpty())
        mDateTime = KDateTime(dt, KDateTime::ClockTime);
    else if (zone == QLatin1String("UTC"))
    {
        dt.setTimeSpec(Qt::UTC);
        mDateTime = KDateTime(dt, KDateTime::UTC);
    }
    else
    {
        KTimeZone tz = KSystemTimeZones::zone(zone);
        mDateTime = KDateTime(dt, (tz.isValid() ? tz : KSystemTimeZones::local()));
    }
    bool dateOnly        = config.readEntry("DateOnly", false);
    if (dateOnly)
        mDateTime.setDateOnly(true);
    mCloseTime           = config.readEntry("Expiry", invalidDateTime);
    mAudioFile           = config.readPathEntry("AudioFile", QString());
    mVolume              = static_cast<float>(config.readEntry("Volume", 0)) / 100;
    mFadeVolume          = -1;
    mFadeSeconds         = 0;
    if (!mAudioFile.isEmpty())   // audio file URL was only saved if it repeats
        mAudioRepeatPause = config.readEntry("AudioPause", 0);
    mSpeak               = config.readEntry("Speak", false);
    mRestoreHeight       = config.readEntry("Height", 0);
    mDefaultDeferMinutes = config.readEntry("DeferMins", 0);
    mNoDefer             = config.readEntry("NoDefer", false);
    mNoPostAction        = config.readEntry("NoPostAction", true);
    mKMailSerialNumber   = static_cast<unsigned long>(config.readEntry("KMailSerial", QVariant(QVariant::ULongLong)).toULongLong());
    mCommandError        = KAEvent::CmdErrType(config.readEntry("CmdErr", static_cast<int>(KAEvent::CMD_NO_ERROR)));
    mDontShowAgain       = config.readEntry("DontShowAgain", QString());
    mShowEdit            = false;
#ifdef USE_AKONADI
    mCollection          = Akonadi::Collection();
#else
    mResource            = 0;
#endif
    kDebug() << mEventID;
    if (mAlarmType != KAAlarm::INVALID_ALARM)
    {
        // Recreate the event from the calendar file (if possible)
        if (!mEventID.isEmpty())
        {
            KAEvent* event = AlarmCalendar::resources()->event(mEventID);
            if (event)
            {
                mEvent = *event;
#ifdef USE_AKONADI
                mCollection = AkonadiModel::instance()->collectionForItem(mEventItemId);
#else
                mResource = AlarmCalendar::resources()->resourceForEvent(mEventID);
#endif
                mShowEdit = true;
            }
            else
            {
                // It's not in the active calendar, so try the displaying or archive calendars
#ifdef USE_AKONADI
                retrieveEvent(mEvent, mCollection, mShowEdit, mNoDefer);
#else
                retrieveEvent(mEvent, mResource, mShowEdit, mNoDefer);
#endif
                mNoDefer = !mNoDefer;
            }
        }
        initView();
    }
}

/******************************************************************************
* Redisplay alarms which were being shown when the program last exited.
* Normally, these alarms will have been displayed by session restoration, but
* if the program crashed or was killed, we can redisplay them here so that
* they won't be lost.
*/
void MessageWin::redisplayAlarms()
{
    AlarmCalendar* cal = AlarmCalendar::displayCalendar();
    if (cal->isOpen())
    {
        KAEvent event;
#ifdef USE_AKONADI
        Akonadi::Collection collection;
#else
        AlarmResource* resource;
#endif
        Event::List events = cal->kcalEvents();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            bool showDefer, showEdit;
#ifdef USE_AKONADI
            reinstateFromDisplaying(events[i], event, collection, showEdit, showDefer);
#else
            reinstateFromDisplaying(events[i], event, resource, showEdit, showDefer);
#endif
            if (!findEvent(event.id()))
            {
                // This event should be displayed, but currently isn't being
                KAAlarm alarm = event.convertDisplayingAlarm();
                if (alarm.type() == KAAlarm::INVALID_ALARM)
                {
                    kError() << "Invalid alarm: id=" << event.id();
                    continue;
                }
                kDebug() << event.id();
                bool login = alarm.repeatAtLogin();
                int flags = NO_RESCHEDULE | (login ? NO_DEFER : 0) | NO_INIT_VIEW;
                MessageWin* win = new MessageWin(&event, alarm, flags);
#ifdef USE_AKONADI
                win->mCollection = collection;
                bool rw = CollectionControlModel::isWritableEnabled(collection, event.category()) > 0;
#else
                win->mResource = resource;
                bool rw = resource  &&  resource->writable();
#endif
                win->mShowEdit = rw ? showEdit : false;
                win->mNoDefer  = (rw && !login) ? !showDefer : true;
                win->initView();
                win->show();
            }
        }
    }
}

/******************************************************************************
* Retrieves the event with the current ID from the displaying calendar file,
* or if not found there, from the archive calendar.
*/
#ifdef USE_AKONADI
bool MessageWin::retrieveEvent(KAEvent& event, Akonadi::Collection& resource, bool& showEdit, bool& showDefer)
#else
bool MessageWin::retrieveEvent(KAEvent& event, AlarmResource*& resource, bool& showEdit, bool& showDefer)
#endif
{
#ifdef USE_AKONADI
    Event::Ptr kcalEvent = AlarmCalendar::displayCalendar()->kcalEvent(CalEvent::uid(mEventID, CalEvent::DISPLAYING));
#else
    const Event* kcalEvent = AlarmCalendar::displayCalendar()->kcalEvent(CalEvent::uid(mEventID, CalEvent::DISPLAYING));
#endif
    if (!reinstateFromDisplaying(kcalEvent, event, resource, showEdit, showDefer))
    {
        // The event isn't in the displaying calendar.
        // Try to retrieve it from the archive calendar.
        KAEvent* ev = AlarmCalendar::resources()->event(CalEvent::uid(mEventID, CalEvent::ARCHIVED));
        if (!ev)
            return false;
        event = *ev;
        event.setArchive();     // ensure that it gets re-archived if it's saved
        event.setCategory(CalEvent::ACTIVE);
        if (mEventID != event.id())
            kError() << "Wrong event ID";
        event.setEventId(mEventID);
#ifdef USE_AKONADI
        resource  = Akonadi::Collection();
#else
        resource  = 0;
#endif
        showEdit  = true;
        showDefer = true;
        kDebug() << event.id() << ": success";
    }
    return true;
}

/******************************************************************************
* Retrieves the displayed event from the calendar file, or if not found there,
* from the displaying calendar.
*/
#ifdef USE_AKONADI
bool MessageWin::reinstateFromDisplaying(const Event::Ptr& kcalEvent, KAEvent& event, Akonadi::Collection& collection, bool& showEdit, bool& showDefer)
#else
bool MessageWin::reinstateFromDisplaying(const Event* kcalEvent, KAEvent& event, AlarmResource*& resource, bool& showEdit, bool& showDefer)
#endif
{
    if (!kcalEvent)
        return false;
#ifdef USE_AKONADI
    Akonadi::Collection::Id collectionId;
    event.reinstateFromDisplaying(kcalEvent, collectionId, showEdit, showDefer);
    collection = AkonadiModel::instance()->collectionById(collectionId);
#else
    QString resourceID;
    event.reinstateFromDisplaying(kcalEvent, resourceID, showEdit, showDefer);
    resource = AlarmResources::instance()->resourceWithId(resourceID);
    if (resource  &&  !resource->isOpen())
        resource = 0;
#endif
    kDebug() << event.id() << ": success";
    return true;
}

/******************************************************************************
* Called when an alarm is currently being displayed, to store a copy of the
* alarm in the displaying calendar, and to reschedule it for its next repetition.
* If no repetitions remain, cancel it.
*/
void MessageWin::alarmShowing(KAEvent& event)
{
    kDebug() << event.id() << "," << KAAlarm::debugType(mAlarmType);
#ifndef USE_AKONADI
    const KCal::Event* kcalEvent = AlarmCalendar::resources()->kcalEvent(event.id());
    if (!kcalEvent)
    {
        kError() << "Event ID not found:" << event.id();
        return;
    }
#endif
    KAAlarm alarm = event.alarm(mAlarmType);
    if (!alarm.isValid())
    {
        kError() << "Alarm type not found:" << event.id() << ":" << mAlarmType;
        return;
    }
    if (!mAlwaysHide)
    {
        // Copy the alarm to the displaying calendar in case of a crash, etc.
        KAEvent* dispEvent = new KAEvent;
#ifdef USE_AKONADI
        Akonadi::Collection collection = AkonadiModel::instance()->collectionForItem(event.itemId());
        dispEvent->setDisplaying(event, mAlarmType, collection.id(),
                    mDateTime.effectiveKDateTime(), mShowEdit, !mNoDefer);
#else
        AlarmResource* resource = AlarmResources::instance()->resource(kcalEvent);
        dispEvent->setDisplaying(event, mAlarmType, (resource ? resource->identifier() : QString()),
                    mDateTime.effectiveKDateTime(), mShowEdit, !mNoDefer);
#endif
        AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
        if (cal)
        {
#ifdef USE_AKONADI
            cal->deleteDisplayEvent(dispEvent->id());   // in case it already exists
            if (!cal->addEvent(*dispEvent))
#else
            cal->deleteEvent(dispEvent->id());   // in case it already exists
            if (!cal->addEvent(dispEvent))
#endif
                delete dispEvent;
            cal->save();
        }
        else
            delete dispEvent;
    }
    theApp()->rescheduleAlarm(event, alarm);
}

/******************************************************************************
* Spread alarm windows over the screen so that they are all visible, or pile
* them on top of each other again.
* Reply = true if windows are now scattered, false if piled up.
*/
bool MessageWin::spread(bool scatter)
{
    if (instanceCount(true) <= 1)    // ignore always-hidden windows
        return false;

    QRect desk = KAlarm::desktopWorkArea();   // get the usable area of the desktop
    if (scatter == isSpread(desk.topLeft()))
        return scatter;

    if (scatter)
    {
        // Usually there won't be many windows, so a crude
        // scattering algorithm should suffice.
        int x = desk.left();
        int y = desk.top();
        int ynext = y;
        for (int errmsgs = 0;  errmsgs < 2;  ++errmsgs)
        {
            // Display alarm messages first, then error messages, since most
            // error messages tend to be the same height.
            for (int i = 0, end = mWindowList.count();  i < end;  ++i)
            {
                MessageWin* w = mWindowList[i];
                if ((!errmsgs && w->mErrorWindow)
                ||  (errmsgs && !w->mErrorWindow))
                    continue;
                QSize sz = w->frameGeometry().size();
                if (x + sz.width() > desk.right())
                {
                    x = desk.left();
                    y = ynext;
                }
                int ytmp = y;
                if (y + sz.height() > desk.bottom())
                {
                    ytmp = desk.bottom() - sz.height();
                    if (ytmp < desk.top())
                        ytmp = desk.top();
                }
                w->move(x, ytmp);
                x += sz.width();
                if (ytmp + sz.height() > ynext)
                    ynext = ytmp + sz.height();
            }
        }
    }
    else
    {
        // Move all windows to the top left corner
        for (int i = 0, end = mWindowList.count();  i < end;  ++i)
            mWindowList[i]->move(desk.topLeft());
    }
    return scatter;
}

/******************************************************************************
* Check whether message windows are all piled up, or are spread out.
* Reply = true if windows are currently spread, false if piled up.
*/
bool MessageWin::isSpread(const QPoint& topLeft)
{
    for (int i = 0, end = mWindowList.count();  i < end;  ++i)
    {
        if (mWindowList[i]->pos() != topLeft)
            return true;
    }
    return false;
}

/******************************************************************************
* Returns the existing message window (if any) which is displaying the event
* with the specified ID.
*/
MessageWin* MessageWin::findEvent(const QString& eventID)
{
    if (!eventID.isEmpty())
    {
        for (int i = 0, end = mWindowList.count();  i < end;  ++i)
        {
            MessageWin* w = mWindowList[i];
            if (w->mEventID == eventID  &&  !w->mErrorWindow)
                return w;
        }
    }
    return 0;
}

/******************************************************************************
* Beep and play the audio file, as appropriate.
*/
void MessageWin::playAudio()
{
    if (mBeep)
    {
        // Beep using two methods, in case the sound card/speakers are switched off or not working
        QApplication::beep();      // beep through the internal speaker
        KNotification::beep();     // beep through the sound card & speakers
    }
    if (!mAudioFile.isEmpty())
    {
        if (!mVolume  &&  mFadeVolume <= 0)
            return;    // ensure zero volume doesn't play anything
        startAudio();    // play the audio file
    }
    else if (mSpeak)
    {
        // The message is to be spoken. In case of error messges,
        // call it on a timer to allow the window to display first.
        QTimer::singleShot(0, this, SLOT(slotSpeak()));
    }
}

/******************************************************************************
* Speak the message.
* Called asynchronously to avoid delaying the display of the message.
*/
void MessageWin::slotSpeak()
{
    QString error;
    OrgKdeKSpeechInterface* kspeech = theApp()->kspeechInterface(error);
    if (!kspeech)
    {
        if (!haveErrorMessage(ErrMsg_Speak))
        {
            KAMessageBox::detailedError(MainWindow::mainMainWindow(), i18nc("@info", "Unable to speak message"), error);
            clearErrorMessage(ErrMsg_Speak);
        }
        return;
    }
    if (!kspeech->say(mMessage, 0))
    {
        kDebug() << "SayMessage() D-Bus error";
        if (!haveErrorMessage(ErrMsg_Speak))
        {
            KAMessageBox::detailedError(MainWindow::mainMainWindow(), i18nc("@info", "Unable to speak message"), i18nc("@info", "D-Bus call say() failed"));
            clearErrorMessage(ErrMsg_Speak);
        }
    }
}

/******************************************************************************
* Called when another window's audio thread has been destructed.
* Start playing this window's audio file. Because initialising the sound system
* and loading the file may take some time, it is called in a separate thread to
* allow the window to display first.
*/
void MessageWin::startAudio()
{
    if (mAudioThread)
    {
        // An audio file is already playing for another message
        // window, so wait until it has finished.
        connect(mAudioThread, SIGNAL(destroyed(QObject*)), SLOT(audioTerminating()));
    }
    else
    {
        kDebug() << QThread::currentThread();
        mAudioThread = new AudioThread(this, mAudioFile, mVolume, mFadeVolume, mFadeSeconds, mAudioRepeatPause);
        mAudioOwner = this;
        connect(mAudioThread, SIGNAL(readyToPlay()), SLOT(playReady()));
        connect(mAudioThread, SIGNAL(finished()), SLOT(playFinished()));
        if (mSilenceButton)
            connect(mSilenceButton, SIGNAL(clicked()), mAudioThread, SLOT(quit()));
        // Notify after creating mAudioThread, so that isAudioPlaying() will
        // return the correct value.
        theApp()->notifyAudioPlaying(true);
        mAudioThread->start();
    }
}

/******************************************************************************
* Return whether audio playback is currently active.
*/
bool MessageWin::isAudioPlaying()
{
    return mAudioThread;
}

/******************************************************************************
* Stop audio playback.
*/
void MessageWin::stopAudio(bool wait)
{
    kDebug();
    if (mAudioThread)
        mAudioThread->stop(wait);
}

/******************************************************************************
* Called when another window's audio thread is being destructed.
* Wait until the destructor has finished.
*/
void MessageWin::audioTerminating()
{
    QTimer::singleShot(0, this, SLOT(startAudio()));
}

/******************************************************************************
* Called when the audio file is ready to start playing.
*/
void MessageWin::playReady()
{
    if (mSilenceButton)
        mSilenceButton->setEnabled(true);
}

/******************************************************************************
* Called when the audio file thread finishes.
*/
void MessageWin::playFinished()
{
    if (mSilenceButton)
        mSilenceButton->setEnabled(false);
    if (!mAudioThread->error().isEmpty())
    {
        if (!haveErrorMessage(ErrMsg_AudioFile))
        {
            KAMessageBox::error(this, mAudioThread->error());
            clearErrorMessage(ErrMsg_AudioFile);
        }
    }
    delete mAudioThread.data();
    // Notify after deleting mAudioThread, so that isAudioPlaying() will
    // return the correct value.
    theApp()->notifyAudioPlaying(false);
    mAudioOwner = 0;
    if (mAlwaysHide)
        close();
}

/******************************************************************************
* Destructor for audio thread. Waits for thread completion and tidies up.
* Note that this destructor is executed in the parent thread.
*/
AudioThread::~AudioThread()
{
    kDebug();
    stop(true);   // stop playing and tidy up (timeout 3 seconds)
    delete mAudioObject;
    mAudioObject = 0;
}

/******************************************************************************
* Quits the thread and waits for thread completion and tidies up.
*/
void AudioThread::stop(bool waiT)
{
    kDebug();
    quit();       // stop playing and tidy up
    wait(3000);   // wait for run() to exit (timeout 3 seconds)
    if (!isFinished())
    {
        // Something has gone wrong - forcibly kill the thread
        terminate();
        if (waiT)
            wait();
    }
}

/******************************************************************************
* Kick off the thread to play the audio file.
*/
void AudioThread::run()
{
    mMutex.lock();
    if (mAudioObject)
    {
        mMutex.unlock();
        return;
    }
    kDebug() << QThread::currentThread() << mFile;
    QString audioFile = mFile;
    mFile = KAlarm::pathOrUrl(mFile);
    Phonon::MediaSource source(audioFile);
    if (source.type() == Phonon::MediaSource::Invalid)
    {
        mError = i18nc("@info", "Cannot open audio file: <filename>%1</filename>", audioFile);
        mMutex.unlock();
        kError() << "Open failure:" << audioFile;
        return;
    }
    mAudioObject = new Phonon::MediaObject();
    mAudioObject->setCurrentSource(source);
    mAudioObject->setTransitionTime(100);   // workaround to prevent clipping of end of files in Xine backend
    Phonon::AudioOutput* output = new Phonon::AudioOutput(Phonon::NotificationCategory, mAudioObject);
    mPath = Phonon::createPath(mAudioObject, output);
    if (mVolume >= 0  ||  mFadeVolume >= 0)
    {
        float vol = (mVolume >= 0) ? mVolume : output->volume();
        float maxvol = qMax(vol, mFadeVolume);
        output->setVolume(maxvol);
        if (mFadeVolume >= 0  &&  mFadeSeconds > 0)
        {
            Phonon::VolumeFaderEffect* fader = new Phonon::VolumeFaderEffect(mAudioObject);
            fader->setVolume(mFadeVolume / maxvol);
            fader->fadeTo(mVolume / maxvol, mFadeSeconds * 1000);
            mPath.insertEffect(fader);
        }
    }
    connect(mAudioObject, SIGNAL(stateChanged(Phonon::State,Phonon::State)), SLOT(playStateChanged(Phonon::State)), Qt::DirectConnection);
    connect(mAudioObject, SIGNAL(finished()), SLOT(checkAudioPlay()), Qt::DirectConnection);
    mPlayedOnce = false;
    mPausing    = false;
    mMutex.unlock();
    emit readyToPlay();
    checkAudioPlay();

    // Start an event loop.
    // The function will exit once exit() or quit() is called.
    // First, ensure that the thread object is deleted once it has completed.
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    exec();
    stopPlay();
}

/******************************************************************************
* Called when the audio file has loaded and is ready to play, or when play
* has completed.
* If it is ready to play, start playing it (for the first time or repeated).
* If play has not yet completed, wait a bit longer.
*/
void AudioThread::checkAudioPlay()
{
    mMutex.lock();
    if (!mAudioObject)
    {
        mMutex.unlock();
        return;
    }
    if (mPausing)
        mPausing = false;
    else
    {
        // The file has loaded and is ready to play, or play has completed
        if (mPlayedOnce)
        {
            if (mRepeatPause < 0)
            {
                // Play has completed
                mMutex.unlock();
                stopPlay();
                return;
            }
            if (mRepeatPause > 0)
            {
                // Pause before playing the file again
                mPausing = true;
                QTimer::singleShot(mRepeatPause * 1000, this, SLOT(checkAudioPlay()));
                mMutex.unlock();
                return;
            }
        }
        mPlayedOnce = true;
    }

    // Start playing the file, either for the first time or again
    kDebug() << "start";
    mAudioObject->play();
    mMutex.unlock();
}

/******************************************************************************
* Called when the playback object changes state.
* If an error has occurred, quit and return the error to the caller.
*/
void AudioThread::playStateChanged(Phonon::State newState)
{
    if (newState == Phonon::ErrorState)
    {
        QMutexLocker locker(&mMutex);
        QString err = mAudioObject->errorString();
        if (!err.isEmpty())
        {
            kError() << "Play failure:" << mFile << ":" << err;
            mError = i18nc("@info", "<para>Error playing audio file: <filename>%1</filename></para><para>%2</para>", mFile, err);
            exit(1);
        }
    }
}

/******************************************************************************
* Called when play completes, the Silence button is clicked, or the window is
* closed, to terminate audio access.
*/
void AudioThread::stopPlay()
{
    mMutex.lock();
    if (mAudioObject)
    {
        mAudioObject->stop();
        QList<Phonon::Effect*> effects = mPath.effects();
        for (int i = 0;  i < effects.count();  ++i)
        {
            mPath.removeEffect(effects[i]);
            delete effects[i];
        }
        delete mAudioObject;
        mAudioObject = 0;
    }
    mMutex.unlock();
    quit();   // exit the event loop, if it's still running
}

QString AudioThread::error() const
{
    QMutexLocker locker(&mMutex);
    return mError;
}

/******************************************************************************
* Raise the alarm window, re-output any required audio notification, and
* reschedule the alarm in the calendar file.
*/
void MessageWin::repeat(const KAAlarm& alarm)
{
    if (!mInitialised)
        return;
    if (mDeferDlg)
    {
        // Cancel any deferral dialog so that the user notices something's going on,
        // and also because the deferral time limit will have changed.
        delete mDeferDlg;
        mDeferDlg = 0;
    }
    KAEvent* event = mEventID.isNull() ? 0 : AlarmCalendar::resources()->event(mEventID);
    if (event)
    {
        mAlarmType = alarm.type();    // store new alarm type for use if it is later deferred
        if (mAlwaysHide)
            playAudio();
        else
        {
            if (!mDeferDlg  ||  Preferences::modalMessages())
            {
                raise();
                playAudio();
            }
            if (mDeferButton->isVisible())
            {
                mDeferButton->setEnabled(true);
                setDeferralLimit(*event);    // ensure that button is disabled when alarm can't be deferred any more
            }
        }
        alarmShowing(*event);
    }
}

/******************************************************************************
* Display the window.
* If windows are being positioned away from the mouse cursor, it is initially
* positioned at the top left to slightly reduce the number of times the
* windows need to be moved in showEvent().
*/
void MessageWin::show()
{
    if (mCloseTime.isValid())
    {
        // Set a timer to auto-close the window
        int delay = KDateTime::currentLocalDateTime().dateTime().secsTo(mCloseTime);
        if (delay < 0)
            delay = 0;
        QTimer::singleShot(delay * 1000, this, SLOT(close()));
        if (!delay)
            return;    // don't show the window if auto-closing is already due
    }
    if (Preferences::messageButtonDelay() == 0)
        move(0, 0);
    MainWindowBase::show();
}

/******************************************************************************
* Returns the window's recommended size exclusive of its frame.
*/
QSize MessageWin::sizeHint() const
{
    QSize desired;
    switch (mAction)
    {
        case KAEvent::MESSAGE:
            desired = MainWindowBase::sizeHint();
            break;
        case KAEvent::COMMAND:
            if (mShown)
            {
                // For command output, expand the window to accommodate the text
                QSize texthint = mCommandText->sizeHint();
                int w = texthint.width() + 2*KDialog::marginHint();
                if (w < width())
                    w = width();
                int ypadding = height() - mCommandText->height();
                desired = QSize(w, texthint.height() + ypadding);
                break;
            }
            // fall through to default
        default:
            return MainWindowBase::sizeHint();
    }

    // Limit the size to fit inside the working area of the desktop
    QSize desktop  = KAlarm::desktopWorkArea(mScreenNumber).size();
    QSize frameThickness = frameGeometry().size() - geometry().size();  // title bar & window frame
    return desired.boundedTo(desktop - frameThickness);
}

/******************************************************************************
* Called when the window is shown.
* The first time, output any required audio notification, and reschedule or
* delete the event from the calendar file.
*/
void MessageWin::showEvent(QShowEvent* se)
{
    MainWindowBase::showEvent(se);
    if (mShown)
        return;
    if (mErrorWindow  ||  mAlarmType == KAAlarm::INVALID_ALARM)
    {
        // Don't bother repositioning error messages,
        // and invalid alarms should be deleted anyway.
        enableButtons();
    }
    else
    {
        /* Set the window size.
         * Note that the frame thickness is not yet known when this
         * method is called, so for large windows the size needs to be
         * set again later.
         */
        bool execComplete = true;
        QSize s = sizeHint();     // fit the window round the message
        if (mAction == KAEvent::FILE  &&  !mErrorMsgs.count())
            KAlarm::readConfigWindowSize("FileMessage", s);
        resize(s);

        QRect desk = KAlarm::desktopWorkArea(mScreenNumber);
        QRect frame = frameGeometry();

        mButtonDelay = Preferences::messageButtonDelay() * 1000;
        if (mButtonDelay)
        {
            // Position the window in the middle of the screen, and
            // delay enabling the buttons.
            mPositioning = true;
            move((desk.width() - frame.width())/2, (desk.height() - frame.height())/2);
            execComplete = false;
        }
        else
        {
            /* Try to ensure that the window can't accidentally be acknowledged
             * by the user clicking the mouse just as it appears.
             * To achieve this, move the window so that the OK button is as far away
             * from the cursor as possible. If the buttons are still too close to the
             * cursor, disable the buttons for a short time.
             * N.B. This can't be done in show(), since the geometry of the window
             *      is not known until it is displayed. Unfortunately by moving the
             *      window in showEvent(), a flicker is unavoidable.
             *      See the Qt documentation on window geometry for more details.
             */
            // PROBLEM: The frame size is not known yet!
            QPoint cursor = QCursor::pos();
            QRect rect  = geometry();
            // Find the offsets from the outside of the frame to the edges of the OK button
            QRect button(mOkButton->mapToParent(QPoint(0, 0)), mOkButton->mapToParent(mOkButton->rect().bottomRight()));
            int buttonLeft   = button.left() + rect.left() - frame.left();
            int buttonRight  = width() - button.right() + frame.right() - rect.right();
            int buttonTop    = button.top() + rect.top() - frame.top();
            int buttonBottom = height() - button.bottom() + frame.bottom() - rect.bottom();

            int centrex = (desk.width() + buttonLeft - buttonRight) / 2;
            int centrey = (desk.height() + buttonTop - buttonBottom) / 2;
            int x = (cursor.x() < centrex) ? desk.right() - frame.width() : desk.left();
            int y = (cursor.y() < centrey) ? desk.bottom() - frame.height() : desk.top();

            // Find the enclosing rectangle for the new button positions
            // and check if the cursor is too near
            QRect buttons = mOkButton->geometry().unite(mKAlarmButton->geometry());
            buttons.translate(rect.left() + x - frame.left(), rect.top() + y - frame.top());
            int minDistance = proximityMultiple * mOkButton->height();
            if ((abs(cursor.x() - buttons.left()) < minDistance
              || abs(cursor.x() - buttons.right()) < minDistance)
            &&  (abs(cursor.y() - buttons.top()) < minDistance
              || abs(cursor.y() - buttons.bottom()) < minDistance))
                mButtonDelay = proximityButtonDelay;    // too near - disable buttons initially

            if (x != frame.left()  ||  y != frame.top())
            {
                mPositioning = true;
                move(x, y);
                execComplete = false;
            }
        }
        if (execComplete)
            displayComplete();    // play audio, etc.
    }

    // Set the window size etc. once the frame size is known
    QTimer::singleShot(0, this, SLOT(frameDrawn()));

    mShown = true;
}

/******************************************************************************
* Called when the window has been moved.
*/
void MessageWin::moveEvent(QMoveEvent* e)
{
    MainWindowBase::moveEvent(e);
    theApp()->setSpreadWindowsState(isSpread(KAlarm::desktopWorkArea(mScreenNumber).topLeft()));
    if (mPositioning)
    {
        // The window has just been initially positioned
        mPositioning = false;
        displayComplete();    // play audio, etc.
    }
}

/******************************************************************************
* Called after (hopefully) the window frame size is known.
* Reset the initial window size if it exceeds the working area of the desktop.
* Set the 'spread windows' menu item status.
*/
void MessageWin::frameDrawn()
{
    if (!mErrorWindow  &&  mAction == KAEvent::MESSAGE)
    {
        QSize s = sizeHint();
        if (width() > s.width()  ||  height() > s.height())
            resize(s);
    }
    theApp()->setSpreadWindowsState(isSpread(KAlarm::desktopWorkArea(mScreenNumber).topLeft()));
}

/******************************************************************************
* Called when the window has been displayed properly (in its correct position),
* to play sounds and reschedule the event.
*/
void MessageWin::displayComplete()
{
    playAudio();
    if (mRescheduleEvent)
        alarmShowing(mEvent);

    if (!mAlwaysHide)
    {
        // Enable the window's buttons either now or after the configured delay
        if (mButtonDelay > 0)
            QTimer::singleShot(mButtonDelay, this, SLOT(enableButtons()));
        else
            enableButtons();
    }
}

/******************************************************************************
* Enable the window's buttons.
*/
void MessageWin::enableButtons()
{
    mOkButton->setEnabled(true);
    mKAlarmButton->setEnabled(true);
    if (mDeferButton->isVisible()  &&  !mDisableDeferral)
        mDeferButton->setEnabled(true);
    if (mEditButton)
        mEditButton->setEnabled(true);
    if (mKMailButton)
        mKMailButton->setEnabled(true);
}

/******************************************************************************
* Called when the window's size has changed (before it is painted).
*/
void MessageWin::resizeEvent(QResizeEvent* re)
{
    if (mRestoreHeight)
    {
        // Restore the window height on session restoration
        if (mRestoreHeight != re->size().height())
        {
            QSize size = re->size();
            size.setHeight(mRestoreHeight);
            resize(size);
        }
        else if (isVisible())
            mRestoreHeight = 0;
    }
    else
    {
        if (mShown  &&  mAction == KAEvent::FILE  &&  !mErrorMsgs.count())
            KAlarm::writeConfigWindowSize("FileMessage", re->size());
        MainWindowBase::resizeEvent(re);
    }
}

/******************************************************************************
* Called when a close event is received.
* Only quits the application if there is no system tray icon displayed.
*/
void MessageWin::closeEvent(QCloseEvent* ce)
{
    // Don't prompt or delete the alarm from the display calendar if the session is closing
    if (!mErrorWindow  &&  !theApp()->sessionClosingDown())
    {
        if (mConfirmAck  &&  !mNoCloseConfirm)
        {
            // Ask for confirmation of acknowledgement. Use warningYesNo() because its default is No.
            if (KAMessageBox::warningYesNo(this, i18nc("@info", "Do you really want to acknowledge this alarm?"),
                                                 i18nc("@action:button", "Acknowledge Alarm"), KGuiItem(i18nc("@action:button", "Acknowledge")), KStandardGuiItem::cancel())
                != KMessageBox::Yes)
            {
                ce->ignore();
                return;
            }
        }
        if (!mEventID.isNull())
        {
            // Delete from the display calendar
            KAlarm::deleteDisplayEvent(CalEvent::uid(mEventID, CalEvent::DISPLAYING));
        }
    }
    MainWindowBase::closeEvent(ce);
}

/******************************************************************************
* Called when the OK button is clicked.
*/
void MessageWin::slotOk()
{
    if (mDontShowAgainCheck  &&  mDontShowAgainCheck->isChecked())
        KAlarm::setDontShowErrors(mEventID, mDontShowAgain);
    close();
}

#ifdef KMAIL_SUPPORTED
/******************************************************************************
* Called when the KMail button is clicked.
* Tells KMail to display the email message displayed in this message window.
*/
void MessageWin::slotShowKMailMessage()
{
    kDebug();
    if (!mKMailSerialNumber)
        return;
    QString err = KAlarm::runKMail(false);
    if (!err.isNull())
    {
        KAMessageBox::sorry(this, err);
        return;
    }
    org::kde::kmail::kmail kmail(KMAIL_DBUS_SERVICE, KMAIL_DBUS_PATH, QDBusConnection::sessionBus());
    QDBusReply<bool> reply = kmail.showMail((qulonglong)mKMailSerialNumber, QString());
    if (!reply.isValid())
        kError() << "kmail D-Bus call failed:" << reply.error().message();
    else if (!reply.value())
        KAMessageBox::sorry(this, i18nc("@info", "Unable to locate this email in <application>KMail</application>"));
}
#endif

/******************************************************************************
* Called when the Edit... button is clicked.
* Displays the alarm edit dialog.
*
* NOTE: The alarm edit dialog is made a child of the main window, not this
*       window, so that if this window closes before the dialog (e.g. on
*       auto-close), KAlarm doesn't crash. The dialog is set non-modal so that
*       the main window is unaffected, but modal mode is simulated so that
*       this window is inactive while the dialog is open.
*/
void MessageWin::slotEdit()
{
    kDebug();
    MainWindow* mainWin = MainWindow::mainMainWindow();
    mEditDlg = EditAlarmDlg::create(false, &mOriginalEvent, false, mainWin, EditAlarmDlg::RES_IGNORE);
    KWindowSystem::setMainWindow(mEditDlg, winId());
    KWindowSystem::setOnAllDesktops(mEditDlg->winId(), false);
    setButtonsReadOnly(true);
    connect(mEditDlg, SIGNAL(accepted()), SLOT(editCloseOk()));
    connect(mEditDlg, SIGNAL(rejected()), SLOT(editCloseCancel()));
    connect(mEditDlg, SIGNAL(destroyed(QObject*)), SLOT(editCloseCancel()));
    connect(KWindowSystem::self(), SIGNAL(activeWindowChanged(WId)), SLOT(activeWindowChanged(WId)));
#ifdef USE_AKONADI
    mainWin->editAlarm(mEditDlg, mOriginalEvent);
#else
    mainWin->editAlarm(mEditDlg, mOriginalEvent, mResource);
#endif
}

/******************************************************************************
* Called when OK is clicked in the alarm edit dialog invoked by the Edit button.
* Closes the window.
*/
void MessageWin::editCloseOk()
{
    mEditDlg = 0;
    mNoCloseConfirm = true;   // allow window to close without confirmation prompt
    close();
}

/******************************************************************************
* Called when Cancel is clicked in the alarm edit dialog invoked by the Edit
* button, or when the dialog is deleted.
*/
void MessageWin::editCloseCancel()
{
    mEditDlg = 0;
    setButtonsReadOnly(false);
}

/******************************************************************************
* Called when the active window has changed. If this window has become the
* active window and there is an alarm edit dialog, simulate a modal dialog by
* making the alarm edit dialog the active window instead.
*/
void MessageWin::activeWindowChanged(WId win)
{
    if (mEditDlg  &&  win == winId())
        KWindowSystem::activateWindow(mEditDlg->winId());
}

/******************************************************************************
* Set or clear the read-only state of the dialog buttons.
*/
void MessageWin::setButtonsReadOnly(bool ro)
{
    mOkButton->setReadOnly(ro, true);
    mDeferButton->setReadOnly(ro, true);
    mEditButton->setReadOnly(ro, true);
    if (mSilenceButton)
        mSilenceButton->setReadOnly(ro, true);
    if (mKMailButton)
        mKMailButton->setReadOnly(ro, true);
    mKAlarmButton->setReadOnly(ro, true);
}

/******************************************************************************
* Set up to disable the defer button when the deferral limit is reached.
*/
void MessageWin::setDeferralLimit(const KAEvent& event)
{
    mDeferLimit = event.deferralLimit().effectiveKDateTime().toLocalZone().dateTime();
    MidnightTimer::connect(this, SLOT(checkDeferralLimit()));   // check every day
    mDisableDeferral = false;
    checkDeferralLimit();
}

/******************************************************************************
* Check whether the deferral limit has been reached.
* If so, disable the Defer button.
* N.B. Ideally, just a single QTimer::singleShot() call would be made to disable
*      the defer button at the corret time. But for a 32-bit integer, the
*      milliseconds parameter overflows in about 25 days, so instead a daily
*      check is done until the day when the deferral limit is reached, followed
*      by a non-overflowing QTimer::singleShot() call.
*/
void MessageWin::checkDeferralLimit()
{
    if (!mDeferButton->isEnabled()  ||  !mDeferLimit.isValid())
        return;
    int n = KDateTime::currentLocalDate().daysTo(mDeferLimit.date());
    if (n > 0)
        return;
    MidnightTimer::disconnect(this, SLOT(checkDeferralLimit()));
    if (n == 0)
    {
        // The deferral limit will be reached today
        n = KDateTime::currentLocalTime().secsTo(mDeferLimit.time());
        if (n > 0)
        {
            QTimer::singleShot(n * 1000, this, SLOT(checkDeferralLimit()));
            return;
        }
    }
    mDeferButton->setEnabled(false);
    mDisableDeferral = true;
}

/******************************************************************************
* Called when the Defer... button is clicked.
* Displays the defer message dialog.
*/
void MessageWin::slotDefer()
{
    mDeferDlg = new DeferAlarmDlg(KDateTime::currentDateTime(Preferences::timeZone()).addSecs(60), mDateTime.isDateOnly(), false, this);
    mDeferDlg->setObjectName("DeferDlg");    // used by LikeBack
    mDeferDlg->setDeferMinutes(mDefaultDeferMinutes > 0 ? mDefaultDeferMinutes : Preferences::defaultDeferTime());
    mDeferDlg->setLimit(mEventID);
    if (!Preferences::modalMessages())
        lower();
    if (mDeferDlg->exec() == QDialog::Accepted)
    {
        DateTime dateTime  = mDeferDlg->getDateTime();
        int      delayMins = mDeferDlg->deferMinutes();
        // Fetch the up-to-date alarm from the calendar. Note that it could have
        // changed since it was displayed.
        const KAEvent* event = mEventID.isNull() ? 0 : AlarmCalendar::resources()->event(mEventID);
        if (event)
        {
            // The event still exists in the active calendar
            kDebug() << "Deferring event" << mEventID;
            KAEvent newev(*event);
            newev.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
            newev.setDeferDefaultMinutes(delayMins);
            KAlarm::updateEvent(newev, mDeferDlg, true);
            if (newev.deferred())
                mNoPostAction = true;
        }
        else
        {
            // Try to retrieve the event from the displaying or archive calendars
#ifdef USE_AKONADI
            Akonadi::Collection collection;
#else
            AlarmResource* resource = 0;
#endif
            KAEvent event;
            bool showEdit, showDefer;
#ifdef USE_AKONADI
            if (!retrieveEvent(event, collection, showEdit, showDefer))
#else
            if (!retrieveEvent(event, resource, showEdit, showDefer))
#endif
            {
                // The event doesn't exist any more !?!, so recurrence data,
                // flags, and more, have been lost.
                KAMessageBox::error(this, i18nc("@info", "<para>Cannot defer alarm:</para><para>Alarm not found.</para>"));
                raise();
                delete mDeferDlg;
                mDeferDlg = 0;
                mDeferButton->setEnabled(false);
                mEditButton->setEnabled(false);
                return;
            }
            kDebug() << "Deferring retrieved event" << mEventID;
            event.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
            event.setDeferDefaultMinutes(delayMins);
            event.setCommandError(mCommandError);
            // Add the event back into the calendar file, retaining its ID
            // and not updating KOrganizer.
#ifdef USE_AKONADI
            KAlarm::addEvent(event, &collection, mDeferDlg, KAlarm::USE_EVENT_ID);
#else
            KAlarm::addEvent(event, resource, mDeferDlg, KAlarm::USE_EVENT_ID);
#endif
            if (event.deferred())
                mNoPostAction = true;
            // Finally delete it from the archived calendar now that it has
            // been reactivated.
            event.setCategory(CalEvent::ARCHIVED);
            KAlarm::deleteEvent(event, false);
        }
        if (theApp()->wantShowInSystemTray())
        {
            // Alarms are to be displayed only if the system tray icon is running,
            // so start it if necessary so that the deferred alarm will be shown.
            theApp()->displayTrayIcon(true);
        }
        mNoCloseConfirm = true;   // allow window to close without confirmation prompt
        close();
    }
    else
        raise();
    delete mDeferDlg;
    mDeferDlg = 0;
}

/******************************************************************************
* Called when the KAlarm icon button in the message window is clicked.
* Displays the main window, with the appropriate alarm selected.
*/
void MessageWin::displayMainWindow()
{
#ifdef USE_AKONADI
    KAlarm::displayMainWindowSelected(mEventItemId);
#else
    KAlarm::displayMainWindowSelected(mEventID);
#endif
}

/******************************************************************************
* Check whether the specified error message is already displayed for this
* alarm, and note that it will now be displayed.
* Reply = true if message is already displayed.
*/
bool MessageWin::haveErrorMessage(unsigned msg) const
{
    if (!mErrorMessages.contains(mEventID))
        mErrorMessages.insert(mEventID, 0);
    bool result = (mErrorMessages[mEventID] & msg);
    mErrorMessages[mEventID] |= msg;
    return result;
}

void MessageWin::clearErrorMessage(unsigned msg) const
{
    if (mErrorMessages.contains(mEventID))
    {
        if (mErrorMessages[mEventID] == msg)
            mErrorMessages.remove(mEventID);
        else
            mErrorMessages[mEventID] &= ~msg;
    }
}


/******************************************************************************
* Check whether the message window should be modal, i.e. with title bar etc.
* Normally this follows the Preferences setting, but if there is a full screen
* window displayed, on X11 the message window has to bypass the window manager
* in order to display on top of it (which has the side effect that it will have
* no window decoration).
*
* Also find the usable area of the desktop (excluding panel etc.), on the
* appropriate screen if there are multiple screens.
*/
bool MessageWin::getWorkAreaAndModal()
{
    mScreenNumber = -1;
    bool modal = Preferences::modalMessages();
#ifdef Q_WS_X11
    QDesktopWidget* desktop = qApp->desktop();
    int numScreens = desktop->numScreens();
    if (numScreens > 1)
    {
        // There are multiple screens.
        // Check for any full screen windows, even if they are not the active
        // window, and try not to show the alarm message their screens.
        mScreenNumber = desktop->screenNumber(MainWindow::mainMainWindow());  // default = KAlarm's screen
        if (desktop->isVirtualDesktop())
        {
            // The screens form a single virtual desktop.
            // Xinerama, for example, uses this scheme.
            QVector<FullScreenType> screenTypes(numScreens);
            QVector<QRect> screenRects(numScreens);
            for (int s = 0;  s < numScreens;  ++s)
                screenRects[s] = desktop->screenGeometry(s);
            FullScreenType full = findFullScreenWindows(screenRects, screenTypes);
            if (full == NoFullScreen  ||  screenTypes[mScreenNumber] == NoFullScreen)
                return modal;
            for (int s = 0;  s < numScreens;  ++s)
            {
                if (screenTypes[s] == NoFullScreen)

                {
                    // There is no full screen window on this screen
                    mScreenNumber = s;
                    return modal;
                }
            }
            // All screens contain a full screen window: use one without
            // an active full screen window.
            for (int s = 0;  s < numScreens;  ++s)
            {
                if (screenTypes[s] == FullScreen)
                {
                    mScreenNumber = s;
                    return modal;
                }
            }
        }
        else
        {
            // The screens are completely separate from each other.
            int inactiveScreen = -1;
            FullScreenType full = haveFullScreenWindow(mScreenNumber);
kDebug()<<"full="<<full<<", screen="<<mScreenNumber;
            if (full == NoFullScreen)
                return modal;   // KAlarm's screen doesn't contain a full screen window
            if (full == FullScreen)
                inactiveScreen = mScreenNumber;
            for (int s = 0;  s < numScreens;  ++s)
            {
                if (s != mScreenNumber)
                {
                    full = haveFullScreenWindow(s);
                    if (full == NoFullScreen)
                    {
                        // There is no full screen window on this screen
                        mScreenNumber = s;
                        return modal;
                    }
                    if (full == FullScreen  &&  inactiveScreen < 0)
                        inactiveScreen = s;
                }
            }
            if (inactiveScreen >= 0)
            {
                // All screens contain a full screen window: use one without
                // an active full screen window.
                mScreenNumber = inactiveScreen;
                return modal;
            }
        }
        return false;  // can't logically get here, since there can only be one active window...
    }
#endif
    if (modal)
    {
        WId activeId = KWindowSystem::activeWindow();
        KWindowInfo wi = KWindowSystem::windowInfo(activeId, NET::WMState);
        if (wi.valid()  &&  wi.hasState(NET::FullScreen))
            return false;    // the active window is full screen.
    }
    return modal;
}

#ifdef Q_WS_X11
/******************************************************************************
* In a multi-screen setup (not a single virtual desktop), find whether the
* specified screen has a full screen window on it.
*/
FullScreenType haveFullScreenWindow(int screen)
{
    FullScreenType type = NoFullScreen;
    Display* display = QX11Info::display();
    NETRootInfo rootInfo(display, NET::ClientList | NET::ActiveWindow, screen);
    Window rootWindow     = rootInfo.rootWindow();
    Window activeWindow   = rootInfo.activeWindow();
    const Window* windows = rootInfo.clientList();
    int windowCount       = rootInfo.clientListCount();
kDebug()<<"Screen"<<screen<<": Window count="<<windowCount<<", active="<<activeWindow<<", geom="<<qApp->desktop()->screenGeometry(screen);
NETRect geom;
NETRect frame;
    for (int w = 0;  w < windowCount;  ++w)
    {
        NETWinInfo winInfo(display, windows[w], rootWindow, NET::WMState|NET::WMGeometry);
winInfo.kdeGeometry(frame, geom);
QRect fr(frame.pos.x, frame.pos.y, frame.size.width, frame.size.height);
QRect gm(geom.pos.x, geom.pos.y, geom.size.width, geom.size.height);
        if (winInfo.state() & NET::FullScreen)
        {
kDebug()<<"Found FULL SCREEN: "<<windows[w]<<", geom="<<gm<<", frame="<<fr;
            type = FullScreen;
            if (windows[w] == activeWindow)
                return FullScreenActive;
        }
//else { kDebug()<<"Found normal: "<<windows[w]<<", geom="<<gm<<", frame="<<fr; }
    }
    return type;
}

/******************************************************************************
* In a multi-screen setup (single virtual desktop, e.g. Xinerama), find which
* screens have full screen windows on them.
*/
FullScreenType findFullScreenWindows(const QVector<QRect>& screenRects, QVector<FullScreenType>& screenTypes)
{
    FullScreenType result = NoFullScreen;
    screenTypes.fill(NoFullScreen);
    Display* display = QX11Info::display();
    NETRootInfo rootInfo(display, NET::ClientList | NET::ActiveWindow, 0);
    Window rootWindow     = rootInfo.rootWindow();
    Window activeWindow   = rootInfo.activeWindow();
    const Window* windows = rootInfo.clientList();
    int windowCount       = rootInfo.clientListCount();
kDebug()<<"Virtual desktops: Window count="<<windowCount<<", active="<<activeWindow<<", geom="<<qApp->desktop()->screenGeometry(0);
    NETRect netgeom;
    NETRect netframe;
    for (int w = 0;  w < windowCount;  ++w)
    {
        NETWinInfo winInfo(display, windows[w], rootWindow, NET::WMState | NET::WMGeometry);
        if (winInfo.state() & NET::FullScreen)
        {
            // Found a full screen window - find which screen it's on
            bool active = (windows[w] == activeWindow);
            winInfo.kdeGeometry(netframe, netgeom);
            QRect winRect(netgeom.pos.x, netgeom.pos.y, netgeom.size.width, netgeom.size.height);
kDebug()<<"Found FULL SCREEN: "<<windows[w]<<", geom="<<winRect;
            for (int s = 0, count = screenRects.count();  s < count;  ++s)
            {
                if (screenRects[s].contains(winRect))
                {
kDebug()<<"FULL SCREEN on screen"<<s<<", active="<<active;
                    if (active)
                        screenTypes[s] = result = FullScreenActive;
                    else
                    {
                        if (screenTypes[s] == NoFullScreen)
                            screenTypes[s] = FullScreen;
                        if (result == NoFullScreen)
                            result = FullScreen;
                    }
                    break;
                }
            }
        }
    }
    return result;
}
#endif

// vim: et sw=4:
