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

#ifndef _LUXRAYS_PROPERTIES_H
#define	_LUXRAYS_PROPERTIES_H

#include <map>

namespace luxrays {

class Properties {
public:
	Properties() { }
	Properties(const std::string &fileName);
	~Properties() { }

	void LoadFile(const std::string &fileName);

	std::vector<std::string> GetAllKeys() const;

	std::string GetString(const std::string propName, const std::string defaultValue) const;
	int GetInt(const std::string propName, const int defaultValue) const;
	float GetFloat(const std::string propName, const float defaultValue) const;

	std::vector<int> GetIntVector(const std::string propName, const std::string &defaultValue) const;
	std::vector<float> GetFloatVector(const std::string propName, const std::string &defaultValue) const;

	static std::vector<int> ConvertToIntVector(const std::string &values);
	static std::vector<float> ConvertToFloatVector(const std::string &values);

private:
	std::map<std::string, std::string> props;
};

}

#endif	/* _LUXRAYS_PROPERTIES_H */
