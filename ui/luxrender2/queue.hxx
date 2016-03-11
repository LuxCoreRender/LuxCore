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

#ifndef QUEUE_H
#define QUEUE_H

#include <boost/function.hpp>

#include <QPersistentModelIndex>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>

class Queue : public QStandardItemModel
{

	Q_OBJECT
	
public:
	Queue(boost::function<void(const QPersistentModelIndex&)> functionToStartRenderingScene, boost::function<void()> functionToStopRenderingScene);
	virtual ~Queue() {}

	QPersistentModelIndex addFile(const QString& filename);
	QPersistentModelIndex addGroup(const QString& groupName);
	void remove(const QPersistentModelIndex& index);
	virtual void clear();

	bool isRendering();
	bool renderScene(const QPersistentModelIndex& sceneIndex);
	bool renderNextScene();

	bool getRestartWhenFinished() const;
	void setRestartWhenFinished(bool restart);

	int getSceneCount() const;
	QPersistentModelIndex getCurrentScene() const;

	QString getFilename(const QPersistentModelIndex& index);
	QString getFlmFilename(const QPersistentModelIndex& index);
	QString getStatus(const QPersistentModelIndex& index);
	QString getProgress(const QPersistentModelIndex& index);
	unsigned int getPasses(const QPersistentModelIndex& index);

	void setFlmFilename(const QPersistentModelIndex& index, const QString& flmFilename);
	void setProgress(const QPersistentModelIndex& index, const QString& text);

	enum Columns {
		COLUMN_LXSFILENAME,
		COLUMN_FLMFILENAME,
		COLUMN_STATUS,
		COLUMN_PROGRESS,
		COLUMN_PASSES
	};

private:
	void init();
	void stopRendering(bool callback = true);

	QPersistentModelIndex addLxqFile(const QString& lxqFilename);
	QPersistentModelIndex addLxsFile(const QString& lxsFilename, QStandardItem* group = NULL, const QString& directory = "");

	QPersistentModelIndex addGroup(QStandardItem* group);
	void removeGroup(const QPersistentModelIndex& groupIndex);
	void removeScene(const QPersistentModelIndex& sceneIndex);

	QPersistentModelIndex getNextScene() const;
	QPersistentModelIndex getDefaultGroup() const;

	void setStatus(const QPersistentModelIndex& index, const QString& status);
	void incrementPasses(const QPersistentModelIndex& index);

	boost::function<void(const QPersistentModelIndex&)> startRenderingCallback;
	boost::function<void()> stopRenderingCallback;

	QPersistentModelIndex currentSceneIndex;
	QPersistentModelIndex defaultGroupIndex;
	bool restartWhenFinished;
	bool rendering;

	static const QString defaultGroupName;
};

#endif //QUEUE_H
