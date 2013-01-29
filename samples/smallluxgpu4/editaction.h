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

typedef enum {
	FILM_EDIT           = 0x01,  // Use this for image Film resize
	CAMERA_EDIT         = 0x02,  // Use this for any Camera parameter editing
	GEOMETRY_EDIT       = 0x04,  // Use this for any DataSet related editing
	INSTANCE_TRANS_EDIT = 0x08,  // Use this for any instance transformation related editing
	MATERIALS_EDIT      = 0x10,  // Use this for any Material related editing
	MATERIAL_TYPES_EDIT = 0x20,  // Use this if the kind of materials changes
	AREALIGHTS_EDIT     = 0x40,  // Use this for any AreaLight related editing
	INFINITELIGHT_EDIT  = 0x80,  // Use this for any InfiniteLight related editing
	SUNLIGHT_EDIT       = 0x100, // Use this for any SunLight related editing
	SKYLIGHT_EDIT       = 0x200, // Use this for any SkyLight related editing
	IMAGEMAPS_EDIT      = 0x400  // Use this for any ImageMaps related editing
} EditAction;

class EditActionList {
public:
	EditActionList() { actions = 0; };

	void Reset() { actions = 0; }
	void AddAction(const EditAction a) { actions |= a; };
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
		AddAction(IMAGEMAPS_EDIT);
	}
	void AddActions(const u_int a) { actions |= a; };
	u_int GetActions() const { return actions; };
	bool Has(const EditAction a) const { return (actions & a); };
	bool HasAnyAction() const { return actions; };

	friend std::ostream &operator<<(std::ostream &os, const EditActionList &eal);

private:
	u_int actions;
};

inline std::ostream &operator<<(std::ostream &os, const EditActionList &eal) {
	os << "EditActionList[";

	bool addSeparetor = false;
#define SHOW_SEP\
		if (addSeparetor)\
			os << ", ";\
		addSeparetor = true;

	if (eal.Has(FILM_EDIT)) {
        SHOW_SEP;
		os << "FILM_EDIT";
    }
	if (eal.Has(CAMERA_EDIT)) {
        SHOW_SEP;
		os << "CAMERA_EDIT";
    }
	if (eal.Has(GEOMETRY_EDIT)) {
        SHOW_SEP;
		os << "GEOMETRY_EDIT";
    }
	if (eal.Has(INSTANCE_TRANS_EDIT)) {
        SHOW_SEP;
		os << "INSTANCE_TRANS_EDIT";
    }
	if (eal.Has(MATERIALS_EDIT)) {
        SHOW_SEP;
		os << "MATERIALS_EDIT";
    }
	if (eal.Has(MATERIAL_TYPES_EDIT)) {
        SHOW_SEP;
		os << "MATERIAL_TYPES_EDIT";
    }
	if (eal.Has(AREALIGHTS_EDIT)) {
        SHOW_SEP;
		os << "AREALIGHTS_EDIT";
    }
	if (eal.Has(INFINITELIGHT_EDIT)) {
        SHOW_SEP;
		os << "INFINITELIGHT_EDIT";
    }
	if (eal.Has(SUNLIGHT_EDIT)) {
        SHOW_SEP;
		os << "SUNLIGHT_EDIT";
    }
	if (eal.Has(SKYLIGHT_EDIT)) {
        SHOW_SEP;
		os << "SKYLIGHT_EDIT";
    }
	if (eal.Has(IMAGEMAPS_EDIT)) {
        SHOW_SEP;
		os << "IMAGEMAPS_EDIT";
	}

	os << "]";

#undef SHOW_SEP

	return os;
}

}

#endif	/* _SLG_EDITACTION_H */
