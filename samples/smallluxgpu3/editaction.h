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

#include "slg.h"

namespace slg {

enum EditAction {
	FILM_EDIT, // Use this for image Film resize
	CAMERA_EDIT, // Use this for any Camera parameter editing
	GEOMETRY_EDIT, // Use this for any DataSet related editing
	INSTANCE_TRANS_EDIT, // Use this for any instance transformation related editing
	MATERIALS_EDIT, // Use this for any Material related editing
	MATERIAL_TYPES_EDIT, // Use this if the kind of materials changes
	AREALIGHTS_EDIT, // Use this for any AreaLight related editing
	INFINITELIGHT_EDIT, // Use this for any InfiniteLight related editing
	SUNLIGHT_EDIT, // Use this for any SunLight related editing
	SKYLIGHT_EDIT, // Use this for any SkyLight related editing
	TEXTUREMAPS_EDIT // Use this for any TextureMaps related editing
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
		AddAction(INSTANCE_TRANS_EDIT);
		AddAction(MATERIALS_EDIT);
		AddAction(MATERIAL_TYPES_EDIT);
		AddAction(AREALIGHTS_EDIT);
		AddAction(INFINITELIGHT_EDIT);
		AddAction(SUNLIGHT_EDIT);
		AddAction(SKYLIGHT_EDIT);
		AddAction(TEXTUREMAPS_EDIT);
	}
	bool Has(const EditAction a) const { return (actions.find(a) != actions.end()); };
	size_t Size() const { return actions.size(); };

	friend std::ostream &operator<<(std::ostream &os, const EditActionList &eal);
private:
	std::set<EditAction> actions;
};

inline std::ostream &operator<<(std::ostream &os, const EditActionList &eal) {
	os << "EditActionList[";

	bool addSeparetor = false;
	for (std::set<EditAction>::const_iterator it = eal.actions.begin(); it!=eal.actions.end(); ++it) {
		if (addSeparetor)
			os << ", ";

		switch (*it) {
			case FILM_EDIT:
				os << "FILM_EDIT";
				break;
			case CAMERA_EDIT:
				os << "CAMERA_EDIT";
				break;
			case GEOMETRY_EDIT:
				os << "GEOMETRY_EDIT";
				break;
			case INSTANCE_TRANS_EDIT:
				os << "INSTANCE_TRANS_EDIT";
				break;
			case MATERIALS_EDIT:
				os << "MATERIALS_EDIT";
				break;
			case MATERIAL_TYPES_EDIT:
				os << "MATERIAL_TYPES_EDIT";
				break;
			case AREALIGHTS_EDIT:
				os << "AREALIGHTS_EDIT";
				break;
			case INFINITELIGHT_EDIT:
				os << "INFINITELIGHT_EDIT";
				break;
			case SUNLIGHT_EDIT:
				os << "SUNLIGHT_EDIT";
				break;
			case SKYLIGHT_EDIT:
				os << "SKYLIGHT_EDIT";
				break;
			case TEXTUREMAPS_EDIT:
				os << "TEXTUREMAPS_EDIT";
				break;
			default:
				os << "UNKNOWN[" << *it << "]";
				break;
		}

		addSeparetor = true;
	}

	os << "]";

	return os;
}

}

#endif	/* _SLG_EDITACTION_H */
