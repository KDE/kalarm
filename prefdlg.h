/*
 *  prefdlg.h  -  program preferences dialog
 *  Program:  kalarm
 *  Copyright © 2001-2008 by David Jarvie <djarvie@kde.org>
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

#ifndef PREFDLG_H
#define PREFDLG_H

#include <kpagedialog.h>

class FontColourPrefTab;
class EditPrefTab;
class EmailPrefTab;
class ViewPrefTab;
class StorePrefTab;
class TimePrefTab;
class MiscPrefTab;


// The Preferences dialog
class KAlarmPrefDlg : public KPageDialog
{
		Q_OBJECT
	public:
		static void display();
		~KAlarmPrefDlg();
		virtual QSize minimumSizeHint() const;

		MiscPrefTab*       mMiscPage;
		TimePrefTab*       mTimePage;
		StorePrefTab*      mStorePage;
		EditPrefTab*       mEditPage;
		EmailPrefTab*      mEmailPage;
		ViewPrefTab*       mViewPage;
		FontColourPrefTab* mFontColourPage;

		KPageWidgetItem*   mMiscPageItem;
		KPageWidgetItem*   mTimePageItem;
		KPageWidgetItem*   mStorePageItem;
		KPageWidgetItem*   mEditPageItem;
		KPageWidgetItem*   mEmailPageItem;
		KPageWidgetItem*   mViewPageItem;
		KPageWidgetItem*   mFontColourPageItem;

	protected:
		virtual void  resizeEvent(QResizeEvent*);

	protected slots:
		virtual void slotOk();
		virtual void slotApply();
		virtual void slotHelp();
		virtual void slotDefault()  { restore(true); }
		virtual void slotCancel();

	private:
		KAlarmPrefDlg();
		void         restore(bool defaults);

		static KAlarmPrefDlg* mInstance;
		bool         mValid;
};

#endif // PREFDLG_H
