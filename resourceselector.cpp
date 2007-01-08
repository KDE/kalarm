/*
 *  resourceselector.cpp  -  calendar resource selection widget
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
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
#include <QToolTip>
#include <QTimer>
#include <Q3Header>
#include <QPainter>
#include <QFont>
#include <QResizeEvent>

#include <kdialog.h>
#include <k3listview.h>
#include <klocale.h>
#include <kglobal.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#include <kmenu.h>
#include <kdebug.h>
#include <kicon.h>
#include <kactioncollection.h>


#include <kcal/resourcecalendar.h>

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "daemon.h"
#include "preferences.h"
#include "resourceconfigdialog.h"
#include "resourceselector.moc"

using namespace KCal;


class ResourceItem : public Q3CheckListItem
{
    public:
	ResourceItem(AlarmResource*, ResourceSelector*, K3ListView* parent);
	AlarmResource*    resource()               { return mResource; }
	void              updateActive();

    protected:
	virtual void      stateChange(bool active);
	virtual void      paintCell(QPainter*, const QColorGroup&, int column, int width, int alignment);

    private:
	AlarmResource*    mResource;
	ResourceSelector* mSelector;
	bool              mBlockStateChange;     // prevent stateChange() from doing anything
};



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

	mListView = new K3ListView(this);
	mListView->addColumn(i18n("Calendar"));
	mListView->setResizeMode(Q3ListView::LastColumn);
	mListView->header()->hide();
	connect(mListView, SIGNAL(selectionChanged(Q3ListViewItem*)), SLOT(selectionChanged(Q3ListViewItem*)));
	connect(mListView, SIGNAL(clicked(Q3ListViewItem*)), SLOT(clicked(Q3ListViewItem*)));
	connect(mListView, SIGNAL(doubleClicked(Q3ListViewItem*, const QPoint&, int)), SLOT(editResource()));
	connect(mListView, SIGNAL(contextMenuRequested(Q3ListViewItem*, const QPoint&, int)),
	        SLOT(contextMenuRequested(Q3ListViewItem*, const QPoint&, int)));
	mListView->setWhatsThis(i18n("List of available resources of the selected type. The checked state shows whether a resource "
	                             "is enabled (checked) or disabled (unchecked). The default resource is shown in bold."));
	mListView->installEventFilter(this);
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

	connect(mCalendar, SIGNAL(signalResourceAdded(AlarmResource*)), SLOT(addItem(AlarmResource*)));
	connect(mCalendar, SIGNAL(signalResourceModified(AlarmResource*)), SLOT(updateItem(AlarmResource*)));
	connect(mCalendar, SIGNAL(standardResourceChange(AlarmResource::Type)), SLOT(slotStandardChanged(AlarmResource::Type)));
	connect(mCalendar, SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotStatusChanged(AlarmResource*, AlarmResources::Change)));

	connect(mAlarmType, SIGNAL(activated(int)), SLOT(refreshList()));
	QTimer::singleShot(0, this, SLOT(refreshList()));
}

/******************************************************************************
* Called when an alarm type has been selected.
* Refresh the resource list with resources of the selected alarm type, and
* add appropriate whatsThis texts to the list and to the Add button.
*/
void ResourceSelector::refreshList()
{
	mListView->clear();

	// Disconnect slots which will be reconnected by addItem()
	disconnect(this, SLOT(slotCloseResource(AlarmResource*)));

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
	AlarmResourceManager* manager = mCalendar->resourceManager();
	for (AlarmResourceManager::Iterator it = manager->begin();  it != manager->end();  ++it)
	{
		if ((*it)->alarmType() == mCurrentAlarmType)
			addItem(*it, false);
	}

	mAddButton->setWhatsThis(addTip);
	mAddButton->setToolTip(addTip);
	selectionChanged(0);
	emitResourcesChanged();
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
		KMessageBox::error(this, "<qt>" + i18n("Unable to create resource of type %1.", "<b>" + type + "</b>") + "</qt>");
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
	ResourceItem* item = currentItem();
	if (!item)
		return;
	AlarmResource* resource = item->resource();
	bool readOnly = resource->readOnly();
	ResourceConfigDialog dlg(this, resource);
	if (dlg.exec())
	{
		// Act on any changed settings.
		// Read-only is handled automatically by AlarmResource::setReadOnly().
		item->setText(0, resource->resourceName());

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
	ResourceItem* item = currentItem();
	if (!item)
		return;
	AlarmResource* resource = item->resource();
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
	QString tmp = "<b>" + item->text(0) + "</b>";
	QString text = std ? i18n("Do you really want to remove your default resource (%1) from the list?", tmp)
	                   : i18n("Do you really want to remove the resource %1 from the list?", tmp);
	text = "<qt>" + text + "</qt>";
	if (KMessageBox::warningContinueCancel(this, text, "", KStandardGuiItem::remove()) == KMessageBox::Cancel)
		return;

	AlarmResourceManager* manager = mCalendar->resourceManager();
	manager->remove(item->resource());
	manager->writeConfig();
	mListView->takeItem(item);
	delete item;
	emitResourcesChanged();
}

/******************************************************************************
* Add the specified resource to the displayed list.
*/
void ResourceSelector::addItem(AlarmResource* resource, bool emitSignal)
{
	new ResourceItem(resource, this, mListView);
	connect(resource, SIGNAL(resourceSaved(AlarmResource*)), SLOT(slotCloseResource(AlarmResource*)));
	if (emitSignal)
		emitResourcesChanged();
}

/******************************************************************************
* Ask for the resource calendar to be closed.
* It is queued up for closing once it has been saved.
*/
void ResourceSelector::queueClose(AlarmResource* resource)
{
	mResourcesToClose.append(resource);
}

/******************************************************************************
* Called when saving the resource has completed, to close the resource.
*/
void ResourceSelector::slotCloseResource(AlarmResource* resource)
{
	int i = mResourcesToClose.indexOf(resource);
	if (i >= 0)
	{
kDebug(5950)<<"ResourceSelector::slotCloseResource()\n";
		resource->close();
		mResourcesToClose.removeAt(i);
	}
}

/******************************************************************************
* Called when the resource has been updated here or elsewhere, to update the
* active status displayed for the resource item.
*/
void ResourceSelector::updateItem(AlarmResource* resource)
{
	ResourceItem* item = findItem(resource);
	if (item)
		item->updateActive();
}

/******************************************************************************
* Called when the list is clicked. If no item is clicked, deselect any
* currently selected item.
*/
void ResourceSelector::clicked(Q3ListViewItem* item)
{
	if (!item)
		selectionChanged(0);
}

/******************************************************************************
* Called when the current selection changes, to enable/disable the
* Delete and Edit buttons accordingly.
*/
void ResourceSelector::selectionChanged(Q3ListViewItem* item)
{
	if (item)
		mListView->setSelected(item, true);
	else
		mListView->setSelected(mListView->selectedItem(), false);
	bool state = item;
	mDeleteButton->setEnabled(state);
	mEditButton->setEnabled(state);
}

void ResourceSelector::emitResourcesChanged()
{
	emit resourcesChanged();
}


/******************************************************************************
* Initialise the button and context menu actions.
*/
void ResourceSelector::initActions(KActionCollection* actions)
{
	mActionReload       = new KAction(KIcon("reload"), i18n("Re&load"), this);
	actions->addAction("resReload", mActionReload      );
	connect(mActionReload, SIGNAL(triggered(bool)), SLOT(reloadResource()));
	mActionSave         = new KAction(KIcon("filesave"), i18n("&Save"), this);
	actions->addAction("resSave", mActionSave        );
	connect(mActionSave, SIGNAL(triggered(bool)), SLOT(saveResource()));
	mActionShowDetails  = new KAction(KIcon("info"), i18n("Show &Details"), this);
	actions->addAction("resDetails", mActionShowDetails );
	connect(mActionShowDetails, SIGNAL(triggered(bool)), SLOT(showInfo()));
	mActionEdit         = new KAction(KIcon("edit"), i18n("&Edit..."), this);
	actions->addAction("resEdit", mActionEdit        );
	connect(mActionEdit, SIGNAL(triggered(bool)), SLOT(editResource()));
	mActionRemove       = new KAction(KIcon("editdelete"), i18n("&Remove"), this);
	actions->addAction("resRemove", mActionRemove      );
	connect(mActionRemove, SIGNAL(triggered(bool)), SLOT(removeResource()));
	mActionSetDefault  = new KToggleAction(this);
	actions->addAction(QLatin1String("resDefault"), mActionSetDefault);
	connect(mActionSetDefault, SIGNAL(triggered(bool)), SLOT(setStandard()));
	QAction * action     = new KAction(KIcon("filenew"), i18n("&Add..."), this);
	actions->addAction("resAdd", action    );
	connect(action, SIGNAL(triggered(bool)), SLOT(addResource()));
	mActionImport       = new KAction(i18n("Im&port..."), this);
	actions->addAction("resImport", mActionImport      );
	connect(mActionImport, SIGNAL(triggered(bool)), SLOT(importCalendar()));
}

void ResourceSelector::setContextMenu(KMenu* menu)
{
	mContextMenu = menu;
}

/******************************************************************************
* Display the context menu for the selected resource.
*/
void ResourceSelector::contextMenuRequested(Q3ListViewItem* itm, const QPoint& pos, int)
{
	if (!mContextMenu)
		return;
	if (!itm)
		selectionChanged(0);
	bool active   = false;
	bool writable = false;
	int type = -1;
	AlarmResource* resource = 0;
	ResourceItem* item = static_cast<ResourceItem*>(itm);
	if (item)
	{
		resource = item->resource();
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
	mActionShowDetails->setEnabled(item);
	mActionEdit->setEnabled(item);
	mActionRemove->setEnabled(item);
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
	mContextMenu->popup(pos);
}

/******************************************************************************
* Called from the context menu to reload the selected resource.
*/
void ResourceSelector::reloadResource()
{
	ResourceItem* item = currentItem();
	if (item)
		AlarmCalendar::resources()->loadAndDaemonReload(item->resource(), this);
}

/******************************************************************************
* Called from the context menu to save the selected resource.
*/
void ResourceSelector::saveResource()
{
	ResourceItem* item = currentItem();
	if (item)
		item->resource()->save();
}

/******************************************************************************
* Called from the context menu to set the selected resource as the default
* for its alarm type. The resource is automatically made active.
*/
void ResourceSelector::setStandard()
{
	ResourceItem* item = currentItem();
	if (item)
	{
		AlarmResource* res = static_cast<ResourceItem*>(item)->resource();
		res->setEnabled(true);
		mCalendar->setStandardResource(res);
	}
}

/******************************************************************************
* Called when a different resource has been set as the standard resource.
*/
void ResourceSelector::slotStandardChanged(AlarmResource::Type type)
{
	for (Q3ListViewItem* item = mListView->firstChild();  item;  item = item->nextSibling())
	{
		ResourceItem* ritem = static_cast<ResourceItem*>(item);
		AlarmResource* res = ritem->resource();
		if (res->alarmType() == type)
			ritem->repaint();
	}
}

/******************************************************************************
* Called when a resource status has changed, to update the list.
*/
void ResourceSelector::slotStatusChanged(AlarmResource* resource, AlarmResources::Change change)
{
	ResourceItem* item = findItem(resource);
	if (!item)
		return;
	switch (change)
	{
		case AlarmResources::Enabled:
			item->updateActive();
			break;
		case AlarmResources::ReadOnly:
			item->repaint();      // show it in the appropriate font
			break;
		default:
			break;
	}
}

/******************************************************************************
* Called from the context menu to merge alarms from an external calendar into
* the selected resource.
*/
void ResourceSelector::importCalendar()
{
	ResourceItem* item = currentItem();
	AlarmCalendar::importAlarms(this, (item ? item->resource() : 0));
}

/******************************************************************************
* Called from the context menu to display information for the selected resource.
*/
void ResourceSelector::showInfo()
{
	ResourceItem* item = currentItem();
	if (item)
	{
		QString txt = "<qt>" + item->resource()->infoText() + "</qt>";
		KMessageBox::information(this, txt);
	}
}

/******************************************************************************
* Find the list item for the specified calendar.
*/
ResourceItem* ResourceSelector::findItem(AlarmResource* rc)
{
	ResourceItem* ritem = 0;
	for (Q3ListViewItem* item = mListView->firstChild();  item;  item = item->nextSibling())
	{
		ritem = static_cast<ResourceItem*>(item);
		if (ritem->resource() == rc)
			return ritem;
	}
	return 0;
}

/******************************************************************************
* Return the currently selected resource in the list.
*/
ResourceItem* ResourceSelector::currentItem()
{
	return static_cast<ResourceItem*>(mListView->currentItem());
}

void ResourceSelector::resizeEvent(QResizeEvent* re)
{
	emit resized(re->oldSize(), re->size());
}

/******************************************************************************
*  Called when any event occurs in the list view.
*  Displays the resource details in a tooltip.
*/
bool ResourceSelector::eventFilter(QObject* obj, QEvent* e)
{
	if (obj != mListView  ||  e->type() != QEvent::ToolTip)
		return false;    // let mListView handle non-tooltip events
	QHelpEvent* he = (QHelpEvent*)e;
	QPoint pt = he->pos();
	ResourceItem* item = (ResourceItem*)mListView->itemAt(pt);
	if (!item)
		return true;
	QString tipText;
	AlarmResource* resource = item->resource();
	if (item->width(mListView->fontMetrics(), mListView, 0) > mListView->viewport()->width())
		tipText = resource->resourceName() + '\n';
	tipText += resource->displayLocation(true);
	bool inactive = !resource->isActive();
	if (inactive)
		tipText += '\n' + i18n("Disabled");
	if (resource->readOnly())
		tipText += (inactive ? ", " : "\n") + i18n("Read-only");

	QRect rect = mListView->itemRect(item);
	kDebug(5950) << "ResourceListTooltip::maybeTip(): display\n";
	QToolTip::showText(pt, tipText);
	return true;
}


/*=============================================================================
=  Class: ResourceItem
=============================================================================*/

ResourceItem::ResourceItem(AlarmResource* resource, ResourceSelector* selector, K3ListView* parent)
	: Q3CheckListItem(parent, resource->resourceName(), CheckBox),
	  mResource(resource),
	  mSelector(selector),
	  mBlockStateChange(false)
{
	updateActive();
}

/******************************************************************************
* Called when the checkbox changes state.
* Opens or closes the resource.
*/
void ResourceItem::stateChange(bool active)
{
	if (mBlockStateChange)
		return;

	bool refreshList = true;
	bool saveChange = false;
	AlarmResources* resources = AlarmResources::instance();
	if (active)
	{
		// Enable the resource.
		// The new setting needs to be written before calling load(), since
		// load completion triggers daemon notification, and the daemon
		// needs to see when it is triggered what the new resource status is.
		mResource->setActive(true);     // enable it now so that load() will work
		saveChange = resources->load(mResource);
		mResource->setActive(false);    // reset so that setEnabled() will work
		refreshList = !saveChange;      // load() emits a signal which refreshes the list
	}
	else
	{
		// Disable the resource
		if (mResource->standardResource())
		{
			// It's the standard resource for its type.
			if (mResource->alarmType() == AlarmResource::ACTIVE)
			{
				KMessageBox::sorry(mSelector, i18n("You cannot disable your default active alarm resource."));
				updateActive();
				return;

			}
			if (mResource->alarmType() == AlarmResource::ARCHIVED  &&  Preferences::archivedKeepDays())
			{
				// Only allow the archived alarms standard resource to be disabled if
				// we're not saving archived alarms.
				KMessageBox::sorry(mSelector, i18n("You cannot disable your default archived alarm resource "
				                                   "while expired alarms are configured to be kept."));
				updateActive();
				return;
			}
			if (KMessageBox::warningContinueCancel(mSelector, i18n("Do you really want to disable your default resource?"))
			           == KMessageBox::Cancel)
			{
				updateActive();
				return;
			}
		}
		saveChange = mResource->save();
		if (mResource->isSaving())
			mSelector->queueClose(mResource);   // close resource after it is saved
		else
			mResource->close();
	}
	if (saveChange)
	{
		// Save the change and notify the alarm daemon
		mResource->setEnabled(active);
//		Daemon::reloadResource(mResource->identifier());
	}
	updateActive();
	if (refreshList)
		mSelector->emitResourcesChanged();
}

/******************************************************************************
* Set the checkbox to reflect the status of the resource.
*/
void ResourceItem::updateActive()
{
	mBlockStateChange = true;
	setOn(mResource->isEnabled());
	mBlockStateChange = false;
}

void ResourceItem::paintCell(QPainter* painter, const QColorGroup& cg, int column, int width, int alignment)
{
	if (mResource->isEnabled()  &&  mResource->standardResource())
	{
		QFont font = painter->font();    // show standard resources in bold
		font.setBold(true);
		painter->setFont(font);
	}
	bool readOnly = mResource->readOnly();
	QColor col;
	switch (mResource->alarmType())
	{
		case AlarmResource::ACTIVE:    col = readOnly ? Qt::darkGray : Qt::black;  break;
		case AlarmResource::ARCHIVED:  col = readOnly ? Qt::green : Qt::darkGreen;  break;
		case AlarmResource::TEMPLATE:  col = readOnly ? Qt::blue : Qt::darkBlue;  break;
	}
	QColorGroup cg2 = cg;
	cg2.setColor(QColorGroup::Text, col);
	Q3CheckListItem::paintCell(painter, cg2, column, width, alignment);
}
