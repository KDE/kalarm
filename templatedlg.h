/*
 *  templatedlg.h  -  dialogue to create, edit and delete alarm templates
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef TEMPLATEDLG_H
#define TEMPLATEDLG_H

#include <kdialogbase.h>

class QPushButton;
class TemplateListView;
class KAEvent;


class TemplateDlg : public KDialogBase
{
		Q_OBJECT
	public:
		static TemplateDlg*  create(QWidget* parent = 0, const char* name = 0);
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
		TemplateDlg(QWidget* parent, const char* name);

		static TemplateDlg* mInstance;   // the current instance, to prevent multiple dialogues

		TemplateListView*   mTemplateList;
		QPushButton*        mEditButton;
		QPushButton*        mCopyButton;
		QPushButton*        mDeleteButton;
};

#endif // TEMPLATEDLG_H
