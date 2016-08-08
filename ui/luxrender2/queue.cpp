/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QModelIndex>
#include <QTextStream>

#include "queue.hxx"

const QString Queue::defaultGroupName = "Default Queue Group";

Queue::Queue(boost::function<void(const QPersistentModelIndex&)> startRenderingCallback, boost::function<void()> stopRenderingCallback)
	: QStandardItemModel(), startRenderingCallback(startRenderingCallback), stopRenderingCallback(stopRenderingCallback),
	  restartWhenFinished(false), rendering(false)
{
	init();
}

QPersistentModelIndex Queue::addFile(const QString& filename)
{
	QFileInfo fi(filename);

	if (fi.suffix() == "lxq")
		return addLxqFile(fi.canonicalFilePath());
	else if (fi.suffix() == "lxs")
		return addLxsFile(fi.canonicalFilePath());
	else if (filename == "-")
		return addLxsFile("-");

	return QModelIndex();
}

QPersistentModelIndex Queue::addGroup(const QString& groupName)
{
	QList<QStandardItem*> queueList = findItems(groupName);

	if (queueList.size())
		return QModelIndex();

	return addGroup(new QStandardItem(groupName));
}

void Queue::remove(const QPersistentModelIndex& index)
{
	if (index.isValid())
	{
		QPersistentModelIndex nextIndex;
		if (currentSceneIndex.isValid() && (currentSceneIndex == index || currentSceneIndex.parent() == index))
		{
			stopRendering();
			nextIndex = getNextScene();
		}

		if (index.parent() == invisibleRootItem()->index())
			removeGroup(index);
		else
			removeScene(index);

		if (nextIndex.isValid())
			renderScene(nextIndex);
	}
}

void Queue::clear()
{
	stopRendering();
	QStandardItemModel::clear();
	init();
}

bool Queue::isRendering()
{
	return rendering;
}

bool Queue::renderScene(const QPersistentModelIndex& sceneIndex)
{
	stopRendering();

	if (sceneIndex.isValid())
	{
		rendering = true;
		currentSceneIndex = sceneIndex;
		incrementPasses(currentSceneIndex);
		setStatus(currentSceneIndex, "Rendering");
		startRenderingCallback(sceneIndex);
		return true;
	}

	return false;
}

bool Queue::renderNextScene()
{
	QPersistentModelIndex index = getNextScene();
	if (index.isValid())
		return renderScene(index);

	stopRendering(false);

	return false;
}

bool Queue::getRestartWhenFinished() const
{
	return restartWhenFinished;
}

void Queue::setRestartWhenFinished(bool restart)
{
	restartWhenFinished = restart;
}

int Queue::getSceneCount() const
{
	int count = 0;

	for (int i = 0; i < rowCount(); ++i)
		count += item(i)->rowCount();

	return count;
}

QPersistentModelIndex Queue::getCurrentScene() const
{
	return currentSceneIndex;
}

QString Queue::getFilename(const QPersistentModelIndex& index)
{
	return itemFromIndex(index)->data().toString();
}

QString Queue::getFlmFilename(const QPersistentModelIndex& index)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
		return itemFromIndex(index)->parent()->child(index.row(), COLUMN_FLMFILENAME)->data().toString();
	return QString();
}

QString Queue::getStatus(const QPersistentModelIndex& index)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
		return itemFromIndex(index)->parent()->child(index.row(), COLUMN_STATUS)->text();
	return QString();
}

unsigned int Queue::getPasses(const QPersistentModelIndex& index)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
		return itemFromIndex(index)->parent()->child(index.row(), COLUMN_PASSES)->data().toUInt();
	return 0;
}

void Queue::setFlmFilename(const QPersistentModelIndex& index, const QString& flmFilename)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
	{
		QFileInfo fi(flmFilename);
		QStandardItem* flm = itemFromIndex(index)->parent()->child(index.row(), COLUMN_FLMFILENAME);

		flm->setText(QDir::toNativeSeparators(fi.canonicalFilePath()));
		flm->setData(fi.canonicalFilePath());
	}
}
void Queue::setProgress(const QPersistentModelIndex& index, const QString& text)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
		itemFromIndex(index)->parent()->child(index.row(), COLUMN_PROGRESS)->setText(text);
}

void Queue::init()
{
	defaultGroupIndex = addGroup(defaultGroupName);
	setHorizontalHeaderLabels(QStringList() << "Name" << "FLM" << "Status" << "Progress" << "Pass #");
}

void Queue::stopRendering(bool callback)
{
	if (rendering && currentSceneIndex.isValid())
		setStatus(currentSceneIndex, "Completed " + QDateTime::currentDateTime().toString(Qt::DefaultLocaleShortDate));

	if (callback)
		stopRenderingCallback();

	rendering = false;
}

QPersistentModelIndex Queue::addLxqFile(const QString& lxqFilename)
{
	if (lxqFilename.isEmpty())
		return QModelIndex();

	QFileInfo fi(lxqFilename);

	for (int i = 0; i < rowCount(); ++i)
		if(getFilename(this->index(i, 0)) == fi.canonicalFilePath())
			return QModelIndex();

	QStandardItem* group = new QStandardItem(QDir::toNativeSeparators(fi.canonicalFilePath()));
	group->setData(fi.canonicalFilePath());

	QFile file(fi.canonicalFilePath());
	if (file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QTextStream in(&file);
		while (!in.atEnd())
			addLxsFile(in.readLine(), group, fi.canonicalPath());
	}

	if (!group->rowCount())
	{
		delete group;
		return QModelIndex();
	}

	return addGroup(group);
}

QPersistentModelIndex Queue::addLxsFile(const QString& lxsFilename, QStandardItem* group, const QString& directory)
{
	QFileInfo fi(directory, lxsFilename);

	if (group == NULL)
		group = itemFromIndex(getDefaultGroup());

	for (int i = 0; i < group->rowCount(); ++i)
		if (group->child(i)->data().toString() == fi.canonicalFilePath())
			return QModelIndex();

	QStandardItem* filename;
	if (lxsFilename == "-")
	{
		filename = new QStandardItem("Piped Scene");
		filename->setData("-");
	}
	else
	{
		filename = new QStandardItem(QDir::toNativeSeparators(fi.canonicalFilePath()));
		filename->setData(fi.canonicalFilePath());
	}
	QStandardItem* flmFilename = new QStandardItem();
	QStandardItem* status = new QStandardItem("Pending");
	QStandardItem* progress = new QStandardItem("Unknown");
	QStandardItem* passes = new QStandardItem(QString().setNum(0));

	group->appendRow(QList<QStandardItem*>() << filename << flmFilename << status << progress << passes);

	for (int i = 0; i < group->columnCount(); ++i)
		group->child(filename->row(), i)->setEditable(false);

	return indexFromItem(filename);
}

QPersistentModelIndex Queue::addGroup(QStandardItem* group)
{
	group->setEditable(false);
	appendRow(group);
	return indexFromItem(group);
}

void Queue::removeGroup(const QPersistentModelIndex& groupIndex)
{
	if (groupIndex.isValid() && groupIndex.parent() == invisibleRootItem()->index())
	{
		if (groupIndex == defaultGroupIndex)
		{
			removeRow(groupIndex.row());
			insertRow(0, new QStandardItem(defaultGroupName));
			item(0)->setEditable(false);
			defaultGroupIndex = item(0)->index();
		}
		else
			removeRow(groupIndex.row());
	}
}

void Queue::removeScene(const QPersistentModelIndex& sceneIndex)
{
	if (sceneIndex.isValid() && sceneIndex.parent() != invisibleRootItem()->index())
	{
		QPersistentModelIndex queueIndex = sceneIndex.parent();

		QStandardItem* group = itemFromIndex(queueIndex);
		group->removeRow(sceneIndex.row());

		if (!group->hasChildren())
			removeGroup(queueIndex);
	}
}

QPersistentModelIndex Queue::getNextScene() const
{
	int sceneIndex = currentSceneIndex.isValid() ? currentSceneIndex.row() + 1 : 0;
	int groupIndex = currentSceneIndex.isValid() ? currentSceneIndex.parent().row() : 0;

	do
	{
		for (int i = groupIndex; i < rowCount(); ++i)
		{
			for (int j = sceneIndex; j < item(i)->rowCount(); ++j)
				return item(i)->child(j)->index();
			sceneIndex = 0;
		}
		groupIndex = 0;
	}
	while (restartWhenFinished && getSceneCount() != 0);

	return QModelIndex();
}

QPersistentModelIndex Queue::getDefaultGroup() const
{
	return defaultGroupIndex;
}

void Queue::setStatus(const QPersistentModelIndex& index, const QString& status)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
		itemFromIndex(index.parent().child(index.row(), COLUMN_STATUS))->setText(status);
}

void Queue::incrementPasses(const QPersistentModelIndex& index)
{
	if (index.isValid() && index.parent() != invisibleRootItem()->index())
	{
		QStandardItem* passes = itemFromIndex(index)->parent()->child(index.row(), COLUMN_PASSES);
		passes->setText(QString().setNum(passes->text().toUInt() + 1));
	}
}
