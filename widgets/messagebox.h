/*
 *  messagebox.h  -  enhanced KMessageBox class
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <kstdguiitem.h>
#include <kmessagebox.h>


/**
 *  The MessageBox class provides an extension to KMessageBox, including the option for
 *  Continue/Cancel message boxes to have a default button of Cancel.
 *
 *  @short Enhanced KMessageBox.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class MessageBox : public KMessageBox
{
	public:
		/** MessageBox types.
		 *  @li CONT_CANCEL_DEF_CONT - Continue/Cancel, with Continue as the default button.
		 *  @li CONT_CANCEL_DEF_CANCEL - Continue/Cancel, with Cancel as the default button.
		 *  @li YES_NO_DEF_NO - Yes/No, with No as the default button.
		 */
		enum AskType {      // MessageBox types
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
		/** Displays a Continue/Cancel message box with the option as to which button is the default.
		 *  @param defaultButton The default button for the message box. Valid values are Continue or Cancel.
		 *  @param dontAskAgainName If specified, the message box will only be suppressed
		 *    if the user chose Continue last time.
		 */
		static int  warningContinueCancel(QWidget* parent, ButtonCode defaultButton, const QString& text,
		                                  const QString& caption = QString::null,
		                                  const KGuiItem& buttonContinue = KStdGuiItem::cont(),
		                                  const QString& dontAskAgainName = QString::null);
		/** Displays a Continue/Cancel message box.
		 *  @param dontAskAgainName If specified, (1) The message box will only be suppressed
		 *    if the user chose Continue last time, and (2) The default button is that last set
		 *    with either setContinueDefault() or warningContinueCancel() for the same
		 *    @p dontAskAgainName value. If neither method has been used to set a default button,
		 *    Continue is the default.
		 */
		static int  warningContinueCancel(QWidget* parent, const QString& text, const QString& caption = QString::null,
		                                  const KGuiItem& buttonContinue = KStdGuiItem::cont(),
		                                  const QString& dontAskAgainName = QString::null);
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
		 *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
		 */
		static bool shouldBeShownContinue(const QString& dontShowAgainName);
		/** Stores whether the Yes/No message box should or should not be shown again.
		 *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
		 *  @param dontShow If true, the message box will be suppressed and will return @p result.
		 *  @param result The button code to return if the message box is suppressed.
		 */
		static void saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow = true, ButtonCode result = No);
		/** Stores whether a non-Yes/No message box should or should not be shown again.
		 *  If the message box has Cancel as the default button, either setContinueDefault()
		 *  or warningContinueCancel() must have been called previously to set this for the
		 *  specified @p dontShowAgainName value.
		 *  @param dontAskAgainName The identifier controlling whether the message box is suppressed.
		 *  @param dontShow If true, the message box will be suppressed and will return Continue.
		 */
		static void saveDontShowAgainContinue(const QString& dontShowAgainName, bool dontShow = true);
		/** Sets the KConfig object to be used by the MessageBox class. */
		static void setDontShowAskAgainConfig(KConfig* cfg)    { mConfig = cfg; }

	private:
		static void saveDontShowAgain(const QString& dontShowAgainName, bool yesno, bool dontShow, const char* yesnoResult = 0);
		static KConfig*                  mConfig;
		static QMap<QString, ButtonCode> mContinueDefaults;
};

#endif
