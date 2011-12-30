/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <sstream>

#include <boost/thread/thread.hpp>
#include <QtCore/qabstractitemmodel.h>

#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/core/virtualdevice.h"

#include "slg2/smalllux.h"
#include "hardwaretree.h"
#include "mainwindow.h"

//------------------------------------------------------------------------------
// Hardware tree view
//------------------------------------------------------------------------------

HardwareTreeItem::HardwareTreeItem(const QVariant &data, HardwareTreeItem *parent) {
	deviceIndex = 0;
	parentItem = parent;
	itemData = data;
	checkable = false;
	checked = false;
}

HardwareTreeItem::HardwareTreeItem(const int index, const QVariant &data,
		HardwareTreeItem *parent) {
	deviceIndex = index;
	parentItem = parent;
	itemData = data;
	checkable = true;
	checked = true;
}

HardwareTreeItem::~HardwareTreeItem() {
	qDeleteAll(childItems);
}

void HardwareTreeItem::appendChild(HardwareTreeItem *item) {
	item->parentItem = this;
	childItems.append(item);
}

HardwareTreeItem *HardwareTreeItem::child(int row) {
	return childItems.value(row);
}

int HardwareTreeItem::childCount() const {
	return childItems.count();
}

int HardwareTreeItem::columnCount() const {
	return 1;
}

QVariant HardwareTreeItem::data(int column) const {
	return itemData;
}

HardwareTreeItem *HardwareTreeItem::parent() {
	return parentItem;
}

int HardwareTreeItem::row() const {
	if (parentItem)
		return parentItem->childItems.indexOf(const_cast<HardwareTreeItem *>(this));

	return 0;
}

//------------------------------------------------------------------------------
// HardwareTreeModel
//------------------------------------------------------------------------------

HardwareTreeModel::HardwareTreeModel(MainWindow *w,
		const vector<DeviceDescription *> &devDescs) : QAbstractItemModel() {
	win = w;

	rootItem = new HardwareTreeItem("Hardware");

	// OpenCL devices
	HardwareTreeItem *oclDev = new HardwareTreeItem("OpenCL");
	rootItem->appendChild(oclDev);

	HardwareTreeItem *oclCPUDev = new HardwareTreeItem("CPUs");
	oclDev->appendChild(oclCPUDev);
	HardwareTreeItem *oclGPUDev = new HardwareTreeItem("GPUs and Accelerators");
	oclDev->appendChild(oclGPUDev);

#ifndef LUXRAYS_DISABLE_OPENCL
	int index = 0;
	for (size_t i = 0; i < devDescs.size(); ++i) {
		DeviceDescription *devDesc = devDescs[i];

		if (devDesc->GetType() ==  DEVICE_TYPE_OPENCL) {
			const OpenCLDeviceDescription *odevDesc = (OpenCLDeviceDescription *)devDesc;

			HardwareTreeItem *newNode = new HardwareTreeItem(index++, odevDesc->GetName().c_str());

			stringstream ss;
			cl::Platform platform = odevDesc->GetOCLDevice().getInfo<CL_DEVICE_PLATFORM>();
			ss << "Platform: " << platform.getInfo<CL_PLATFORM_VENDOR>();
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Platform Version: " << platform.getInfo<CL_PLATFORM_VERSION>();
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Type: ";
			switch (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_TYPE>()) {
				case CL_DEVICE_TYPE_CPU:
					ss << "CPU";
					break;
				case CL_DEVICE_TYPE_GPU:
					ss << "GPU";
					break;
				case CL_DEVICE_TYPE_ACCELERATOR:
					ss << "ACCELERATOR";
					break;
				default:
					ss << "UNKNOWN";
					break;
			}
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Compute Units: " << odevDesc->GetComputeUnits();
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Clock: " << odevDesc->GetOCLDevice().getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>() << " MHz";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Max. Global Memory: " << (odevDesc->GetMaxMemory() / 1024) << " Kbytes";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Local Memory: " << (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / 1024) << " Kbytes";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Max. Constant Memory: " << (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() / 1024) << " Kbytes";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			bool isCPU = (odevDesc->GetOpenCLType() == OCL_DEVICE_TYPE_CPU);
			if (isCPU) {
				// The default mode is GPU-only
				newNode->setChecked(false);
				oclCPUDev->appendChild(newNode);
			} else {
				newNode->setChecked(true);
				oclGPUDev->appendChild(newNode);
			}
			deviceSelection.push_back(!isCPU);
		}
	}
#endif
}

HardwareTreeModel::~HardwareTreeModel() {
	delete rootItem;
}

int HardwareTreeModel::columnCount(const QModelIndex &parent) const {
	if (parent.isValid())
		return static_cast<HardwareTreeItem *>(parent.internalPointer())->columnCount();
	else
		return rootItem->columnCount();
}

QVariant HardwareTreeModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		return item->data(index.column());
	} else if (role ==  Qt::CheckStateRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		if (item->isCheckable()) {
			if (item->isChecked())
				return Qt::Checked;
			else
				return Qt::Unchecked;
		} else
			return QVariant();
	} else if (role == Qt::ForegroundRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		if (item->isCheckable()) {
			// It is a device node
			return QVariant(QColor(Qt::blue));
		} else {
			if (item->childCount() == 0) {
				return QVariant(QColor(Qt::darkGray));
			} else {
				// It is a leaf
				return QVariant();
			}
		}
	} else
		return QVariant();
}

QModelIndex HardwareTreeModel::index(int row, int column, const QModelIndex &parent) const {
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	HardwareTreeItem *parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

	HardwareTreeItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	else
		return QModelIndex();
}

QModelIndex HardwareTreeModel::parent(const QModelIndex &index) const {
	if (!index.isValid())
		return QModelIndex();

	HardwareTreeItem *childItem = static_cast<HardwareTreeItem*>(index.internalPointer());
	HardwareTreeItem *parentItem = childItem->parent();

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

int HardwareTreeModel::rowCount(const QModelIndex &parent) const {
	HardwareTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

	return parentItem->childCount();
}

Qt::ItemFlags HardwareTreeModel::flags(const QModelIndex &index) const {
	if (!index.isValid())
		return Qt::ItemIsSelectable | Qt::ItemIsEnabled;

	HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

	if (item->isCheckable())
		return Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	else
		return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool HardwareTreeModel::setData(const QModelIndex &index, const QVariant &value,
		int role) {
	if (!index.isValid())
		return false;

	if (role == Qt::CheckStateRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		const bool v = (value == Qt::Checked);
		if (v != item->isChecked()) {
			item->setChecked(v);

			// Set pause mode
			win->Pause();

			deviceSelection[item->getDeviceIndex()] = v;
		}

		return true;
	} else
		return false;
}

string HardwareTreeModel::getDeviceSelectionString() const {
	stringstream ss;

	for (size_t i = 0; i < deviceSelection.size(); ++i)
		ss << (deviceSelection[i] ? "1" : "0");

	return ss.str();
}

//------------------------------------------------------------------------------
// DeviceTreeModel
//------------------------------------------------------------------------------

vector<BenchmarkDeviceDescription> BuildDeviceDescriptions(
	const vector<OpenCLIntersectionDevice *> &devices) {
	vector<BenchmarkDeviceDescription> descs;

	for (size_t i = 0; i < devices.size(); ++i) {
		const OpenCLDeviceDescription *odevDesc = ((OpenCLIntersectionDevice *)devices[i])->GetDeviceDesc();
		cl::Platform platform = odevDesc->GetOCLDevice().getInfo<CL_DEVICE_PLATFORM>();

		BenchmarkDeviceDescription desc;
		desc.platformName = platform.getInfo<CL_PLATFORM_VENDOR>();
		desc.platformVersion = platform.getInfo<CL_PLATFORM_VERSION>();
		desc.deviceName = odevDesc->GetName();
		switch (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_TYPE>()) {
			case CL_DEVICE_TYPE_CPU:
				desc.deviceType = "CPU";
				break;
			case CL_DEVICE_TYPE_GPU:
				desc.deviceType = "GPU";
				break;
			case CL_DEVICE_TYPE_ACCELERATOR:
				desc.deviceType = "ACCELERATOR";
				break;
			default:
				desc.deviceType = "UNKNOWN";
				break;
		}
		desc.units = odevDesc->GetComputeUnits();
		desc.clock = odevDesc->GetOCLDevice().getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
		desc.globalMem = odevDesc->GetMaxMemory() / 1024;
		desc.localMem = odevDesc->GetOCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / 1024;
		desc.constantMem = odevDesc->GetOCLDevice().getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() / 1024;

		descs.push_back(desc);
	}

	return descs;
}

DeviceListModel::DeviceListModel(const vector<BenchmarkDeviceDescription> &descs) :
			QAbstractItemModel() {
	rootItem = new HardwareTreeItem("Devices");

#ifndef LUXRAYS_DISABLE_OPENCL
	for (size_t i = 0; i < descs.size(); ++i) {
		const BenchmarkDeviceDescription &desc = descs[i];

		stringstream ss;
		ss << desc.deviceName << " [" << desc.deviceType << ", " <<
				desc.units << ", " << desc.clock << " MHz]";

		HardwareTreeItem *newNode = new HardwareTreeItem(ss.str().c_str());
		rootItem->appendChild(newNode);
	}
#endif
}

DeviceListModel::~DeviceListModel() {
	delete rootItem;
}

int DeviceListModel::columnCount(const QModelIndex &parent) const {
	if (parent.isValid())
		return static_cast<HardwareTreeItem *>(parent.internalPointer())->columnCount();
	else
		return rootItem->columnCount();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		return item->data(index.column());
	} else if (role == Qt::ForegroundRole) {
		return QVariant(QColor(Qt::blue));
	} else
		return QVariant();
}

QModelIndex DeviceListModel::index(int row, int column, const QModelIndex &parent) const {
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	HardwareTreeItem *parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

	HardwareTreeItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	else
		return QModelIndex();
}

QModelIndex DeviceListModel::parent(const QModelIndex &index) const {
	if (!index.isValid())
		return QModelIndex();

	HardwareTreeItem *childItem = static_cast<HardwareTreeItem*>(index.internalPointer());
	HardwareTreeItem *parentItem = childItem->parent();

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

int DeviceListModel::rowCount(const QModelIndex &parent) const {
	HardwareTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

	return parentItem->childCount();
}

Qt::ItemFlags DeviceListModel::flags(const QModelIndex &index) const {
	return 0;
}
