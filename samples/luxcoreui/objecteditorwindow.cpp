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

#include <iostream>
#include <boost/format.hpp>

#include "luxcoreapp.h"
#include "objecteditorwindow.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// ObjectEditorWindow
//------------------------------------------------------------------------------

void ObjectEditorWindow::RefreshGUIProperties() {
	// Refresh objectProps
	RefreshObjectProperties(objectGUIProps);
	modifiedGUIProps = false;
}

void ObjectEditorWindow::ParseGUIProperties() {
	ParseObjectProperties(objectGUIProps);

	// It will be filled at the next iteration
	objectGUIProps.Clear();
}

void ObjectEditorWindow::RefreshEditorProperties() {
	// Refresh objectProps
	RefreshObjectProperties(objectEditorProps);
	modifiedEditorProps = false;

	strncpy(advancedEditorText,
			objectEditorProps.ToString().c_str(),
			LA_ARRAYSIZE(advancedEditorText));
	modifiedEditorProps = false;
}

void ObjectEditorWindow::ParseEditorProperties() {
	ParseObjectProperties(objectEditorProps);

	// It will be filled at the next iteration
	objectEditorProps.Clear();
}

void ObjectEditorWindow::Draw() {
	if (!opened)
		return;

	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		// Check if I have fill object properties
		if (!objectGUIProps.GetSize() || !objectEditorProps.GetSize()) {
			// object properties are empty
			RefreshGUIProperties();
			RefreshEditorProperties();
		}

		//----------------------------------------------------------------------
		// Object properties
		//----------------------------------------------------------------------

		if (ImGui::CollapsingHeader("Sampler properties", NULL, true, true)) {
			ImGui::PushID("Sampler properties");


			//------------------------------------------------------------------
			// Object GUI
			//------------------------------------------------------------------

			if (DrawObjectGUI(objectGUIProps, modifiedGUIProps))
				ParseGUIProperties();

			//------------------------------------------------------------------
			// Refresh + Parse buttons
			//------------------------------------------------------------------

			if (ImGui::Button("Refresh"))
				RefreshGUIProperties();

			ImGui::SameLine();

			if (ImGui::Button(modifiedGUIProps ? "Parse (*)" : "Parse"))
				ParseGUIProperties();

			ImGui::PopID();
		}

		//----------------------------------------------------------------------
		// Advance editing
		//----------------------------------------------------------------------

		if (ImGui::CollapsingHeader("Advance editor", NULL, true, false)) {
			ImGui::PushID("Advance editor");

			//------------------------------------------------------------------
			// Refresh + Parse buttons
			//------------------------------------------------------------------

			if (ImGui::InputTextMultiline("Properties editor", advancedEditorText, LA_ARRAYSIZE(advancedEditorText)))
				modifiedEditorProps = true;

			//------------------------------------------------------------------
			// Refresh + Parse
			//------------------------------------------------------------------

			if (ImGui::Button("Refresh"))
				RefreshEditorProperties();

			ImGui::SameLine();

			if (ImGui::Button(modifiedEditorProps ? "Parse (*)" : "Parse")) {
				objectEditorProps.Clear();
				objectEditorProps.SetFromString(string(advancedEditorText));

				ParseEditorProperties();
			}

			ImGui::PopID();
		}
	} else {
		// Clear the properties when the window is first closed so they are
		// re-initialized the next time.
		if (objectGUIProps.GetSize())
			objectGUIProps.Clear();
		if (objectEditorProps.GetSize())
			objectEditorProps.Clear();
	}
	ImGui::End();
}
