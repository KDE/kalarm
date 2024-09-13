/*
 *  messagewindow.cpp  -  displays an alarm message in a window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2024 David Jarvie <djarvie@kde.org>
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
#include "akonadiplugin/akonadiplugin.h"
#include "kalarm_debug.h"

#include <KAboutData>
#include <KStandardGuiItem>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KTextEdit>
#include <KSqueezedTextLabel>
#include <KWindowSystem>
#if ENABLE_X11
#include <KX11Extras>
#include <KWindowInfo>
#include <netwm.h>
#include <QGuiApplication>
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
#include <QScreen>
#include <QStyle>
using namespace Qt::Literals::StringLiterals;

#include "kmailinterface.h"

using namespace KAlarmCal;

namespace
{

enum FullScreenType { NoFullScreen = 0, FullScreen = 1, FullScreenActive = 2 };
FullScreenType haveFullScreenWindow(int screen);
FullScreenType findFullScreenWindows(const QList<QRect>& screenRects, QList<FullScreenType>& screenTypes);

const QLatin1StringView KMAIL_DBUS_SERVICE("org.kde.kmail");
const QLatin1StringView KMAIL_DBUS_PATH("/KMail");

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
        return {static_cast<int>(docsize.width() + 0.99) + verticalScrollBar()->width(),
                static_cast<int>(docsize.height() + 0.99) + horizontalScrollBar()->height()};
    }
};

QList<MessageWindow*> MessageWindow::mWindowList;

/******************************************************************************
* Construct the message window for the specified alarm.
* Other alarms in the supplied event may have been updated by the caller, so
* the whole event needs to be stored for updating the calendar file when it is
* displayed.
*/
MessageWindow::MessageWindow(const KAEvent& event, const KAAlarm& alarm, int flags)
    : MainWindowBase(nullptr, static_cast<Qt::WindowFlags>(WFLAGS | WFLAGS2 | ((flags & AlwaysHide) || getWorkAreaAndModal() ? Qt::WindowType(0) : Qt::X11BypassWindowManagerHint)))
    , MessageDisplay(event, alarm, flags)
    , mRestoreHeight(0)
{
    qCDebug(KALARM_LOG) << "MessageWindow():" << mEventId();
    setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags));
    setWindowModality(Qt::WindowModal);
    setObjectName("MessageWindow"_L1);    // used by LikeBack
    if (!(flags & (NoInitView | AlwaysHide)))
        MessageWindow::setUpDisplay();   // avoid calling virtual method from constructor

    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageWindow::textsChanged);
    connect(mHelper, &MessageDisplayHelper::commandExited, this, &MessageWindow::commandCompleted);

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
MessageWindow::MessageWindow(const KAEvent& event, const DateTime& alarmDateTime,
                       const QStringList& errmsgs, const QString& dontShowAgain)
    : MainWindowBase(nullptr, WFLAGS | WFLAGS2)
    , MessageDisplay(event, alarmDateTime, errmsgs, dontShowAgain)
    , mRestoreHeight(0)
{
    qCDebug(KALARM_LOG) << "MessageWindow(errmsg)";
    setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags));
    setWindowModality(Qt::WindowModal);
    setObjectName("ErrorWin"_L1);    // used by LikeBack
    getWorkAreaAndModal();
    MessageWindow::setUpDisplay();   // avoid calling virtual method from constructor

    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageWindow::textsChanged);

    mWindowList.append(this);
}

/******************************************************************************
* Construct the message window for restoration by session management.
* The window is initialised by readProperties().
*/
MessageWindow::MessageWindow()
    : MainWindowBase(nullptr, WFLAGS)
    , MessageDisplay()
{
    qCDebug(KALARM_LOG) << "MessageWindow(): restore";
    setAttribute(WidgetFlags);
    setWindowModality(Qt::WindowModal);
    setObjectName("RestoredMsgWin"_L1);    // used by LikeBack
    getWorkAreaAndModal();

    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageWindow::textsChanged);
    connect(mHelper, &MessageDisplayHelper::commandExited, this, &MessageWindow::commandCompleted);

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
* Construct the message window.
*/
void MessageWindow::setUpDisplay()
{
    mHelper->initTexts();
    MessageDisplayHelper::DisplayTexts texts = mHelper->texts();

    const bool reminder = (!mErrorWindow()  &&  (mAlarmType() & KAAlarm::Type::Reminder));
    const int leading = fontMetrics().leading();
    setCaption(texts.title);
    QWidget* topWidget = new QWidget(this);
    setCentralWidget(topWidget);
    auto topLayout = new QVBoxLayout(topWidget);
    const int dcmLeft   = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    const int dcmTop    = style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    const int dcmRight  = style()->pixelMetric(QStyle::PM_LayoutRightMargin);
    const int dcmBottom = style()->pixelMetric(QStyle::PM_LayoutBottomMargin);

    QPalette labelPalette = palette();
    labelPalette.setColor(backgroundRole(), labelPalette.color(QPalette::Window));

    // Show the alarm date/time, together with a reminder text where appropriate.
    // Alarm date/time: display time zone if not local time zone.
    mTimeLabel = new QLabel(topWidget);
    mTimeLabel->setText(texts.timeFull);
    mTimeLabel->setFrameStyle(QFrame::StyledPanel);
    mTimeLabel->setPalette(labelPalette);
    mTimeLabel->setAutoFillBackground(true);
    mTimeLabel->setAlignment(Qt::AlignHCenter);
    topLayout->addWidget(mTimeLabel, 0, Qt::AlignHCenter);
    mTimeLabel->setWhatsThis(i18nc("@info:whatsthis", "The scheduled date/time for the message (as opposed to the actual time of display)."));
    if (texts.timeFull.isEmpty())
        mTimeLabel->hide();

    if (!mErrorWindow())
    {
        // It's a normal alarm message window
        switch (mAction())
        {
            case KAEvent::SubAction::File:
            {
                // Display the file name
                auto label = new KSqueezedTextLabel(texts.fileName, topWidget);
                label->setFrameStyle(QFrame::StyledPanel);
                label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
                label->setPalette(labelPalette);
                label->setAutoFillBackground(true);
                label->setWhatsThis(i18nc("@info:whatsthis", "The file whose contents are displayed below"));
                topLayout->addWidget(label, 0, Qt::AlignHCenter);

                if (mErrorMsgs().isEmpty())
                {
                    // Display contents of file
                    auto view = new QTextBrowser(topWidget);
                    view->setFrameStyle(QFrame::NoFrame);
                    view->setWordWrapMode(QTextOption::NoWrap);
                    QPalette pal = view->viewport()->palette();
                    pal.setColor(view->viewport()->backgroundRole(), mBgColour());
                    view->viewport()->setPalette(pal);
                    view->setTextColor(mFgColour());
                    view->setCurrentFont(mFont());

                    switch (texts.fileType)
                    {
                        case File::Type::Image:
                        case File::Type::TextFormatted:
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
            case KAEvent::SubAction::Message:
            {
                // Message label
                // Using MessageText instead of QLabel allows scrolling and mouse copying
                auto text = new MessageText(topWidget);
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
                    auto layout = new QHBoxLayout();
                    layout->addSpacing(hspace);
                    layout->addWidget(text, 1, Qt::AlignHCenter);
                    layout->addSpacing(hspace);
                    topLayout->addLayout(layout);
                }
                if (!reminder)
                    topLayout->addStretch();
                break;
            }
            case KAEvent::SubAction::Command:
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
            case KAEvent::SubAction::Email:
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
            case KAEvent::SubAction::Email:
            {
                // Display the email addresses and subject.
                QFrame* frame = new QFrame(topWidget);
                frame->setFrameStyle(QFrame::Box | QFrame::Raised);
                frame->setWhatsThis(i18nc("@info:whatsthis", "The email to send"));
                topLayout->addWidget(frame, 0, Qt::AlignHCenter);
                auto grid = new QGridLayout(frame);

                QLabel* label = new QLabel(texts.errorEmail[0], frame);
                grid->addWidget(label, 0, 0, Qt::AlignLeft);
                label = new QLabel(texts.errorEmail[1], frame);
                grid->addWidget(label, 0, 1, Qt::AlignLeft);

                label = new QLabel(texts.errorEmail[2], frame);
                grid->addWidget(label, 1, 0, Qt::AlignLeft);
                label = new QLabel(texts.errorEmail[3], frame);
                grid->addWidget(label, 1, 1, Qt::AlignLeft);
                break;
            }
            case KAEvent::SubAction::Command:
            case KAEvent::SubAction::File:
            case KAEvent::SubAction::Message:
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
        auto layout = new QHBoxLayout();
        layout->setContentsMargins(2 * dcmLeft, 2 * dcmTop, 2 * dcmRight, 2 * dcmBottom);
        layout->addStretch();
        topLayout->addLayout(layout);
        QLabel* label = new QLabel(topWidget);
        label->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-error")).pixmap(style()->pixelMetric(QStyle::PM_MessageBoxIconSize)));
        layout->addWidget(label, 0, Qt::AlignRight);
        auto vlayout = new QVBoxLayout();
        layout->addLayout(vlayout);
        for (const QString& msg : mErrorMsgs())
        {
            label = new QLabel(msg, topWidget);
            vlayout->addWidget(label, 0, Qt::AlignLeft);
        }
        layout->addStretch();
        if (!mDontShowAgain().isEmpty())
        {
            mDontShowAgainCheck = new QCheckBox(i18nc("@option:check", "Do not display this error message again for this alarm"), topWidget);
            topLayout->addWidget(mDontShowAgainCheck, 0, Qt::AlignLeft);
        }
    }

    auto grid = new QGridLayout();
    grid->setColumnStretch(0, 1);     // keep the buttons right-adjusted in the window
    topLayout->addLayout(grid);
    int gridIndex = 1;

    // Close button
    mOkButton = new PushButton(KStandardGuiItem::close(), topWidget);
    // Prevent accidental acknowledgement of the message if the user is typing when the window appears
    mOkButton->clearFocus();
    mOkButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
    connect(mOkButton, &QAbstractButton::clicked, this, &MessageWindow::slotOk);
    grid->addWidget(mOkButton, 0, gridIndex++, Qt::AlignHCenter);
    mOkButton->setWhatsThis(i18nc("@info:whatsthis", "Acknowledge the alarm"));

    if (mShowEdit())
    {
        // Edit button
        mEditButton = new PushButton(i18nc("@action:button", "Edit..."), topWidget);
        mEditButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
        connect(mEditButton, &QAbstractButton::clicked, this, &MessageWindow::slotEdit);
        grid->addWidget(mEditButton, 0, gridIndex++, Qt::AlignHCenter);
        mEditButton->setToolTip(i18nc("@info:tooltip", "Edit the alarm"));
        mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the alarm."));
    }

    // Defer button
    mDeferButton = new PushButton(i18nc("@action:button", "Defer..."), topWidget);
    mDeferButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
    connect(mDeferButton, &QAbstractButton::clicked, this, &MessageWindow::slotDefer);
    grid->addWidget(mDeferButton, 0, gridIndex++, Qt::AlignHCenter);
    mDeferButton->setToolTip(i18nc("@info:tooltip", "Defer the alarm until later"));
    mDeferButton->setWhatsThis(xi18nc("@info:whatsthis", "<para>Defer the alarm until later.</para>"
                                    "<para>You will be prompted to specify when the alarm should be redisplayed.</para>"));

    if (mNoDefer())
        mDeferButton->hide();
    else
        mHelper->setDeferralLimit(mEvent());    // ensure that button is disabled when alarm can't be deferred any more

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

    if (mEmailId() >= 0  &&  Preferences::useAkonadi())
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
#if ENABLE_X11
    if (KWindowSystem::isPlatformX11())
    {
        const bool modal = !(windowFlags() & Qt::X11BypassWindowManagerHint);
        NET::States wstate = NET::Sticky | NET::KeepAbove;
        if (modal)
            wstate |= NET::Modal;
        WId winid = winId();
        KX11Extras::setState(winid, wstate);
        KX11Extras::setOnAllDesktops(winid, true);   // show on all virtual desktops
    }
#endif

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
        for (MessageWindow* win : std::as_const(mWindowList))
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
    if (!mAlwaysHidden())
        display();
}

void MessageWindow::raiseDisplay()
{
    raise();
}

/******************************************************************************
* Raise the alarm window, re-output any required audio notification, and
* reschedule the alarm in the calendar file.
*/
void MessageWindow::repeat(const KAAlarm& alarm)
{
    if (!mInitialised)
        return;
    if (mDeferData)
    {
        // Cancel any deferral dialog so that the user notices something's going on,
        // and also because the deferral time limit will have changed.
        delete mDeferData;
        mDeferData = nullptr;
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
            if (!mDeferData  ||  Preferences::modalMessages())
            {
                raise();
                playAudio();
            }
            if (mDeferButton->isVisible())
            {
                mDeferButton->setEnabled(true);
                mHelper->setDeferralLimit(event);    // ensure that button is disabled when alarm can't be deferred any more
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
        mHelper->setDeferralLimit(mEvent());    // ensure that button is disabled when alarm can't be deferred any more
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
        mTimeLabel->setText(texts.timeFull);
        if (mRemainingText)
            mRemainingText->hide();
        setMinimumHeight(0);
        centralWidget()->layout()->activate();
        setMinimumHeight(sizeHint().height());
        resize(sizeHint());
    }
}

/******************************************************************************
* Update and show the alarm's trigger time.
* This is assumed to have previously been hidden.
*/
void MessageWindow::showDateTime(const KAEvent& event, const KAAlarm& alarm)
{
    if (mTimeLabel  &&  mHelper->updateDateTime(event, alarm))
    {
        mTimeLabel->setText(mHelper->texts().timeFull);
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

    if (ids & MessageDisplayHelper::DisplayTexts::TimeFull)
        mTimeLabel->setText(texts.timeFull);

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
* Called when the command providing the alarm message text has exited.
* 'success' is true if the command did not fail completely.
*/
void MessageWindow::commandCompleted(bool success)
{
    if (!success)
    {
        // The command failed completely. KAlarmApp will output an error
        // message, so delete the empty window.
        close();
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
    if (KWindowSystem::isPlatformWayland())
        return false;    // Wayland doesn't allow positioning of windows

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
            for (MessageWindow* w : std::as_const(mWindowList))
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
        for (MessageWindow* w : std::as_const(mWindowList))
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
    for (MessageWindow* w : std::as_const(mWindowList))
    {
        if (w->pos() != topLeft)
            return true;
    }
    return false;
}

/******************************************************************************
* Display the window, if it should not already be auto-closed.
* If windows are being positioned away from the mouse cursor, it is initially
* positioned at the top left to slightly reduce the number of times the
* windows need to be moved in showEvent().
*/
void MessageWindow::display()
{
    connect(mHelper, &MessageDisplayHelper::autoCloseNow, this, &QWidget::close);
    if (mHelper->activateAutoClose())
    {
        if (Preferences::messageButtonDelay() == 0)
            move(0, 0);
        MainWindowBase::show();
        // Ensure that the screen wakes from sleep, in case the window manager
        // doesn't do this when the window is displayed.
        mHelper->wakeScreen();
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
        case KAEvent::SubAction::Message:
            desired = MainWindowBase::sizeHint();
            break;
        case KAEvent::SubAction::Command:
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
            [[fallthrough]];
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
    if (mErrorWindow()  ||  mAlarmType() == KAAlarm::Type::Invalid)
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
        if (mAction() == KAEvent::SubAction::File  &&  mErrorMsgs().isEmpty())
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
    QTimer::singleShot(0, this, &MessageWindow::frameDrawn);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    mShown = true;
}

/******************************************************************************
* Called when the window has been moved.
*/
void MessageWindow::moveEvent(QMoveEvent* e)
{
    MainWindowBase::moveEvent(e);
    if (!KWindowSystem::isPlatformWayland())  // Wayland doesn't allow positioning of windows
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
    if (!mErrorWindow()  &&  mAction() == KAEvent::SubAction::Message)
    {
        const QSize s = sizeHint();
        if (width() > s.width()  ||  height() > s.height())
            resize(s);
    }
    if (!KWindowSystem::isPlatformWayland())  // Wayland doesn't allow positioning of windows
        theApp()->setSpreadWindowsState(isSpread(Desktop::workArea(mScreenNumber).topLeft()));
}

/******************************************************************************
* Called when the window has been displayed properly (in its correct position),
* to play sounds and reschedule the event.
*/
void MessageWindow::displayComplete()
{
    mHelper->displayComplete(true);

    if (!mAlwaysHidden())
    {
        // Enable the window's buttons either now or after the configured delay
        if (mButtonDelay > 0)
            QTimer::singleShot(mButtonDelay, this, &MessageWindow::enableButtons);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
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
    if (mDeferButton->isVisible()  &&  !mDisableDeferral())
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
        if (mShown  &&  mAction() == KAEvent::SubAction::File  &&  mErrorMsgs().isEmpty())
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
    if (!mNoCloseConfirm())
    {
        // Ask for confirmation of acknowledgement. Use warningYesNo() because its default is No.
        if (KAMessageBox::warningYesNo(this, i18nc("@info", "Do you really want to acknowledge this alarm?"),
                                             i18nc("@action:button", "Acknowledge Alarm"), KGuiItem(i18nc("@action:button", "Acknowledge")), KStandardGuiItem::cancel())
            != KMessageBox::ButtonCode::PrimaryAction)
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
    AkonadiPlugin* akonadiPlugin = Preferences::akonadiPlugin();
    if (!akonadiPlugin)
        return;
    qCDebug(KALARM_LOG) << "MessageWindow::slotShowKMailMessage";
    if (mEmailId() < 0)
        return;
    org::kde::kmail::kmail kmail(KMAIL_DBUS_SERVICE, KMAIL_DBUS_PATH, QDBusConnection::sessionBus());
    // Display the message contents
    QDBusReply<bool> reply = kmail.showMail(mEmailId());
    bool failed1 = true;
    bool failed2 = true;
    if (!reply.isValid())
        qCCritical(KALARM_LOG) << "kmail 'showMail' D-Bus call failed:" << reply.error().message();
    else if (reply.value())
        failed1 = false;

    // Select the mail folder containing the message
    qint64 colId = akonadiPlugin->getCollectionId(mEmailId());
    if (colId < 0)
        qCWarning(KALARM_LOG) << "MessageWindow::slotShowKMailMessage: No parent found for item" << mEmailId();
    else
    {
        reply = kmail.selectFolder(QString::number(colId));
        if (!reply.isValid())
            qCCritical(KALARM_LOG) << "kmail 'selectFolder' D-Bus call failed:" << reply.error().message();
        else if (reply.value())
            failed2 = false;
    }

    if (failed1 || failed2)
        KAMessageBox::error(this, xi18nc("@info", "Unable to locate this email in <application>KMail</application>"));
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
    EditAlarmDlg* dlg = mHelper->createEdit();
    if (!dlg)
        return;
    KWindowSystem::setMainWindow(dlg->windowHandle(), winId());
#if ENABLE_X11
    if (KWindowSystem::isPlatformX11())
        KX11Extras::setOnAllDesktops(dlg->winId(), false);   // don't show on all virtual desktops
#endif
    setButtonsReadOnly(true);
#if ENABLE_X11
    if (KWindowSystem::isPlatformX11())
        connect(KX11Extras::self(), &KX11Extras::activeWindowChanged, this, &MessageWindow::activeWindowChanged);
#endif
    mHelper->executeEdit();
}

/******************************************************************************
* Called when Cancel is clicked in the alarm edit dialog invoked by the Edit
* button, or when the dialog is deleted.
*/
void MessageWindow::editDlgCancelled()
{
    setButtonsReadOnly(false);
}

/******************************************************************************
* Called when the active window has changed. If this window has become the
* active window and there is an alarm edit dialog, simulate a modal dialog by
* making the alarm edit dialog the active window instead.
*/
void MessageWindow::activeWindowChanged(WId win)
{
#if ENABLE_X11
    // Note that the alarm edit dialog is not a QWindow, so we can't call
    // QWindow::requestActivate().
    if (mEditDlg()  &&  win == winId())
        KX11Extras::activateWindow(mEditDlg()->winId());
#endif
}

/******************************************************************************
* Called when the Defer... button is clicked.
* Displays the defer message dialog.
*/
void MessageWindow::slotDefer()
{
    mDeferData = createDeferDlg(this, false);
    if (windowFlags() & Qt::X11BypassWindowManagerHint)
        mDeferData->dlg->setWindowFlags(mDeferData->dlg->windowFlags() | Qt::X11BypassWindowManagerHint);
    if (!Preferences::modalMessages())
        lower();
    executeDeferDlg(mDeferData);
    mDeferData = nullptr;   // it was deleted by executeDeferDlg()
}

/******************************************************************************
* Set or clear the read-only state of the dialog buttons.
*/
void MessageWindow::setButtonsReadOnly(bool ro)
{
    mOkButton->setReadOnly(ro, true);
    mDeferButton->setReadOnly(ro, true);
    if (mEditButton)
        mEditButton->setReadOnly(ro, true);
    if (mSilenceButton)
        mSilenceButton->setReadOnly(ro, true);
    if (mKMailButton)
        mKMailButton->setReadOnly(ro, true);
    mKAlarmButton->setReadOnly(ro, true);
}

bool MessageWindow::isDeferButtonEnabled() const
{
    return mDeferButton->isEnabled()  &&  mDeferButton->isVisible();
}

void MessageWindow::enableDeferButton(bool enable)
{
    mDeferButton->setEnabled(enable);
}

void MessageWindow::enableEditButton(bool enable)
{
    if (mEditButton)
        mEditButton->setEnabled(enable);
}

/******************************************************************************
* Called when the KAlarm icon button in the message window is clicked.
* Displays the main window, with the appropriate alarm selected.
*/
void MessageWindow::displayMainWindow()
{
    MessageDisplay::displayMainWindow();
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
    const QList<QScreen*> screens = QGuiApplication::screens();
    const int numScreens = screens.count();
    if (numScreens > 1)
    {
        // There are multiple screens.
        // Check for any full screen windows, even if they are not the active
        // window, and try not to show the alarm message their screens.
        mScreenNumber = screens.indexOf(MainWindow::mainMainWindow()->screen());  // default = KAlarm's screen
        if (QGuiApplication::primaryScreen()->virtualSiblings().size() > 1)
        {
            // The screens form a single virtual desktop.
            // Xinerama, for example, uses this scheme.
            QList<FullScreenType> screenTypes(numScreens);
            QList<QRect> screenRects(numScreens);
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
    if (modal)
    {
#if ENABLE_X11
        if (KWindowSystem::isPlatformX11())
        {
            const WId activeId = KX11Extras::activeWindow();
            const KWindowInfo wi = KWindowInfo(activeId, NET::WMState);
            if (wi.valid()  &&  wi.hasState(NET::FullScreen))
                return false;    // the active window is full screen.
        }
#endif
    }
    return modal;
}

namespace
{

/******************************************************************************
* In a multi-screen setup (not a single virtual desktop), find whether the
* specified screen has a full screen window on it.
*/
FullScreenType haveFullScreenWindow(int screen)
{
    FullScreenType type = NoFullScreen;
//TODO: implement on Wayland
#if ENABLE_X11
    if (KWindowSystem::isPlatformX11())
    {
        using namespace QNativeInterface;
        auto* x11App = qGuiApp->nativeInterface<QX11Application>();
        if (!x11App)
            return type;
        xcb_connection_t* connection = x11App->connection();
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
    }
#endif // ENABLE_X11
    return type;
}

/******************************************************************************
* In a multi-screen setup (single virtual desktop, e.g. Xinerama), find which
* screens have full screen windows on them.
*/
FullScreenType findFullScreenWindows(const QList<QRect>& screenRects, QList<FullScreenType>& screenTypes)
{
    FullScreenType result = NoFullScreen;
    screenTypes.fill(NoFullScreen);
//TODO: implement on Wayland
#if ENABLE_X11
    if (KWindowSystem::isPlatformX11())
    {
        using namespace QNativeInterface;
        auto* x11App = qGuiApp->nativeInterface<QX11Application>();
        if (!x11App)
            return result;
        xcb_connection_t* connection = x11App->connection();
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
    }
#endif // ENABLE_X11
    return result;
}

} // namespace

#include "moc_messagewindow.cpp"

// vim: et sw=4:
