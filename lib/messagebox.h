/*
 *  messagebox.h  -  enhanced KMessageBox class
 *  Program:  kalarm
 *  Copyright © 2004,2007,2011,2014 by David Jarvie <djarvie@kde.org>
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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <kdeversion.h>
#include <kstandardguiitem.h>
#include <kmessagebox.h>

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
class KAMessageBox : public KMessageBox
{
    public:
        /** KAMessageBox types.
         *  @li CONT_CANCEL_DEF_CONT - Continue/Cancel, with Continue as the default button.
         *  @li CONT_CANCEL_DEF_CANCEL - Continue/Cancel, with Cancel as the default button.
         *  @li YES_NO_DEF_NO - Yes/No, with No as the default button.
         */
        enum AskType {      // KAMessageBox types
            CONT_CANCEL_DEF_CONT,    // Continue/Cancel, with default = Continue
            CONT_CANCEL_DEF_CANCEL,  // Continue/Cancel, with default = Cancel
            YES_NO_DEF_NO            // Yes/No, with default = No
        };
        /** Gets the default button for the Continue/Cancel message box with the specified
         * "don't ask again" name.
         *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
         */
        static ButtonCode getContinueDefault(const QString& dontAskAgainName);
        /** Sets the default button for the Continue/Cancel message box with the specified
         * "don't ask again" name.
         *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
         *  @param defaultButton The default button for the message box. Valid values are Continue or Cancel.
         */
        static void setContinueDefault(const QString& dontAskAgainName, ButtonCode defaultButton);
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
        static void saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow = true, ButtonCode result = No);
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
                                  Options options = Options(Notify|WindowModal))
        { KMessageBox::detailedError(parent, text, details, caption, options); }

        /** Same as KMessageBox::detailedSorry() except that it defaults to window-modal,
         *  not application-modal. */
        static void detailedSorry(QWidget* parent, const QString& text, const QString& details,
                                  const QString& caption = QString(),
                                  Options options = Options(Notify|WindowModal))
        { KMessageBox::detailedSorry(parent, text, details, caption, options); }

        /** Same as KMessageBox::error() except that it defaults to window-modal,
         *  not application-modal. */
        static void error(QWidget* parent, const QString& text, const QString& caption = QString(),
                          Options options = Options(Notify|WindowModal))
        { KMessageBox::error(parent, text, caption, options); }

        /** Same as KMessageBox::information() except that it defaults to window-modal,
         *  not application-modal. */
        static void information(QWidget* parent, const QString& text, const QString& caption = QString(),
                                const QString& dontShowAgainName = QString(),
                                Options options = Options(Notify|WindowModal))
        { KMessageBox::information(parent, text, caption, dontShowAgainName, options); }

        /** Same as KMessageBox::sorry() except that it defaults to window-modal,
         *  not application-modal. */
        static void sorry(QWidget* parent, const QString& text, const QString& caption = QString(),
                          Options options = Options(Notify|WindowModal))
        { KMessageBox::sorry(parent, text, caption, options); }

        /** Same as KMessageBox::questionYesNo() except that it defaults to window-modal,
         *  not application-modal. */
        static int questionYesNo(QWidget* parent, const QString& text, const QString& caption = QString(),
                                 const KGuiItem& buttonYes = KStandardGuiItem::yes(),
                                 const KGuiItem& buttonNo = KStandardGuiItem::no(),
                                 const QString& dontAskAgainName = QString(),
                                 Options options = Options(Notify|WindowModal))
        { return KMessageBox::questionYesNo(parent, text, caption, buttonYes, buttonNo, dontAskAgainName, options); }

        /** Same as KMessageBox::questionYesNoCancel() except that it defaults
         *  to window-modal, not application-modal. */
        static int questionYesNoCancel(QWidget* parent, const QString& text, const QString& caption = QString(),
                                       const KGuiItem& buttonYes = KStandardGuiItem::yes(),
                                       const KGuiItem& buttonNo = KStandardGuiItem::no(),
                                       const KGuiItem& buttonCancel = KStandardGuiItem::cancel(),
                                       const QString& dontAskAgainName = QString(),
                                       Options options = Options(Notify|WindowModal))
        { return KMessageBox::questionYesNoCancel(parent, text, caption, buttonYes, buttonNo, buttonCancel, dontAskAgainName, options); }

        /** Same as KMessageBox::warningContinueCancel() except that the
         * default button is Cancel, and it defaults to window-modal, not
         * application-modal.
         * @param parent           Parent widget
         * @param text             Message string
         * @param caption          Caption (window title) of the message box
         * @param buttonContinue   The text for the first button (default = "Continue")
         * @param buttonCancel     The text for the second button (default = "Cancel")
         * @param dontAskAgainName If specified, the message box will only be suppressed
         *                         if the user chose Continue last time.
         */
        static int warningCancelContinue(QWidget* parent, const QString& text, const QString& caption = QString(),
                                         const KGuiItem& buttonContinue = KStandardGuiItem::cont(),
                                         const KGuiItem& buttonCancel = KStandardGuiItem::cancel(),
                                         const QString& dontAskAgainName = QString(),
                                         Options options = Options(Notify|WindowModal))
        { return KMessageBox::warningContinueCancel(parent, text, caption, buttonContinue, buttonCancel, dontAskAgainName, Options(options|Dangerous)); }

        /** Same as KMessageBox::warningContinueCancel() except that it
         *  defaults to window-modal, not application-modal. */
        static int warningContinueCancel(QWidget* parent, const QString& text, const QString& caption = QString(),
                                         const KGuiItem& buttonContinue = KStandardGuiItem::cont(),
                                         const KGuiItem& buttonCancel = KStandardGuiItem::cancel(),
                                         const QString& dontAskAgainName = QString(),
                                         Options options = Options(Notify|WindowModal))
        { return KMessageBox::warningContinueCancel(parent, text, caption, buttonContinue, buttonCancel, dontAskAgainName, options); }

        /** Same as KMessageBox::warningYesNo() except that it defaults to window-modal,
         *  not application-modal. */
        static int warningYesNo(QWidget* parent, const QString& text, const QString& caption = QString(),
                                const KGuiItem& buttonYes = KStandardGuiItem::yes(),
                                const KGuiItem& buttonNo = KStandardGuiItem::no(),
                                const QString& dontAskAgainName = QString(),
                                Options options = Options(Notify|Dangerous|WindowModal))
        { return KMessageBox::warningYesNo(parent, text, caption, buttonYes, buttonNo, dontAskAgainName, options); }

        /** Shortcut to represent Options(Notify | WindowModal). */
        static const Options NoAppModal;

#if !KDE_IS_VERSION(4,7,1)
        static const Option WindowModal = Notify;
#endif

    private:
        static void saveDontShowAgain(const QString& dontShowAgainName, bool yesno, bool dontShow, const char* yesnoResult = 0);
        static QMap<QString, ButtonCode> mContinueDefaults;
};

#endif

// vim: et sw=4:
