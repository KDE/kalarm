/*
 *  templatepickdlg.h  -  dialogue to choose an alarm template
 *  Program:  kalarm
 *  Copyright (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
#ifndef TEMPLATEPICKDLG_H
#define TEMPLATEPICKDLG_H

#include <kdialogbase.h>

class TemplateListView;
class KAEvent;


class TemplatePickDlg : public KDialogBase
{
		Q_OBJECT
	public:
		TemplatePickDlg(QWidget* parent = 0, const char* name = 0);
		const KAEvent*    selectedTemplate() const;
	protected:
		virtual void      resizeEvent(QResizeEvent*);
	private slots:
		void              slotSelectionChanged();
	private:
		TemplateListView* mTemplateList;
};

#endif // TEMPLATEPICKDLG_H
