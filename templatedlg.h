/*
 *  templatedlg.h  -  dialogue to create, edit and delete alarm templates
 *  Program:  kalarm
 *  Copyright (c) 2004,2006 by David Jarvie <software@astrojar.org.uk>
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
#ifndef TEMPLATEDLG_H
#define TEMPLATEDLG_H

#include <kdialog.h>

class QResizeEvent;
class QPushButton;
class TemplateListView;
class KAEvent;


class TemplateDlg : public KDialog
{
		Q_OBJECT
	public:
		static TemplateDlg*  create(QWidget* parent = 0);
		~TemplateDlg();
		static void   createTemplate(const KAEvent* = 0, QWidget* parent = 0, TemplateListView* = 0);

	signals:
		void          emptyToggled(bool notEmpty);

	protected:
		virtual void  resizeEvent(QResizeEvent*);

	private slots:
		void          slotNew();
		void          slotCopy();
		void          slotEdit();
		void          slotDelete();
		void          slotSelectionChanged();

	private:
		TemplateDlg(QWidget* parent);

		static TemplateDlg* mInstance;   // the current instance, to prevent multiple dialogues

		TemplateListView*   mTemplateList;
		QPushButton*        mEditButton;
		QPushButton*        mCopyButton;
		QPushButton*        mDeleteButton;
};

#endif // TEMPLATEDLG_H
