/*
 *  messagebox.h  -  tweaked KMessageBox class
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

#include <kmessagebox.h>

/*=============================================================================
= Class MessageBox
= Adds the option for Continue/Cancel message boxes to have a default of Cancel,
= plus other methods.
=============================================================================*/

class MessageBox : public KMessageBox
{
	public:
		enum AskType {      // MessageBox types
			CONT_CANCEL_DEF_CONT,    // Continue/Cancel, with default = Continue
			CONT_CANCEL_DEF_CANCEL,  // Continue/Cancel, with default = Cancel
			YES_NO_DEF_NO            // Yes/No, with default = No
		};
		static ButtonCode getContinueDefault(const QString& dontAskAgainName);
		static void setContinueDefault(const QString& dontAskAgainName, ButtonCode defaultButton);
		static int  warningContinueCancel(QWidget* parent, ButtonCode defaultButton, const QString& text,
		                                  const QString& caption = QString::null,
		                                  const KGuiItem& buttonContinue = KStdGuiItem::cont(),
		                                  const QString& dontAskAgainName = QString::null, int options = Notify);
		static int  warningContinueCancel(QWidget* parent, const QString& text, const QString& caption = QString::null,
		                                  const KGuiItem& buttonContinue = KStdGuiItem::cont(),
		                                  const QString& dontAskAgainName = QString::null, int options = Notify);
		static bool setDefaultShouldBeShownContinue(const QString& dontShowAgainName, bool defaultShow);
		static bool shouldBeShownContinue(const QString& dontShowAgainName);
		static void saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow = true, ButtonCode result = No);
		static void saveDontShowAgainContinue(const QString& dontShowAgainName, bool dontShow = true);
		static void setDontShowAskAgainConfig(KConfig* cfg)    { mConfig = cfg; }

	private:
		static void saveDontShowAgain(const QString& dontShowAgainName, bool yesno, bool dontShow, const char* yesnoResult = 0);
		static KConfig*                  mConfig;
		static QMap<QString, ButtonCode> mContinueDefaults;
};

#endif
