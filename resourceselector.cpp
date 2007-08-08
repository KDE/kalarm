/*
 *  resourceselector.cpp  -  calendar resource selection widget
 *  Program:  kalarm
 *  Copyright © 2006,2007 by David Jarvie <software@astrojar.org.uk>
 *  Based on KOrganizer's ResourceView class and KAddressBook's ResourceSelection class,
 *  Copyright (C) 2003,2004 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
 *  Copyright (c) 2004 Tobias Koenig <tokoe@kde.org>
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

#include "kalarm.h"

#include <QLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include <QTimer>
#include <QPainter>
#include <QFont>
#include <QResizeEvent>
#include <QApplication>

#include <kdialog.h>
#include <klocale.h>
#include <kglobal.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#include <kmenu.h>
#include <kdebug.h>
#include <kicon.h>
#include <kactioncollection.h>
#include <kcolordialog.h>

#include <kcal/resourcecalendar.h>

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "daemon.h"
#include "preferences.h"
#include "resourceconfigdialog.h"
#include "resourcemodelview.h"
#include "resourceselector.moc"

using namespace KCal;


ResourceSelector::ResourceSelector(AlarmResources* calendar, QWidget* parent)
	: QFrame(parent),
	  mCalendar(calendar),
	  mContextMenu(0)
{
	QBoxLayout* topLayout = new QVBoxLayout(this);
	topLayout->setMargin(KDialog::spacingHint());   // use spacingHint for the margin

	QLabel* label = new QLabel(i18n("Resources"), this);
	topLayout->addWidget(label, 0, Qt::AlignHCenter);
	topLayout->addSpacing(KDialog::spacingHint());

	mAlarmType = new QComboBox(this);
	mAlarmType->addItem(i18n("Active Alarms"));
	mAlarmType->addItem(i18n("Archived Alarms"));
	mAlarmType->addItem(i18n("Alarm Templates"));
	mAlarmType->setFixedHeight(mAlarmType->sizeHint().height());
	mAlarmType->setWhatsThis(i18n("Choose which type of data to show alarm resources for"));
	topLayout->addWidget(mAlarmType);

	ResourceModel* model = ResourceModel::instance(this);
	ResourceFilterModel* filterModel = new ResourceFilterModel(model, this);
	mListView = new ResourceView(this);
	mListView->setModel(filterModel);
	ResourceDelegate* delegate = new ResourceDelegate(mListView);
	mListView->setItemDelegate(delegate);
	connect(mListView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), SLOT(selectionChanged()));
	mListView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(mListView, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(contextMenuRequested(const QPoint&)));
	mListView->setWhatsThis(i18n("List of available resources of the selected type. The checked state shows whether a resource "
	                             "is enabled (checked) or disabled (unchecked). The default resource is shown in bold."));
	topLayout->addWidget(mListView);
	topLayout->addSpacing(KDialog::spacingHint());


	mAddButton    = new QPushButton(i18n("Add..."), this);
	mEditButton   = new QPushButton(i18n("Edit..."), this);
	mDeleteButton = new QPushButton(i18n("Remove"), this);
	topLayout->addWidget(mAddButton, 0, Qt::AlignHCenter);
	topLayout->addSpacing(KDialog::spacingHint());
	topLayout->addWidget(mEditButton, 0, Qt::AlignHCenter);
	topLayout->addSpacing(KDialog::spacingHint());
	topLayout->addWidget(mDeleteButton, 0, Qt::AlignHCenter);
	mEditButton->setWhatsThis(i18n("Edit the highlighted resource"));
	mDeleteButton->setWhatsThis(i18n("Remove the highlighted resource from the list.\n"
	                                 "The resource itself is left intact, and may subsequently be reinstated in the list if desired."));
	mEditButton->setDisabled(true);
	mDeleteButton->setDisabled(true);
	connect(mAddButton, SIGNAL(clicked()), SLOT(addResource()));
	connect(mEditButton, SIGNAL(clicked()), SLOT(editResource()));
	connect(mDeleteButton, SIGNAL(clicked()), SLOT(removeResource()));

	connect(mAlarmType, SIGNAL(activated(int)), SLOT(alarmTypeSelected()));
	QTimer::singleShot(0, this, SLOT(alarmTypeSelected()));
}

/******************************************************************************
* Called when an alarm type has been selected.
* Filter the resource list to show resources of the selected alarm type, and
* add appropriate whatsThis texts to the list and to the Add button.
*/
void ResourceSelector::alarmTypeSelected()
{ 
	QString addTip;
	switch (mAlarmType->currentIndex())
	{
		case 0:
			mCurrentAlarmType = AlarmResource::ACTIVE;
			addTip = i18n("Add a new active alarm resource");
			break;
		case 1:
			mCurrentAlarmType = AlarmResource::ARCHIVED;
			addTip = i18n("Add a new archived alarm resource");
			break;
		case 2:
			mCurrentAlarmType = AlarmResource::TEMPLATE;
			addTip = i18n("Add a new alarm template resource");
			break;
	}
	static_cast<ResourceFilterModel*>(mListView->model())->setFilter(mCurrentAlarmType);
	mAddButton->setWhatsThis(addTip);
	mAddButton->setToolTip(addTip);
	emit resourcesChanged();
}

/******************************************************************************
* Prompt the user for a new resource to add to the list.
*/
void ResourceSelector::addResource()
{
	AlarmResourceManager* manager = mCalendar->resourceManager();
	QStringList descs = manager->resourceTypeDescriptions();
	bool ok = false;
	QString desc = KInputDialog::getItem(i18n("Resource Configuration"),
	                                     i18n("Select storage type of new resource:"), descs, 0, false, &ok, this);
	if (!ok)
		return;
	QString type = manager->resourceTypeNames()[descs.indexOf(desc)];
	AlarmResource* resource = dynamic_cast<AlarmResource*>(manager->createResource(type));
	if (!resource)
	{
		KMessageBox::error(this, i18n("<qt>Unable to create resource of type <b>%1</b>.</qt>", type));
		return;
	}
	resource->setResourceName(i18n("%1 resource", type));
	resource->setAlarmType(mCurrentAlarmType);
	resource->setActive(false);   // prevent setReadOnly() declaring it as unwritable before we've tried to load it

	ResourceConfigDialog dlg(this, resource);
	if (dlg.exec())
	{
		resource->setActive(true);
		resource->setTimeSpec(Preferences::timeZone());
		manager->add(resource);
		manager->writeConfig();
		mCalendar->resourceAdded(resource);   // load the resource and connect in-process change signals
	}
	else
	{
		delete resource;
		resource = 0;
	}
}

/******************************************************************************
* Edit the currently selected resource.
*/
void ResourceSelector::editResource()
{
	AlarmResource* resource = currentResource();
	if (!resource)
		return;
	bool readOnly = resource->readOnly();
	ResourceConfigDialog dlg(this, resource);
	if (dlg.exec())
	{
		// Act on any changed settings.
		// Read-only is handled automatically by AlarmResource::setReadOnly().
		if (!readOnly  &&  resource->readOnly()  &&  resource->standardResource())
		{
			// A standard resource is being made read-only.
			if (resource->alarmType() == AlarmResource::ACTIVE)
			{
				KMessageBox::sorry(this, i18n("You cannot make your default active alarm resource read-only."));
				resource->setReadOnly(false);
			}
			else if (resource->alarmType() == AlarmResource::ARCHIVED  &&  Preferences::archivedKeepDays())
			{
				// Only allow the archived alarms standard resource to be made read-only
				// if we're not saving archived alarms.
				KMessageBox::sorry(this, i18n("You cannot make your default archived alarm resource "
				                              "read-only while expired alarms are configured to be kept."));
				resource->setReadOnly(false);
			}
			else if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to make your default resource read-only?"))
			           == KMessageBox::Cancel)
			{
				resource->setReadOnly(false);
			}
		}
	}
}

/******************************************************************************
* Remove the currently selected resource from the displayed list.
*/
void ResourceSelector::removeResource()
{
	AlarmResource* resource = currentResource();
	if (!resource)
		return;
	bool std = resource->standardResource();
	if (std)
	{
		// It's the standard resource for its type.
		if (resource->alarmType() == AlarmResource::ACTIVE)
		{
			KMessageBox::sorry(this, i18n("You cannot remove your default active alarm resource."));
			return;
		}
		if (resource->alarmType() == AlarmResource::ARCHIVED  &&  Preferences::archivedKeepDays())
		{
			// Only allow the archived alarms standard resource to be removed if
			// we're not saving archived alarms.
			KMessageBox::sorry(this, i18n("You cannot remove your default archived alarm resource "
			                              "while expired alarms are configured to be kept."));
			return;
		}
	}
	QString text = std ? i18n("<qt>Do you really want to remove your default resource (<b>%1</b>) from the list?</qt>", resource->resourceName())
	                   : i18n("<qt>Do you really want to remove the resource <b>%1</b> from the list?</qt>", resource->resourceName());
	if (KMessageBox::warningContinueCancel(this, text, "", KStandardGuiItem::remove()) == KMessageBox::Cancel)
		return;

	AlarmResourceManager* manager = mCalendar->resourceManager();
	manager->remove(resource);
	manager->writeConfig();
//	mListView->takeItem(item);
//	delete item;
	emit resourcesChanged();
}

/******************************************************************************
* Called when the current selection changes, to enable/disable the
* Delete and Edit buttons accordingly.
*/
void ResourceSelector::selectionChanged()
{
	bool state = mListView->selectionModel()->selectedRows().count();
	mDeleteButton->setEnabled(state);
	mEditButton->setEnabled(state);
}

/******************************************************************************
* Initialise the button and context menu actions.
*/
void ResourceSelector::initActions(KActionCollection* actions)
{
	mActionReload      = new KAction(KIcon("view-refresh"), i18n("Re&load"), this);
	actions->addAction(QLatin1String("resReload"), mActionReload);
	connect(mActionReload, SIGNAL(triggered(bool)), SLOT(reloadResource()));
	mActionSave        = new KAction(KIcon("document-save"), i18n("&Save"), this);
	actions->addAction(QLatin1String("resSave"), mActionSave);
	connect(mActionSave, SIGNAL(triggered(bool)), SLOT(saveResource()));
	mActionShowDetails = new KAction(KIcon("document-properties"), i18n("Show &Details"), this);
	actions->addAction(QLatin1String("resDetails"), mActionShowDetails);
	connect(mActionShowDetails, SIGNAL(triggered(bool)), SLOT(showInfo()));
	mActionSetColour   = new KAction(KIcon("color-picker"), i18n("Set &Color"), this);
	actions->addAction(QLatin1String("resSetColour"), mActionSetColour);
	connect(mActionSetColour, SIGNAL(triggered(bool)), SLOT(setColour()));
	mActionClearColour   = new KAction(i18n("Clear C&olor"), this);
	actions->addAction(QLatin1String("resClearColour"), mActionClearColour);
	connect(mActionClearColour, SIGNAL(triggered(bool)), SLOT(clearColour()));
	mActionEdit        = new KAction(KIcon("edit"), i18n("&Edit..."), this);
	actions->addAction(QLatin1String("resEdit"), mActionEdit);
	connect(mActionEdit, SIGNAL(triggered(bool)), SLOT(editResource()));
	mActionRemove      = new KAction(KIcon("edit-delete"), i18n("&Remove"), this);
	actions->addAction(QLatin1String("resRemove"), mActionRemove);
	connect(mActionRemove, SIGNAL(triggered(bool)), SLOT(removeResource()));
	mActionSetDefault  = new KToggleAction(this);
	actions->addAction(QLatin1String("resDefault"), mActionSetDefault);
	connect(mActionSetDefault, SIGNAL(triggered(bool)), SLOT(setStandard()));
	QAction * action   = new KAction(KIcon("document-new"), i18n("&Add..."), this);
	actions->addAction(QLatin1String("resAdd"), action);
	connect(action, SIGNAL(triggered(bool)), SLOT(addResource()));
	mActionImport      = new KAction(i18n("Im&port..."), this);
	actions->addAction(QLatin1String("resImport"), mActionImport);
	connect(mActionImport, SIGNAL(triggered(bool)), SLOT(importCalendar()));
}

void ResourceSelector::setContextMenu(KMenu* menu)
{
	mContextMenu = menu;
}

/******************************************************************************
* Display the context menu for the selected resource.
*/
void ResourceSelector::contextMenuRequested(const QPoint& viewportPos)
{
	if (!mContextMenu)
		return;
	bool active   = false;
	bool writable = false;
	int type = -1;
	AlarmResource* resource = 0;
	QModelIndex index = mListView->indexAt(viewportPos);
	if (index.isValid())
		resource = static_cast<ResourceFilterModel*>(mListView->model())->resource(index);
	else
		mListView->clearSelection();
	if (resource)
	{
		active   = resource->isEnabled();
		type     = resource->alarmType();
		writable = resource->writable();
	}
	else
	{
		switch (mAlarmType->currentIndex())
		{
			case 0:  type = AlarmResource::ACTIVE; break;
			case 1:  type = AlarmResource::ARCHIVED; break;
			case 2:  type = AlarmResource::TEMPLATE; break;
		}
	}
	mActionReload->setEnabled(active);
	mActionSave->setEnabled(active && writable);
	mActionShowDetails->setEnabled(resource);
	mActionSetColour->setEnabled(resource);
	mActionClearColour->setEnabled(resource);
	mActionClearColour->setVisible(resource && resource->colour().isValid());
	mActionEdit->setEnabled(resource);
	mActionRemove->setEnabled(resource);
	mActionImport->setEnabled(active && writable);
	QString text;
	switch (type)
	{
		case AlarmResource::ACTIVE:   text = i18n("Use as &Default for Active Alarms");  break;
		case AlarmResource::ARCHIVED: text = i18n("Use as &Default for Archived Alarms");  break;
		case AlarmResource::TEMPLATE: text = i18n("Use as &Default for Alarm Templates");  break;
		default:  break;
	}
	mActionSetDefault->setText(text);
	bool standard = (resource  &&  resource == mCalendar->getStandardResource(static_cast<AlarmResource::Type>(type)));
	mActionSetDefault->setChecked(active && writable && standard);
	mActionSetDefault->setEnabled(active && writable && !standard);
	mContextMenu->popup(mListView->viewport()->mapToGlobal(viewportPos));
}

/******************************************************************************
* Called from the context menu to reload the selected resource.
*/
void ResourceSelector::reloadResource()
{
	AlarmResource* resource = currentResource();
	if (resource)
		AlarmCalendar::resources()->loadAndDaemonReload(resource, this);
}

/******************************************************************************
* Called from the context menu to save the selected resource.
*/
void ResourceSelector::saveResource()
{
	AlarmResource* resource = currentResource();
	if (resource)
		resource->save();
}

/******************************************************************************
* Called from the context menu to set the selected resource as the default
* for its alarm type. The resource is automatically made active.
*/
void ResourceSelector::setStandard()
{
	AlarmResource* resource = currentResource();
	if (resource)
	{
		resource->setEnabled(true);
		mCalendar->setStandardResource(resource);
	}
}

/******************************************************************************
* Called from the context menu to merge alarms from an external calendar into
* the selected resource (if any).
*/
void ResourceSelector::importCalendar()
{
	AlarmCalendar::importAlarms(this, currentResource());
}

/******************************************************************************
* Called from the context menu to set a colour for the selected resource.
*/
void ResourceSelector::setColour()
{
	AlarmResource* resource = currentResource();
	if (resource)
	{
		QColor colour = resource->colour();
		if (!colour.isValid())
			colour = QApplication::palette().color(QPalette::Base);
		if (KColorDialog::getColor(colour, QColor(), this) == KColorDialog::Accepted)
			resource->setColour(colour);
	}
}

/******************************************************************************
* Called from the context menu to clear the display colour for the selected
* resource.
*/
void ResourceSelector::clearColour()
{
	AlarmResource* resource = currentResource();
	if (resource)
		resource->setColour(QColor());
}

/******************************************************************************
* Called from the context menu to display information for the selected resource.
*/
void ResourceSelector::showInfo()
{
	AlarmResource* resource = currentResource();
	if (resource)
	{
		QString txt = "<qt>" + resource->infoText() + "</qt>";
		KMessageBox::information(this, txt);
	}
}

/******************************************************************************
* Return the currently selected resource in the list.
*/
AlarmResource* ResourceSelector::currentResource() const
{
	return mListView->resource(mListView->selectionModel()->currentIndex());
}

void ResourceSelector::resizeEvent(QResizeEvent* re)
{
	emit resized(re->oldSize(), re->size());
}
