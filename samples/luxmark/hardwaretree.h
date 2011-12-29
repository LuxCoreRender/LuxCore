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

#ifndef _HARDWARETREE_H
#define _HARDWARETREE_H

#include <QVariant>
#include <QModelIndex>
#include <QAbstractItemModel>

#include "luxrays/luxrays.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// Hardware tree view
//------------------------------------------------------------------------------

class HardwareTreeItem {
public:
	HardwareTreeItem(const QVariant &data, bool chkable = false, HardwareTreeItem *parent = 0);
	~HardwareTreeItem();

	void appendChild(HardwareTreeItem *child);

	HardwareTreeItem *child(int row);
	int childCount() const;
	int columnCount() const;
	QVariant data(int column) const;
	int row() const;
	HardwareTreeItem *parent();

	bool isCheckable() const { return checkable; }

private:
	QList<HardwareTreeItem *> childItems;
	QVariant itemData;
	HardwareTreeItem *parentItem;
	bool checkable;
};

class HardwareTreeModel : public QAbstractItemModel {
	Q_OBJECT

public:
	HardwareTreeModel(const vector<DeviceDescription *> &devDescs);
	~HardwareTreeModel();

	QVariant data(const QModelIndex &index, int role) const;
	QModelIndex index(int row, int column,
			const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

private:
	HardwareTreeItem *rootItem;
};

#endif // _HARDWARETREE_H
