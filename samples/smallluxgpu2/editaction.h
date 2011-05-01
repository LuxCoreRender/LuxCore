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

#ifndef _EDITACTION_H
#define	_EDITACTION_H

#include <set>

#include "smalllux.h"

enum EditAction {
	FILM_EDIT, // Use this for image Film resize
	CAMERA_EDIT, // Use this for any Camera parameter editing
	GEOMETRY_EDIT, // Use this for any DataSet related editing
	MATERIALS_EDIT, // Use this for any Material related editing
	MATERIAL_TYPES_EDIT, // Use this if the kind of materials changes
	AREALIGHTS_EDIT, // Use this for any AreaLight related editing
	INFINITELIGHT_EDIT, // Use this for any InfiniteLight related editing
	SUNLIGHT_EDIT // Use this for any SunLight related editing
};

class EditActionList {
public:
	EditActionList() { };
	~EditActionList() { };

	void Reset() { actions.clear(); }
	void AddAction(const EditAction a) { actions.insert(a); };
	void AddAllAction() {
		AddAction(FILM_EDIT);
		AddAction(CAMERA_EDIT);
		AddAction(GEOMETRY_EDIT);
		AddAction(MATERIALS_EDIT);
		AddAction(MATERIAL_TYPES_EDIT);
		AddAction(AREALIGHTS_EDIT);
		AddAction(INFINITELIGHT_EDIT);
		AddAction(SUNLIGHT_EDIT);
	}
	bool Has(const EditAction a) const { return (actions.find(a) != actions.end()); };
	size_t Size() const { return actions.size(); };

private:
	set<EditAction> actions;
};

#endif	/* _EDITACTION_H */
