/*
 *  messagewindow.cpp  -  displays an alarm message in a window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "messagewindow.h"
#include "messagedisplayhelper.h"
#include "config-kalarm.h"

#include "deferdlg.h"
#include "editdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "resourcescalendar.h"
#include "lib/config.h"
#include "lib/desktop.h"
#include "lib/file.h"
#include "lib/messagebox.h"
#include "lib/pushbutton.h"
#include "lib/synchtimer.h"
#include "kalarm_debug.h"

#include <AkonadiCore/ItemFetchJob>
#include <AkonadiCore/ItemFetchScope>

#include <KAboutData>
#include <KStandardGuiItem>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KTextEdit>
#include <KWindowSystem>
#include <KSqueezedTextLabel>
#if KDEPIM_HAVE_X11
#include <KWindowInfo>
#include <netwm.h>
#include <qx11info_x11.h>
#include <QScreen>
#endif

#include <QTextBrowser>
#include <QScrollBar>
#include <QCheckBox>
#include <QLabel>
#include <QPalette>
#include <QTimer>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QStyle>

#include "kmailinterface.h"

using namespace KAlarmCal;

namespace
{

// Display type string for MessageWindow class.
const QString DISPLAY_TYPE = QStringLiteral("window");

#if KDEPIM_HAVE_X11
enum FullScreenType { NoFullScreen = 0, FullScreen = 1, FullScreenActive = 2 };
FullScreenType haveFullScreenWindow(int screen);
FullScreenType findFullScreenWindows(const QVector<QRect>& screenRects, QVector<FullScreenType>& screenTypes);
#endif

const QLatin1String KMAIL_DBUS_SERVICE("org.kde.kmail");
const QLatin1String KMAIL_DBUS_PATH("/KMail");

// The delay for enabling message window buttons if a zero delay is
// configured, i.e. the windows are placed far from the cursor.
const int proximityButtonDelay = 1000;    // (milliseconds)
const int proximityMultiple = 10;         // multiple of button height distance from cursor for proximity

// Basic flags for the window
const Qt::WindowFlags     WFLAGS      = Qt::WindowStaysOnTopHint;
const Qt::WindowFlags     WFLAGS2     = Qt::WindowContextHelpButtonHint;
const Qt::WidgetAttribute WidgetFlags = Qt::WA_DeleteOnClose;

} // namespace


// A text label widget which can be scrolled and copied with the mouse.
class MessageText : public KTextEdit
{
public:
    MessageText(QWidget* parent = nullptr)
        : KTextEdit(parent)
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
    QSize sizeHint() const override
    {
        const QSizeF docsize = document()->size();
        return QSize(static_cast<int>(docsize.width() + 0.99) + verticalScrollBar()->width(),
                     static_cast<int>(docsize.height() + 0.99) + horizontalScrollBar()->height());
    }
};


QVector<MessageWindow*> MessageWindow::mWindowList;
bool                    MessageWindow::mRedisplayed = false;

/******************************************************************************
* Construct the message window for the specified alarm.
* Other alarms in the supplied event may have been updated by the caller, so
* the whole event needs to be stored for updating the calendar file when it is
* displayed.
*/
MessageWindow::MessageWindow(const KAEvent* event, const KAAlarm& alarm, int flags)
    : MainWindowBase(nullptr, static_cast<Qt::WindowFlags>(WFLAGS | WFLAGS2 | ((flags & ALWAYS_HIDE) || getWorkAreaAndModal() ? Qt::WindowType(0) : Qt::X11BypassWindowManagerHint)))
    , MessageDisplay(DISPLAY_TYPE, event, alarm, flags)
    , mRestoreHeight(0)
{
    qCDebug(KALARM_LOG) << "MessageWindow:" << (void*)this << "event" << mEventId();
    setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags));
    setWindowModality(Qt::WindowModal);
    setObjectName(QStringLiteral("MessageWindow"));    // used by LikeBack
    if (!(flags & (NO_INIT_VIEW | ALWAYS_HIDE)))
        setUpDisplay();

    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageWindow::textsChanged);

    // Set to save settings automatically, but don't save window size.
    // File alarm window size is saved elsewhere.
    setAutoSaveSettings(QStringLiteral("MessageWindow"), false);
    mWindowList.append(this);
    if (mAlwaysHidden())
    {
        hide();
        displayComplete();    // play audio, etc.
    }
}

/******************************************************************************
* Construct the message window for a specified error message.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
MessageWindow::MessageWindow(const KAEvent* event, const DateTime& alarmDateTime,
                       const QStringList& errmsgs, const QString& dontShowAgain)
    : MainWindowBase(nullptr, WFLAGS | WFLAGS2)
    , MessageDisplay(DISPLAY_TYPE, event, alarmDateTime, errmsgs, dontShowAgain)
    , mRestoreHeight(0)
{
    qCDebug(KALARM_LOG) << "MessageWindow: errmsg";
    setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags));
    setWindowModality(Qt::WindowModal);
    setObjectName(QStringLiteral("ErrorWin"));    // used by LikeBack
    getWorkAreaAndModal();
    setUpDisplay();

    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageWindow::textsChanged);

    mWindowList.append(this);
}

/******************************************************************************
* Construct the message window for restoration by session management.
* The window is initialised by readProperties().
*/
MessageWindow::MessageWindow()
    : MainWindowBase(nullptr, WFLAGS)
    , MessageDisplay(DISPLAY_TYPE)
{
    qCDebug(KALARM_LOG) << "MessageWindow:" << (void*)this << "restore";
    setAttribute(WidgetFlags);
    setWindowModality(Qt::WindowModal);
    setObjectName(QStringLiteral("RestoredMsgWin"));    // used by LikeBack
    getWorkAreaAndModal();

    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageWindow::textsChanged);
#warning When restoring an error message, the window is blank

    mWindowList.append(this);
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageWindow::~MessageWindow()
{
    qCDebug(KALARM_LOG) << "~MessageWindow" << (void*)this << mEventId();
    mWindowList.removeAll(this);
}

/******************************************************************************
* Display an error message window.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
void MessageWindow::showError(const KAEvent& event, const DateTime& alarmDateTime,
                              const QStringList& errmsgs, const QString& dontShowAgain)
{
    if (!dontShowAgain.isEmpty()
    &&  KAlarm::dontShowErrors(EventId(event), dontShowAgain))
        return;

    if (MessageDisplayHelper::shouldShowError(event, errmsgs, dontShowAgain))
        (new MessageWindow(&event, alarmDateTime, errmsgs, dontShowAgain))->show();
}

/******************************************************************************
* Construct the message window.
*/
void MessageWindow::setUpDisplay()
{
    mHelper->initTexts();
    MessageDisplayHelper::DisplayTexts texts = mHelper->texts();

    const bool reminder = (!mErrorWindow()  &&  (mAlarmType() & KAAlarm::REMINDER_ALARM));
    const int leading = fontMetrics().leading();
    setCaption(texts.title);
    QWidget* topWidget = new QWidget(this);
    setCentralWidget(topWidget);
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    const int dcmLeft   = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    const int dcmTop    = style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    const int dcmRight  = style()->pixelMetric(QStyle::PM_LayoutRightMargin);
    const int dcmBottom = style()->pixelMetric(QStyle::PM_LayoutBottomMargin);

    QPalette labelPalette = palette();
    labelPalette.setColor(backgroundRole(), labelPalette.color(QPalette::Window));

    // Show the alarm date/time, together with a reminder text where appropriate.
    // Alarm date/time: display time zone if not local time zone.
    mTimeLabel = new QLabel(topWidget);
    mTimeLabel->setText(texts.time);
    mTimeLabel->setFrameStyle(QFrame::StyledPanel);
    mTimeLabel->setPalette(labelPalette);
    mTimeLabel->setAutoFillBackground(true);
    mTimeLabel->setAlignment(Qt::AlignHCenter);
    topLayout->addWidget(mTimeLabel, 0, Qt::AlignHCenter);
    mTimeLabel->setWhatsThis(i18nc("@info:whatsthis", "The scheduled date/time for the message (as opposed to the actual time of display)."));
    if (texts.time.isEmpty())
        mTimeLabel->hide();

    if (!mErrorWindow())
    {
        // It's a normal alarm message window
        switch (mAction())
        {
            case KAEvent::FILE:
            {
                // Display the file name
                KSqueezedTextLabel* label = new KSqueezedTextLabel(texts.fileName, topWidget);
                label->setFrameStyle(QFrame::StyledPanel);
                label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
                label->setPalette(labelPalette);
                label->setAutoFillBackground(true);
                label->setWhatsThis(i18nc("@info:whatsthis", "The file whose contents are displayed below"));
                topLayout->addWidget(label, 0, Qt::AlignHCenter);

                if (mErrorMsgs().isEmpty())
                {
                    // Display contents of file
                    QTextBrowser* view = new QTextBrowser(topWidget);
                    view->setFrameStyle(QFrame::NoFrame);
                    view->setWordWrapMode(QTextOption::NoWrap);
                    QPalette pal = view->viewport()->palette();
                    pal.setColor(view->viewport()->backgroundRole(), mBgColour());
                    view->viewport()->setPalette(pal);
                    view->setTextColor(mFgColour());
                    view->setCurrentFont(mFont());

                    switch (texts.fileType)
                    {
                        case File::Image:
                        case File::TextFormatted:
                            view->setHtml(texts.message);
                            break;
                        default:
                            view->setPlainText(texts.message);
                            break;
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
                break;
            }
            case KAEvent::MESSAGE:
            {
                // Message label
                // Using MessageText instead of QLabel allows scrolling and mouse copying
                MessageText* text = new MessageText(topWidget);
                text->setAutoFillBackground(true);
                text->setBackgroundColour(mBgColour());
                text->setTextColor(mFgColour());
                text->setCurrentFont(mFont());
                text->insertPlainText(texts.message);
                const int lineSpacing = text->fontMetrics().lineSpacing();
                const QSize s = text->sizeHint();
                const int h = s.height();
                text->setMaximumHeight(h + text->scrollBarHeight());
                text->setMinimumHeight(qMin(h, lineSpacing*4));
                text->setMaximumWidth(s.width() + text->scrollBarWidth());
                text->setWhatsThis(i18nc("@info:whatsthis", "The alarm message"));
                const int vspace = lineSpacing/2;
                const int hspace = lineSpacing - (dcmLeft + dcmRight)/2;
                topLayout->addSpacing(vspace);
                topLayout->addStretch();
                // Don't include any horizontal margins if message is 2/3 screen width
                if (text->sizeHint().width() >= Desktop::workArea(mScreenNumber).width()*2/3)
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
                mCommandText->setBackgroundColour(mBgColour());
                mCommandText->setTextColor(mFgColour());
                mCommandText->setCurrentFont(mFont());
                topLayout->addWidget(mCommandText);
                mCommandText->setWhatsThis(i18nc("@info:whatsthis", "The output of the alarm's command"));
                mCommandText->setPlainText(texts.message);
                break;
            }
            case KAEvent::EMAIL:
            default:
                break;
        }

        if (!texts.remainingTime.isEmpty())
        {
            // Advance reminder: show remaining time until the actual alarm
            mRemainingText = new QLabel(topWidget);
            mRemainingText->setFrameStyle(QFrame::Box | QFrame::Raised);
            mRemainingText->setContentsMargins(leading, leading, leading, leading);
            mRemainingText->setPalette(labelPalette);
            mRemainingText->setAutoFillBackground(true);
            mRemainingText->setText(texts.remainingTime);
            topLayout->addWidget(mRemainingText, 0, Qt::AlignHCenter);
            topLayout->addSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
            topLayout->addStretch();
        }
    }
    else
    {
        // It's an error message
        switch (mAction())
        {
            case KAEvent::EMAIL:
            {
                // Display the email addresses and subject.
                QFrame* frame = new QFrame(topWidget);
                frame->setFrameStyle(QFrame::Box | QFrame::Raised);
                frame->setWhatsThis(i18nc("@info:whatsthis", "The email to send"));
                topLayout->addWidget(frame, 0, Qt::AlignHCenter);
                QGridLayout* grid = new QGridLayout(frame);

                QLabel* label = new QLabel(texts.errorEmail[0], frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 0, 0, Qt::AlignLeft);
                label = new QLabel(texts.errorEmail[1], frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 0, 1, Qt::AlignLeft);

                label = new QLabel(texts.errorEmail[2], frame);
                label->setFixedSize(label->sizeHint());
                grid->addWidget(label, 1, 0, Qt::AlignLeft);
                label = new QLabel(texts.errorEmail[3], frame);
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

    if (mErrorMsgs().isEmpty())
    {
        topWidget->setAutoFillBackground(true);
        QPalette palette = topWidget->palette();
        palette.setColor(topWidget->backgroundRole(), mBgColour());
        topWidget->setPalette(palette);
    }
    else
    {
        QHBoxLayout* layout = new QHBoxLayout();
        layout->setContentsMargins(2 * dcmLeft, 2 * dcmTop, 2 * dcmRight, 2 * dcmBottom);
        layout->addStretch();
        topLayout->addLayout(layout);
        QLabel* label = new QLabel(topWidget);
        label->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-error")).pixmap(style()->pixelMetric(QStyle::PM_MessageBoxIconSize)));
        label->setFixedSize(label->sizeHint());
        layout->addWidget(label, 0, Qt::AlignRight);
        QVBoxLayout* vlayout = new QVBoxLayout();
        layout->addLayout(vlayout);
        for (QStringList::ConstIterator it = mErrorMsgs().constBegin();  it != mErrorMsgs().constEnd();  ++it)
        {
            label = new QLabel(*it, topWidget);
            label->setFixedSize(label->sizeHint());
            vlayout->addWidget(label, 0, Qt::AlignLeft);
        }
        layout->addStretch();
        if (!mDontShowAgain().isEmpty())
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
    connect(mOkButton, &QAbstractButton::clicked, this, &MessageWindow::slotOk);
    grid->addWidget(mOkButton, 0, gridIndex++, Qt::AlignHCenter);
    mOkButton->setWhatsThis(i18nc("@info:whatsthis", "Acknowledge the alarm"));

    if (mShowEdit())
    {
        // Edit button
        mEditButton = new PushButton(i18nc("@action:button", "&Edit..."), topWidget);
        mEditButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
        mEditButton->setFixedSize(mEditButton->sizeHint());
        connect(mEditButton, &QAbstractButton::clicked, this, &MessageWindow::slotEdit);
        grid->addWidget(mEditButton, 0, gridIndex++, Qt::AlignHCenter);
        mEditButton->setToolTip(i18nc("@info:tooltip", "Edit the alarm"));
        mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the alarm."));
    }

    // Defer button
    mDeferButton = new PushButton(i18nc("@action:button", "&Defer..."), topWidget);
    mDeferButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
    mDeferButton->setFixedSize(mDeferButton->sizeHint());
    connect(mDeferButton, &QAbstractButton::clicked, this, &MessageWindow::slotDefer);
    grid->addWidget(mDeferButton, 0, gridIndex++, Qt::AlignHCenter);
    mDeferButton->setToolTip(i18nc("@info:tooltip", "Defer the alarm until later"));
    mDeferButton->setWhatsThis(xi18nc("@info:whatsthis", "<para>Defer the alarm until later.</para>"
                                    "<para>You will be prompted to specify when the alarm should be redisplayed.</para>"));

    if (mNoDefer())
        mDeferButton->hide();
    else
        setDeferralLimit(mEvent());    // ensure that button is disabled when alarm can't be deferred any more

    if (!mAudioFile().isEmpty()  &&  (mVolume() || mFadeVolume() > 0))
    {
        // Silence button to stop sound repetition
        mSilenceButton = new PushButton(topWidget);
        mSilenceButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
        grid->addWidget(mSilenceButton, 0, gridIndex++, Qt::AlignHCenter);
        mSilenceButton->setToolTip(i18nc("@info:tooltip", "Stop sound"));
        mSilenceButton->setWhatsThis(i18nc("@info:whatsthis", "Stop playing the sound"));
        // To avoid getting in a mess, disable the button until sound playing has been set up
        mSilenceButton->setEnabled(false);

        mHelper->setSilenceButton(mSilenceButton);
    }

    if (mAkonadiItemId() >= 0)
    {
        // KMail button
        mKMailButton = new PushButton(topWidget);
        mKMailButton->setIcon(QIcon::fromTheme(QStringLiteral("internet-mail")));
        connect(mKMailButton, &QAbstractButton::clicked, this, &MessageWindow::slotShowKMailMessage);
        grid->addWidget(mKMailButton, 0, gridIndex++, Qt::AlignHCenter);
        mKMailButton->setToolTip(xi18nc("@info:tooltip Locate this email in KMail", "Locate in <application>KMail</application>"));
        mKMailButton->setWhatsThis(xi18nc("@info:whatsthis", "Locate and highlight this email in <application>KMail</application>"));
    }

    // KAlarm button
    mKAlarmButton = new PushButton(topWidget);
    mKAlarmButton->setIcon(QIcon::fromTheme(KAboutData::applicationData().componentName()));
    connect(mKAlarmButton, &QAbstractButton::clicked, this, &MessageWindow::displayMainWindow);
    grid->addWidget(mKAlarmButton, 0, gridIndex++, Qt::AlignHCenter);
    mKAlarmButton->setToolTip(xi18nc("@info:tooltip", "Activate <application>KAlarm</application>"));
    mKAlarmButton->setWhatsThis(xi18nc("@info:whatsthis", "Activate <application>KAlarm</application>"));

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
    setMinimumSize(QSize(grid->sizeHint().width() + dcmLeft + dcmRight,
                         sizeHint().height()));
    const bool modal = !(windowFlags() & Qt::X11BypassWindowManagerHint);
    NET::States wstate = NET::Sticky | NET::KeepAbove;
    if (modal)
        wstate |= NET::Modal;
    WId winid = winId();
    KWindowSystem::setState(winid, wstate);
    KWindowSystem::setOnAllDesktops(winid, true);

    mInitialised = true;   // the window's widgets have been created
}

/******************************************************************************
* Return the number of message windows, optionally excluding always-hidden ones.
*/
int MessageWindow::windowCount(bool excludeAlwaysHidden)
{
    int count = mWindowList.count();
    if (excludeAlwaysHidden)
    {
        for (MessageWindow* win : qAsConst(mWindowList))
        {
            if (win->mAlwaysHidden())
                --count;
        }
    }
    return count;
}

/******************************************************************************
* Returns the widget to act as parent for error messages, etc.
*/
QWidget* MessageWindow::displayParent()
{
    return this;
}

void MessageWindow::closeDisplay()
{
    close();
}

void MessageWindow::showDisplay()
{
    show();
}

void MessageWindow::raiseDisplay()
{
    raise();
}

void MessageWindow::hideDisplay()
{
    hide();
}

/******************************************************************************
* Raise the alarm window, re-output any required audio notification, and
* reschedule the alarm in the calendar file.
*/
void MessageWindow::repeat(const KAAlarm& alarm)
{
    if (!mInitialised)
        return;
    if (mDeferDlg)
    {
        // Cancel any deferral dialog so that the user notices something's going on,
        // and also because the deferral time limit will have changed.
        delete mDeferDlg;
        mDeferDlg = nullptr;
    }
    if (mEventId().isEmpty())
        return;
    KAEvent event = ResourcesCalendar::event(mEventId());
    if (event.isValid())
    {
        mAlarmType() = alarm.type();    // store new alarm type for use if it is later deferred
        if (mAlwaysHidden())
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
                setDeferralLimit(event);    // ensure that button is disabled when alarm can't be deferred any more
            }
        }
        if (mHelper->alarmShowing(event))
            ResourcesCalendar::updateEvent(event);
    }
}

bool MessageWindow::hasDefer() const
{
    return mDeferButton && mDeferButton->isVisible();
}

/******************************************************************************
* Show the Defer button when it was previously hidden.
*/
void MessageWindow::showDefer()
{
    if (mDeferButton)
    {
        mNoDefer() = false;
        mDeferButton->show();
        setDeferralLimit(mEvent());    // ensure that button is disabled when alarm can't be deferred any more
        resize(sizeHint());
    }
}

/******************************************************************************
* Convert a reminder window into a normal alarm window.
*/
void MessageWindow::cancelReminder(const KAEvent& event, const KAAlarm& alarm)
{
    if (mHelper->cancelReminder(event, alarm))
    {
        const MessageDisplayHelper::DisplayTexts& texts = mHelper->texts();
        setCaption(texts.title);
        mTimeLabel->setText(texts.time);
        if (mRemainingText)
            mRemainingText->hide();
        setMinimumHeight(0);
        centralWidget()->layout()->activate();
        setMinimumHeight(sizeHint().height());
        resize(sizeHint());
    }
}

/******************************************************************************
* Show the alarm's trigger time.
* This is assumed to have previously been hidden.
*/
void MessageWindow::showDateTime(const KAEvent& event, const KAAlarm& alarm)
{
    if (mTimeLabel  &&  mHelper->updateDateTime(event, alarm))
    {
        mTimeLabel->setText(mHelper->texts().time);
        mTimeLabel->show();
    }
}

/******************************************************************************
* Called when the texts to display have changed.
*/
void MessageWindow::textsChanged(MessageDisplayHelper::DisplayTexts::TextIds ids, const QString& change)
{
    const MessageDisplayHelper::DisplayTexts& texts = mHelper->texts();

    if (ids & MessageDisplayHelper::DisplayTexts::Title)
        setCaption(texts.title);

    if (ids & MessageDisplayHelper::DisplayTexts::Time)
        mTimeLabel->setText(texts.time);

    if (ids & MessageDisplayHelper::DisplayTexts::RemainingTime)
    {
        if (mRemainingText)
        {
            if (texts.remainingTime.isEmpty())
                mRemainingText->hide();
            else
                mRemainingText->setText(texts.remainingTime);
        }
    }

    if (ids & MessageDisplayHelper::DisplayTexts::MessageAppend)
    {
        // More output is available from the command which is providing the text
        // for this window. Add the output and resize the window to show it.
        mCommandText->insertPlainText(change);
        resize(sizeHint());
    }
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageWindow::saveProperties(KConfigGroup& config)
{
    if (mShown  &&  mHelper->saveProperties(config))
        config.writeEntry("Height", height());
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
*/
void MessageWindow::readProperties(const KConfigGroup& config)
{
    mRestoreHeight = config.readEntry("Height", 0);

    if (mHelper->readProperties(config))
    {
        // The retrieved alarm was shown by this class, and we need to initialise
        // its display.
        setUpDisplay();
    }
}

/******************************************************************************
* Spread alarm windows over the screen so that they are all visible, or pile
* them on top of each other again.
* Reply = true if windows are now scattered, false if piled up.
*/
bool MessageWindow::spread(bool scatter)
{
    if (windowCount(true) <= 1)    // ignore always-hidden windows
        return false;

    const QRect desk = Desktop::workArea();   // get the usable area of the desktop
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
            for (MessageWindow* w : qAsConst(mWindowList))
            {
                if ((!errmsgs && w->mErrorWindow())
                ||  (errmsgs && !w->mErrorWindow()))
                    continue;
                const QSize sz = w->frameGeometry().size();
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
        for (MessageWindow* w : qAsConst(mWindowList))
            w->move(desk.topLeft());
    }
    return scatter;
}

/******************************************************************************
* Check whether message windows are all piled up, or are spread out.
* Reply = true if windows are currently spread, false if piled up.
*/
bool MessageWindow::isSpread(const QPoint& topLeft)
{
    for (MessageWindow* w : qAsConst(mWindowList))
    {
        if (w->pos() != topLeft)
            return true;
    }
    return false;
}

/******************************************************************************
* Display the window.
* If windows are being positioned away from the mouse cursor, it is initially
* positioned at the top left to slightly reduce the number of times the
* windows need to be moved in showEvent().
*/
void MessageWindow::show()
{
    if (mHelper->activateAutoClose())
    {
        if (Preferences::messageButtonDelay() == 0)
            move(0, 0);
        MainWindowBase::show();
    }
}

/******************************************************************************
* Returns the window's recommended size exclusive of its frame.
*/
QSize MessageWindow::sizeHint() const
{
    QSize desired;
    switch (mAction())
    {
        case KAEvent::MESSAGE:
            desired = MainWindowBase::sizeHint();
            break;
        case KAEvent::COMMAND:
            if (mShown)
            {
                // For command output, expand the window to accommodate the text
                const QSize texthint = mCommandText->sizeHint();
                int w = texthint.width() + style()->pixelMetric(QStyle::PM_LayoutLeftMargin)
                                         + style()->pixelMetric(QStyle::PM_LayoutRightMargin);
                if (w < width())
                    w = width();
                const int ypadding = height() - mCommandText->height();
                desired = QSize(w, texthint.height() + ypadding);
                break;
            }
            // fall through to default
            Q_FALLTHROUGH();
        default:
            return MainWindowBase::sizeHint();
    }

    // Limit the size to fit inside the working area of the desktop
    const QSize desktop = Desktop::workArea(mScreenNumber).size();
    const QSize frameThickness = frameGeometry().size() - geometry().size();  // title bar & window frame
    return desired.boundedTo(desktop - frameThickness);
}

/******************************************************************************
* Called when the window is shown.
* The first time, output any required audio notification, and reschedule or
* delete the event from the calendar file.
*/
void MessageWindow::showEvent(QShowEvent* se)
{
    MainWindowBase::showEvent(se);
    if (mShown  ||  !mInitialised)
        return;
    if (mErrorWindow()  ||  mAlarmType() == KAAlarm::INVALID_ALARM)
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
        if (mAction() == KAEvent::FILE  &&  !mErrorMsgs().count())
            Config::readWindowSize("FileMessage", s);
        resize(s);

        const QRect desk = Desktop::workArea(mScreenNumber);
        const QRect frame = frameGeometry();

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
            const QPoint cursor = QCursor::pos();
            const QRect rect  = geometry();
            // Find the offsets from the outside of the frame to the edges of the OK button
            const QRect button(mOkButton->mapToParent(QPoint(0, 0)), mOkButton->mapToParent(mOkButton->rect().bottomRight()));
            const int buttonLeft   = button.left() + rect.left() - frame.left();
            const int buttonRight  = width() - button.right() + frame.right() - rect.right();
            const int buttonTop    = button.top() + rect.top() - frame.top();
            const int buttonBottom = height() - button.bottom() + frame.bottom() - rect.bottom();

            const int centrex = (desk.width() + buttonLeft - buttonRight) / 2;
            const int centrey = (desk.height() + buttonTop - buttonBottom) / 2;
            const int x = (cursor.x() < centrex) ? desk.right() - frame.width() : desk.left();
            const int y = (cursor.y() < centrey) ? desk.bottom() - frame.height() : desk.top();

            // Find the enclosing rectangle for the new button positions
            // and check if the cursor is too near
            QRect buttons = mOkButton->geometry().united(mKAlarmButton->geometry());
            buttons.translate(rect.left() + x - frame.left(), rect.top() + y - frame.top());
            const int minDistance = proximityMultiple * mOkButton->height();
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
    QTimer::singleShot(0, this, &MessageWindow::frameDrawn);

    mShown = true;
}

/******************************************************************************
* Called when the window has been moved.
*/
void MessageWindow::moveEvent(QMoveEvent* e)
{
    MainWindowBase::moveEvent(e);
    theApp()->setSpreadWindowsState(isSpread(Desktop::workArea(mScreenNumber).topLeft()));
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
void MessageWindow::frameDrawn()
{
    if (!mErrorWindow()  &&  mAction() == KAEvent::MESSAGE)
    {
        const QSize s = sizeHint();
        if (width() > s.width()  ||  height() > s.height())
            resize(s);
    }
    theApp()->setSpreadWindowsState(isSpread(Desktop::workArea(mScreenNumber).topLeft()));
}

/******************************************************************************
* Called when the window has been displayed properly (in its correct position),
* to play sounds and reschedule the event.
*/
void MessageWindow::displayComplete()
{
    mHelper->displayComplete();

    if (!mAlwaysHidden())
    {
        // Enable the window's buttons either now or after the configured delay
        if (mButtonDelay > 0)
            QTimer::singleShot(mButtonDelay, this, &MessageWindow::enableButtons);
        else
            enableButtons();
    }
}

/******************************************************************************
* Enable the window's buttons.
*/
void MessageWindow::enableButtons()
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
void MessageWindow::resizeEvent(QResizeEvent* re)
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
        if (mShown  &&  mAction() == KAEvent::FILE  &&  !mErrorMsgs().count())
            Config::writeWindowSize("FileMessage", re->size());
        MainWindowBase::resizeEvent(re);
    }
}

/******************************************************************************
* Called when a close event is received.
* Only quits the application if there is no system tray icon displayed.
*/
void MessageWindow::closeEvent(QCloseEvent* ce)
{
    if (!mHelper->closeEvent())
    {
        ce->ignore();
        return;
    }
    MainWindowBase::closeEvent(ce);
}

/******************************************************************************
* Called by MessageDisplayHelper to confirm that the alarm message should be
* acknowledged (closed).
*/
bool MessageWindow::confirmAcknowledgement()
{
    if (!mNoCloseConfirm)
    {
        // Ask for confirmation of acknowledgement. Use warningYesNo() because its default is No.
        if (KAMessageBox::warningYesNo(this, i18nc("@info", "Do you really want to acknowledge this alarm?"),
                                             i18nc("@action:button", "Acknowledge Alarm"), KGuiItem(i18nc("@action:button", "Acknowledge")), KStandardGuiItem::cancel())
            != KMessageBox::Yes)
        {
            return false;
        }
    }
    return true;
}

/******************************************************************************
* Called when the OK button is clicked.
*/
void MessageWindow::slotOk()
{
    if (mDontShowAgainCheck  &&  mDontShowAgainCheck->isChecked())
        KAlarm::setDontShowErrors(mEventId(), mDontShowAgain());
    close();
}

/******************************************************************************
* Called when the KMail button is clicked.
* Tells KMail to display the email message displayed in this message window.
*/
void MessageWindow::slotShowKMailMessage()
{
    qCDebug(KALARM_LOG) << "MessageWindow::slotShowKMailMessage";
    if (mAkonadiItemId() < 0)
        return;
    const QString err = KAlarm::runKMail();
    if (!err.isNull())
    {
        KAMessageBox::sorry(this, err);
        return;
    }
    org::kde::kmail::kmail kmail(KMAIL_DBUS_SERVICE, KMAIL_DBUS_PATH, QDBusConnection::sessionBus());
    // Display the message contents
    QDBusReply<bool> reply = kmail.showMail(mAkonadiItemId());
    bool failed1 = true;
    bool failed2 = true;
    if (!reply.isValid())
        qCCritical(KALARM_LOG) << "kmail 'showMail' D-Bus call failed:" << reply.error().message();
    else if (reply.value())
        failed1 = false;

    // Select the mail folder containing the message
    Akonadi::ItemFetchJob* job = new Akonadi::ItemFetchJob(Akonadi::Item(mAkonadiItemId()));
    job->fetchScope().setAncestorRetrieval(Akonadi::ItemFetchScope::Parent);
    Akonadi::Item::List items;
    if (job->exec())
        items = job->items();
    if (items.isEmpty()  ||  !items.at(0).isValid())
        qCWarning(KALARM_LOG) << "MessageWindow::slotShowKMailMessage: No parent found for item" << mAkonadiItemId();
    else
    {
        const Akonadi::Item& it = items.at(0);
        const Akonadi::Collection::Id colId = it.parentCollection().id();
        reply = kmail.selectFolder(QString::number(colId));
        if (!reply.isValid())
            qCCritical(KALARM_LOG) << "kmail 'selectFolder' D-Bus call failed:" << reply.error().message();
        else if (reply.value())
            failed2 = false;
    }

    if (failed1 || failed2)
        KAMessageBox::sorry(this, xi18nc("@info", "Unable to locate this email in <application>KMail</application>"));
}

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
void MessageWindow::slotEdit()
{
    qCDebug(KALARM_LOG) << "MessageWindow::slotEdit";
    MainWindow* mainWin = MainWindow::mainMainWindow();
    mEditDlg = EditAlarmDlg::create(false, &mOriginalEvent(), false, mainWin, EditAlarmDlg::RES_IGNORE);
    if (!mEditDlg)
        return;
    mEditDlg->setAttribute(Qt::WA_NativeWindow, true);
    KWindowSystem::setMainWindow(mEditDlg->windowHandle(), winId());
    KWindowSystem::setOnAllDesktops(mEditDlg->winId(), false);
    setButtonsReadOnly(true);
    connect(mEditDlg, &QDialog::accepted, this, &MessageWindow::editCloseOk);
    connect(mEditDlg, &QDialog::rejected, this, &MessageWindow::editCloseCancel);
    connect(mEditDlg, &QObject::destroyed, this, &MessageWindow::editCloseCancel);
    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &MessageWindow::activeWindowChanged);
    mainWin->editAlarm(mEditDlg, mOriginalEvent());
}

/******************************************************************************
* Called when OK is clicked in the alarm edit dialog invoked by the Edit button.
* Closes the window.
*/
void MessageWindow::editCloseOk()
{
    mEditDlg = nullptr;
    mNoCloseConfirm = true;   // allow window to close without confirmation prompt
    close();
}

/******************************************************************************
* Called when Cancel is clicked in the alarm edit dialog invoked by the Edit
* button, or when the dialog is deleted.
*/
void MessageWindow::editCloseCancel()
{
    mEditDlg = nullptr;
    setButtonsReadOnly(false);
}

/******************************************************************************
* Called when the active window has changed. If this window has become the
* active window and there is an alarm edit dialog, simulate a modal dialog by
* making the alarm edit dialog the active window instead.
*/
void MessageWindow::activeWindowChanged(WId win)
{
    if (mEditDlg  &&  win == winId())
        KWindowSystem::activateWindow(mEditDlg->winId());
}

/******************************************************************************
* Set or clear the read-only state of the dialog buttons.
*/
void MessageWindow::setButtonsReadOnly(bool ro)
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
void MessageWindow::setDeferralLimit(const KAEvent& event)
{
    mDeferLimit = event.deferralLimit().effectiveKDateTime().toUtc().qDateTime();
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
void MessageWindow::checkDeferralLimit()
{
    if (!mDeferButton->isEnabled()  ||  !mDeferLimit.isValid())
        return;
    int n = KADateTime::currentLocalDate().daysTo(KADateTime(mDeferLimit, KADateTime::LocalZone).date());
    if (n > 0)
        return;
    MidnightTimer::disconnect(this, SLOT(checkDeferralLimit()));
    if (n == 0)
    {
        // The deferral limit will be reached today
        n = QDateTime::currentDateTimeUtc().secsTo(mDeferLimit);
        if (n > 0)
        {
            QTimer::singleShot(n * 1000, this, &MessageWindow::checkDeferralLimit);
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
void MessageWindow::slotDefer()
{
    mDeferDlg = new DeferAlarmDlg(KADateTime::currentDateTime(Preferences::timeSpec()).addSecs(60), mDateTime().isDateOnly(), false, this);
    if (windowFlags() & Qt::X11BypassWindowManagerHint)
        mDeferDlg->setWindowFlags(mDeferDlg->windowFlags() | Qt::X11BypassWindowManagerHint);
    mDeferDlg->setObjectName(QStringLiteral("DeferDlg"));    // used by LikeBack
    mDeferDlg->setDeferMinutes(mDefaultDeferMinutes() > 0 ? mDefaultDeferMinutes() : Preferences::defaultDeferTime());
    mDeferDlg->setLimit(mEvent());
    if (!Preferences::modalMessages())
        lower();
    if (mDeferDlg->exec() == QDialog::Accepted)
    {
        const DateTime dateTime  = mDeferDlg->getDateTime();
        const int      delayMins = mDeferDlg->deferMinutes();
        // Fetch the up-to-date alarm from the calendar. Note that it could have
        // changed since it was displayed.
        KAEvent event;
        if (!mEventId().isEmpty())
            event = ResourcesCalendar::event(mEventId());
        if (event.isValid())
        {
            // The event still exists in the active calendar
            qCDebug(KALARM_LOG) << "MessageWindow::slotDefer: Deferring event" << mEventId();
            KAEvent newev(event);
            newev.defer(dateTime, (mAlarmType() & KAAlarm::REMINDER_ALARM), true);
            newev.setDeferDefaultMinutes(delayMins);
            KAlarm::updateEvent(newev, mDeferDlg, true);
            if (newev.deferred())
                mNoPostAction() = true;
        }
        else
        {
            // Try to retrieve the event from the displaying or archive calendars
            Resource resource;   // receives the event's original resource, if known
            KAEvent event2;
            bool showEdit, showDefer;
            if (!retrieveEvent(event2, resource, showEdit, showDefer))
            {
                // The event doesn't exist any more !?!, so recurrence data,
                // flags, and more, have been lost.
                KAMessageBox::error(this, xi18nc("@info", "<para>Cannot defer alarm:</para><para>Alarm not found.</para>"));
                raise();
                delete mDeferDlg;
                mDeferDlg = nullptr;
                mDeferButton->setEnabled(false);
                mEditButton->setEnabled(false);
                return;
            }
            qCDebug(KALARM_LOG) << "MessageWindow::slotDefer: Deferring retrieved event" << mEventId();
            event2.defer(dateTime, (mAlarmType() & KAAlarm::REMINDER_ALARM), true);
            event2.setDeferDefaultMinutes(delayMins);
            event2.setCommandError(mCommandError());
            // Add the event back into the calendar file, retaining its ID
            // and not updating KOrganizer.
            KAlarm::addEvent(event2, resource, mDeferDlg, KAlarm::USE_EVENT_ID);
            if (event2.deferred())
                mNoPostAction() = true;
            // Finally delete it from the archived calendar now that it has
            // been reactivated.
            event2.setCategory(CalEvent::ARCHIVED);
            Resource res;
            KAlarm::deleteEvent(event2, res, false);
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
    mDeferDlg = nullptr;
}

/******************************************************************************
* Called when the KAlarm icon button in the message window is clicked.
* Displays the main window, with the appropriate alarm selected.
*/
void MessageWindow::displayMainWindow()
{
    KAlarm::displayMainWindowSelected(mEventId().eventId());
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
bool MessageWindow::getWorkAreaAndModal()
{
    mScreenNumber = -1;
    const bool modal = Preferences::modalMessages();
#if KDEPIM_HAVE_X11
    const QList<QScreen*> screens = QGuiApplication::screens();
    const int numScreens = screens.count();
    if (numScreens > 1)
    {
        // There are multiple screens.
        // Check for any full screen windows, even if they are not the active
        // window, and try not to show the alarm message their screens.
        mScreenNumber = QApplication::desktop()->screenNumber(MainWindow::mainMainWindow());  // default = KAlarm's screen
        if (QGuiApplication::primaryScreen()->virtualSiblings().size() > 1)
        {
            // The screens form a single virtual desktop.
            // Xinerama, for example, uses this scheme.
            QVector<FullScreenType> screenTypes(numScreens);
            QVector<QRect> screenRects(numScreens);
            for (int s = 0;  s < numScreens;  ++s)
                screenRects[s] = screens[s]->geometry();
            const FullScreenType full = findFullScreenWindows(screenRects, screenTypes);
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
//qCDebug(KALARM_LOG)<<"full="<<full<<", screen="<<mScreenNumber;
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
        const WId activeId = KWindowSystem::activeWindow();
        const KWindowInfo wi = KWindowInfo(activeId, NET::WMState);
        if (wi.valid()  &&  wi.hasState(NET::FullScreen))
            return false;    // the active window is full screen.
    }
    return modal;
}

#if KDEPIM_HAVE_X11
namespace
{

/******************************************************************************
* In a multi-screen setup (not a single virtual desktop), find whether the
* specified screen has a full screen window on it.
*/
FullScreenType haveFullScreenWindow(int screen)
{
    FullScreenType type = NoFullScreen;
    xcb_connection_t* connection = QX11Info::connection();
    const NETRootInfo rootInfo(connection, NET::ClientList | NET::ActiveWindow, NET::Properties2(), screen);
    const xcb_window_t rootWindow   = rootInfo.rootWindow();
    const xcb_window_t activeWindow = rootInfo.activeWindow();
    const xcb_window_t* windows     = rootInfo.clientList();
    const int windowCount           = rootInfo.clientListCount();
    for (int w = 0;  w < windowCount;  ++w)
    {
        NETWinInfo winInfo(connection, windows[w], rootWindow, NET::WMState|NET::WMGeometry, NET::Properties2());
        if (winInfo.state() & NET::FullScreen)
        {
//qCDebug(KALARM_LOG)<<"Found FULL SCREEN: " << windows[w];
            type = FullScreen;
            if (windows[w] == activeWindow)
                return FullScreenActive;
        }
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
    xcb_connection_t* connection = QX11Info::connection();
    const NETRootInfo rootInfo(connection, NET::ClientList | NET::ActiveWindow, NET::Properties2());
    const xcb_window_t rootWindow   = rootInfo.rootWindow();
    const xcb_window_t activeWindow = rootInfo.activeWindow();
    const xcb_window_t* windows     = rootInfo.clientList();
    const int windowCount           = rootInfo.clientListCount();
//qCDebug(KALARM_LOG)<<"Virtual desktops: Window count="<<windowCount<<", active="<<activeWindow<<", geom="<<QApplication::desktop()->screenGeometry(0);
    NETRect netgeom;
    NETRect netframe;
    for (int w = 0;  w < windowCount;  ++w)
    {
        NETWinInfo winInfo(connection, windows[w], rootWindow, NET::WMState | NET::WMGeometry, NET::Properties2());
        if (winInfo.state() & NET::FullScreen)
        {
            // Found a full screen window - find which screen it's on
            const bool active = (windows[w] == activeWindow);
            winInfo.kdeGeometry(netframe, netgeom);
            const QRect winRect(netgeom.pos.x, netgeom.pos.y, netgeom.size.width, netgeom.size.height);
//qCDebug(KALARM_LOG)<<"Found FULL SCREEN: "<<windows[w]<<", geom="<<winRect;
            for (int s = 0, count = screenRects.count();  s < count;  ++s)
            {
                if (screenRects[s].contains(winRect))
                {
//qCDebug(KALARM_LOG)<<"FULL SCREEN on screen"<<s<<", active="<<active;
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

} // namespace

#endif

// vim: et sw=4:
