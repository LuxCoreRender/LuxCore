/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_EDITACTION_H
#define	_SLG_EDITACTION_H

#include <set>

#include "slg/slg.h"

namespace slg {

typedef enum {
	CAMERA_EDIT         = 1 << 0, // Use this for any Camera parameter editing
	GEOMETRY_EDIT       = 1 << 1, // Use this for any DataSet related editing
	GEOMETRY_TRANS_EDIT = 1 << 2, // Use this for any instance transformation related editing
	MATERIALS_EDIT      = 1 << 3, // Use this for any Material related editing
	MATERIAL_TYPES_EDIT = 1 << 4, // Use this if the kind of materials used changes
	LIGHTS_EDIT         = 1 << 5, // Use this for any Light related editing
	LIGHT_TYPES_EDIT    = 1 << 6, // Use this if the kind of lights used changes
	IMAGEMAPS_EDIT		= 1 << 7, // Use this for any ImageMaps related editing
	DENSITYGRIDS_EDIT	= 1 << 8  // Use this for any DensityGrids related editing
	
} EditAction;

class EditActionList {
public:
	EditActionList() { actions = 0; };

	void Reset() { actions = 0; }
	void AddAction(const EditAction a) { actions |= a; };
	void AddAllAction() {
		AddAction(CAMERA_EDIT);
		AddAction(GEOMETRY_EDIT);
		AddAction(GEOMETRY_TRANS_EDIT);
		AddAction(MATERIALS_EDIT);
		AddAction(MATERIAL_TYPES_EDIT);
		AddAction(LIGHTS_EDIT);
		AddAction(LIGHT_TYPES_EDIT);
		AddAction(IMAGEMAPS_EDIT);
		AddAction(DENSITYGRIDS_EDIT);
	}
	void AddActions(const u_int a) { actions |= a; };
	u_int GetActions() const { return actions; };
	bool Has(const EditAction a) const { return (actions & a) != 0; };
	bool HasOnly(const EditAction a) const { return (actions == a); };
	bool HasAnyAction() const { return actions != 0; };

	friend std::ostream &operator<<(std::ostream &os, const EditActionList &eal);

private:
	u_int actions;
};

inline std::ostream &operator<<(std::ostream &os, const EditActionList &eal) {
	os << "EditActionList[";

	bool addSeparator = false;
#define SHOW_SEP\
		if (addSeparator)\
			os << ", ";\
		addSeparator = true;

	if (eal.Has(CAMERA_EDIT)) {
        SHOW_SEP;
		os << "CAMERA_EDIT";
    }
	if (eal.Has(GEOMETRY_EDIT)) {
        SHOW_SEP;
		os << "GEOMETRY_EDIT";
    }
	if (eal.Has(GEOMETRY_TRANS_EDIT)) {
        SHOW_SEP;
		os << "GEOMETRY_TRANS_EDIT";
    }
	if (eal.Has(MATERIALS_EDIT)) {
        SHOW_SEP;
		os << "MATERIALS_EDIT";
    }
	if (eal.Has(MATERIAL_TYPES_EDIT)) {
        SHOW_SEP;
		os << "MATERIAL_TYPES_EDIT";
    }
	if (eal.Has(LIGHTS_EDIT)) {
        SHOW_SEP;
		os << "LIGHTS_EDIT";
    }
	if (eal.Has(LIGHT_TYPES_EDIT)) {
        SHOW_SEP;
		os << "LIGHT_TYPES_EDIT";
    }
	if (eal.Has(IMAGEMAPS_EDIT)) {
		SHOW_SEP;
		os << "IMAGEMAPS_EDIT";
	}
	if (eal.Has(DENSITYGRIDS_EDIT)) {
		SHOW_SEP;
		os << "DENSITYGRIDS_EDIT";
	}

	os << "]";

#undef SHOW_SEP

	return os;
}

}

#endif	/* _SLG_SLG_EDITACTION_H */
