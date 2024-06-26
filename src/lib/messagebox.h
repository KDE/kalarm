/*
 *  messagebox.h  -  enhanced KMessageBox class
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KStandardGuiItem>
#include <KMessageBox>
#include <KLocalizedString>

/**
 *  @short Enhanced KMessageBox.
 *
 *  The KAMessageBox class provides an extension to KMessageBox, including the option for
 *  Continue/Cancel message boxes to have a default button of Cancel.
 *
 *  Note that this class is not called simply MessageBox due to a name clash on Windows.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class KAMessageBox
{
public:
    /** Gets the default button for the Continue/Cancel message box with the specified
     * "don't ask again" name.
     *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
     */
    static KMessageBox::ButtonCode getContinueDefault(const QString& dontAskAgainName);

    /** Sets the default button for the Continue/Cancel message box with the specified
     * "don't ask again" name.
     *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
     *  @param defaultButton The default button for the message box. Valid values are Continue or Cancel.
     */
    static void setContinueDefault(const QString& dontAskAgainName, KMessageBox::ButtonCode defaultButton);

    /** If there is no current setting for whether a non-Yes/No message box should be
     *  shown, sets it to @p defaultShow.
     *  If a Continue/Cancel message box has Cancel as the default button, either
     *  setContinueDefault() or warningContinueCancel() must have been called
     *  previously to set this for the specified @p dontShowAgainName value.
     *  @return true if @p defaultShow was written.
     */
    static bool setDefaultShouldBeShownContinue(const QString& dontShowAgainName, bool defaultShow);

    /** Returns whether a non-Yes/No message box should be shown.
     *  If the message box has Cancel as the default button, either setContinueDefault()
     *  or warningContinueCancel() must have been called previously to set this for the
     *  specified @p dontShowAgainName value.
     *  @param dontShowAgainName The identifier controlling whether the message box is suppressed.
     */
    static bool shouldBeShownContinue(const QString& dontShowAgainName);

    /** Stores whether the Yes/No message box should or should not be shown again.
     *  @param dontShowAgainName The identifier controlling whether the message box is suppressed.
     *  @param dontShow If true, the message box will be suppressed and will return @p result.
     *  @param result The button code to return if the message box is suppressed.
     */
    static void saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow = true, KMessageBox::ButtonCode result = KMessageBox::ButtonCode::SecondaryAction);

    /** Stores whether a non-Yes/No message box should or should not be shown again.
     *  If the message box has Cancel as the default button, either setContinueDefault()
     *  or warningContinueCancel() must have been called previously to set this for the
     *  specified @p dontShowAgainName value.
     *  @param dontShowAgainName The identifier controlling whether the message box is suppressed.
     *  @param dontShow If true, the message box will be suppressed and will return Continue.
     */
    static void saveDontShowAgainContinue(const QString& dontShowAgainName, bool dontShow = true);

    /** Same as KMessageBox::detailedError() except that it defaults to window-modal,
     *  not application-modal. */
    static void detailedError(QWidget* parent, const QString& text, const QString& details,
                              const QString& caption = QString(),
                              KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { KMessageBox::detailedError(parent, text, details, caption, options); }

    /** Same as KMessageBox::informationList() except that it defaults to window-modal,
     *  not application-modal. */
    static void informationList(QWidget* parent, const QString& text, const QStringList& details,
                              const QString& caption = QString(),
                              const QString& dontShowAgainName = QString(),
                              KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { KMessageBox::informationList(parent, text, details, caption, dontShowAgainName, options); }

    /** Same as KMessageBox::error() except that it defaults to window-modal,
     *  not application-modal. */
    static void error(QWidget* parent, const QString& text, const QString& caption = QString(),
                      KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { KMessageBox::error(parent, text, caption, options); }

    /** Same as KMessageBox::information() except that it defaults to window-modal,
     *  not application-modal. */
    static void information(QWidget* parent, const QString& text, const QString& caption = QString(),
                            const QString& dontShowAgainName = QString(),
                            KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { KMessageBox::information(parent, text, caption, dontShowAgainName, options); }

    /** Same as KMessageBox::questionYesNo() except that it defaults to window-modal,
     *  not application-modal. */
    static int questionYesNo(QWidget* parent, const QString& text, const QString& caption = QString(),
                             const KGuiItem& buttonYes = KGuiItem(i18nc("@action:button", "Yes")),
                             const KGuiItem& buttonNo = KGuiItem(i18nc("@action:button", "No")),
                             const QString& dontAskAgainName = QString(),
                             KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { return KMessageBox::questionTwoActions(parent, text, caption, buttonYes, buttonNo, dontAskAgainName, options); }

    /** Same as KMessageBox::questionYesNoCancel() except that it defaults
     *  to window-modal, not application-modal. */
    static int questionYesNoCancel(QWidget* parent, const QString& text, const QString& caption = QString(),
                                   const KGuiItem& buttonYes = KGuiItem(i18nc("@action:button", "Yes")),
                                   const KGuiItem& buttonNo = KGuiItem(i18nc("@action:button", "No")),
                                   const KGuiItem& buttonCancel = KStandardGuiItem::cancel(),
                                   const QString& dontAskAgainName = QString(),
                                   KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { return KMessageBox::questionTwoActionsCancel(parent, text, caption, buttonYes, buttonNo, buttonCancel, dontAskAgainName, options); }
    /** Same as KMessageBox::warningContinueCancel() except that the
     * default button is Cancel, and it defaults to window-modal, not
     * application-modal.
     * @param parent           Parent widget
     * @param text             Message string
     * @param caption          Caption (window title) of the message box
     * @param buttonContinue   The text for the first button (default = "Continue")
     * @param buttonCancel     The text for the second button (default = "Cancel")
     * @param dontAskAgainName If specified, the message box will only be suppressed
     *                         if the user chose Continue last time
     * @param options          Other options
     */
    static int warningCancelContinue(QWidget* parent, const QString& text, const QString& caption = QString(),
                                     const KGuiItem& buttonContinue = KStandardGuiItem::cont(),
                                     const KGuiItem& buttonCancel = KStandardGuiItem::cancel(),
                                     const QString& dontAskAgainName = QString(),
                                     KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { return KMessageBox::warningContinueCancel(parent, text, caption, buttonContinue, buttonCancel, dontAskAgainName, KMessageBox::Options(options | KMessageBox::Dangerous)); }

    /** Same as KMessageBox::warningContinueCancel() except that it
     *  defaults to window-modal, not application-modal. */
    static int warningContinueCancel(QWidget* parent, const QString& text, const QString& caption = QString(),
                                     const KGuiItem& buttonContinue = KStandardGuiItem::cont(),
                                     const KGuiItem& buttonCancel = KStandardGuiItem::cancel(),
                                     const QString& dontAskAgainName = QString(),
                                     KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::WindowModal))
    { return KMessageBox::warningContinueCancel(parent, text, caption, buttonContinue, buttonCancel, dontAskAgainName, options); }

    /** Same as KMessageBox::warningYesNo() except that it defaults to window-modal,
     *  not application-modal. */
    static int warningYesNo(QWidget* parent, const QString& text, const QString& caption = QString(),
                            const KGuiItem& buttonYes = KGuiItem(i18nc("@action:button", "Yes")),
                            const KGuiItem& buttonNo = KGuiItem(i18nc("@action:button", "No")),
                            const QString& dontAskAgainName = QString(),
                            KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify|KMessageBox::Dangerous|KMessageBox::WindowModal))
    { return KMessageBox::questionTwoActions(parent, text, caption, buttonYes, buttonNo, dontAskAgainName, options); }
    /** Shortcut to represent Options(Notify | WindowModal). */
    static const KMessageBox::Options NoAppModal;

private:
    static void saveDontShowAgain(const QString& dontShowAgainName, bool yesno, bool dontShow, const char* yesnoResult = nullptr);
    static QMap<QString, KMessageBox::ButtonCode> mContinueDefaults;
};

// vim: et sw=4:
