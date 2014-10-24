/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>

#include "slg/core/sphericalfunction/photometricdataies.h"

using namespace std;
using namespace slg;

PhotometricDataIES::PhotometricDataIES() { 
	Reset(); 
}

PhotometricDataIES::PhotometricDataIES(const char *sFileName) {
	Reset();

	Load(sFileName);
}

PhotometricDataIES::~PhotometricDataIES() {
	if(m_fsIES.is_open())
		m_fsIES.close();
}

void PhotometricDataIES::Reset() {
	m_bValid 	= false;
	m_Version 	= "NONE";

	m_Keywords.clear();
	m_VerticalAngles.clear(); 
	m_HorizontalAngles.clear(); 
	m_CandelaValues.clear();

	if(m_fsIES.is_open())
		m_fsIES.close();
	m_fsIES.clear();
}

//------------------------------------------------------------------------------

bool PhotometricDataIES::Load(const char *sFileName) {
	bool ok = PrivateLoad(sFileName);
	if(m_fsIES.is_open())
		m_fsIES.close();
	m_fsIES.clear();
	return ok;
}

bool PhotometricDataIES::PrivateLoad(const char *sFileName) {
	Reset();

	m_fsIES.open(sFileName); 

	if (!m_fsIES.good()) return false;

	string templine(256, 0);

	//--------------------------------------------------------------------------
	// Check for valid IES file and get version
	//--------------------------------------------------------------------------

	ReadLine(templine);

	const size_t vpos = templine.find_first_of("IESNA");

	if (vpos != string::npos)
		m_Version = templine.substr(templine.find_first_of(":") + 1);
	else
		return false;

	//--------------------------------------------------------------------------

	if (!BuildKeywordList())
		return false;
	if (!BuildLightData())
		return false;

	//--------------------------------------------------------------------------

	m_bValid = true;

	return true;
}

//------------------------------------------------------------------------------

bool PhotometricDataIES::BuildKeywordList() {
	if (!m_fsIES.good())
		return false;

	m_Keywords.clear();

	string templine(256, 0);

	//--------------------------------------------------------------------------
	// Find the start of the keyword section...
	//--------------------------------------------------------------------------

	m_fsIES.seekg(0);
	ReadLine(templine);

	if (templine.find("IESNA") == string::npos)
		return false; // Invalid file.
	
	//--------------------------------------------------------------------------
	// Build map from the keywords
	//--------------------------------------------------------------------------

	string sKey, sVal;

	while(m_fsIES.good()) {
		ReadLine(templine);

		if (templine.find("TILT") != string::npos)
			break;

		const size_t kwStartPos = templine.find_first_of("[");
		const size_t kwEndPos = templine.find_first_of("]");

		if(kwStartPos != string::npos && 
			kwEndPos != string::npos && 
			kwEndPos > kwStartPos) {
			string sTemp = templine.substr(kwStartPos + 1, (kwEndPos - kwStartPos) - 1); 

			if (templine.find("MORE") == string::npos && !sTemp.empty()) {
				if (!sVal.empty())
				    m_Keywords.insert(pair<string,string>(sKey,sVal));

				sKey = sTemp;
				sVal = templine.substr(kwEndPos + 1, templine.size() - (kwEndPos + 1));

			} else
				sVal += " " + templine.substr(kwEndPos + 1, templine.size() - (kwEndPos + 1));
		}
	}

	if (!m_fsIES.good())
		return false;
	
    m_Keywords.insert(pair<string,string>(sKey,sVal));

	return true;
}

//------------------------------------------------------------------------------

void PhotometricDataIES::BuildDataLine(stringstream &ssLine, unsigned int nDoubles, vector <double> &vLine) {
	double dTemp = 0.0;

	unsigned int count = 0;

	while(count < nDoubles && ssLine.good()) {
		ssLine >> dTemp;

		vLine.push_back(dTemp); 
				
		count++;
	}
}

bool PhotometricDataIES::BuildLightData() {
	if (!m_fsIES.good()) return false;

	string templine(1024, 0);

	//--------------------------------------------------------------------------
	// Find the start of the light data...
	//--------------------------------------------------------------------------

	m_fsIES.seekg(0);

	while(m_fsIES.good()) {
		ReadLine(templine);

		if (templine.find("TILT") != string::npos)
			break;
	}
	
	// Only supporting TILT=NONE right now
	if (templine.find("TILT=NONE") == string::npos)
		return false;

	//--------------------------------------------------------------------------
	// Clean the data fields, some IES files use comma's in data 
	// fields which breaks ifstreams >> op. 
	//--------------------------------------------------------------------------

	int spos = (int)m_fsIES.tellg(); 

	m_fsIES.seekg(0, ios_base::end);

	int epos = (int)m_fsIES.tellg() - spos; 

	m_fsIES.seekg(spos);
	
	string strIES(epos, 0);

	int nChar;
	int n1 = 0;

	for (int n = 0; n < epos; n++) {
		if (m_fsIES.eof()) break;

		nChar = m_fsIES.get();

		if (nChar != ',')  {
			strIES[n1] = (char)nChar; 
			n1++;
		}
	}

	m_fsIES.close(); // done with the file.

	strIES.resize(n1);

	stringstream ssIES(strIES, stringstream::in);

	ssIES.seekg(0, ios_base::beg);

	//--------------------------------------------------------------------------
	// Read first two lines containing light vars.
	//--------------------------------------------------------------------------

	ssIES >> m_NumberOfLamps;
	ssIES >> m_LumensPerLamp;
	ssIES >> m_CandelaMultiplier;
	ssIES >> m_NumberOfVerticalAngles;
	ssIES >> m_NumberOfHorizontalAngles;
	unsigned int photometricTypeInt;
	ssIES >> photometricTypeInt;
	m_PhotometricType = PhotometricType(photometricTypeInt);
	ssIES >> m_UnitsType;
	ssIES >> m_LuminaireWidth;
	ssIES >> m_LuminaireLength;
	ssIES >> m_LuminaireHeight;

	ssIES >> BallastFactor;
	ssIES >> BallastLampPhotometricFactor;
	ssIES >> InputWatts;

	//--------------------------------------------------------------------------
	// Read angle data
	//--------------------------------------------------------------------------

	m_VerticalAngles.clear(); 
	BuildDataLine(ssIES, m_NumberOfVerticalAngles, m_VerticalAngles);

	m_HorizontalAngles.clear(); 
	BuildDataLine(ssIES, m_NumberOfHorizontalAngles, m_HorizontalAngles);
	
	m_CandelaValues.clear();

	vector <double> vTemp;

	for (unsigned int n1 = 0; n1 < m_NumberOfHorizontalAngles; n1++) {
		vTemp.clear();

		BuildDataLine(ssIES, m_NumberOfVerticalAngles, vTemp);

	    m_CandelaValues.push_back(vTemp);
	}

	return true;
}
