#line 2 "filter_types.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// Frame buffer data types
//------------------------------------------------------------------------------

typedef struct {
	Spectrum c;
	float count;
} Pixel;

typedef struct {
	float alpha;
} AlphaPixel;

//------------------------------------------------------------------------------
// Filter data types
//------------------------------------------------------------------------------

// I need only the filter width/height so exporting all the following
// information would be redundant.

/*typedef enum {
	FILTER_NONE, FILTER_BOX, FILTER_GAUSSIAN, FILTER_MITCHELL, FILTER_MITCHELL_SS,
	FILTER_BLACKMANHARRIS, FILTER_SINC,
} FilterType;

typedef struct {
	FilterType type;

	union {
		// Nothing to store for NONE filter
		struct {
			float widthX, widthY;
		} box;
		struct {
			float widthX, widthY;
			float alpha;
		} gaussian;
		struct {
			float widthX, widthY;
			float B, C;
		} mitchell;
		struct {
			float widthX, widthY;
			float B, C, a0, a1;
		} mitchellss;
		struct {
			float widthX, widthY;
		} blackmanharris;
	};
} Filter;*/

typedef struct {
	float widthX, widthY;
} Filter;
