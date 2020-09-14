/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _LUXCOREAPP_OCLDEVICEWINDOW_H
#define	_LUXCOREAPP_OCLDEVICEWINDOW_H

#include "objecteditorwindow.h"

class LuxCoreApp;

class OCLDeviceWindow : public ObjectEditorWindow {
public:
	OCLDeviceWindow(LuxCoreApp *a);
	virtual ~OCLDeviceWindow() { }

private:
	luxrays::Properties GetOpenCLDeviceProperties(const luxrays::Properties &cfgProps) const;

	virtual void RefreshObjectProperties(luxrays::Properties &props);
	virtual void ParseObjectProperties(const luxrays::Properties &props);
	virtual bool DrawObjectGUI(luxrays::Properties &props, bool &modified);
};

#endif	/* _LUXCOREAPP_OCLDEVICEWINDOW_H */
