#line 2 "imagemapdesc_types.cl"

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

#define IMAGEMAPOBJ_NULL 0xffffffffffffffffull

typedef unsigned long long ImageMapObj;

typedef enum {
	BYTE, HALF, FLOAT
} ImageMapStorageType;

typedef enum {
	WRAP_REPEAT,
	WRAP_BLACK,
	WRAP_WHITE,
	WRAP_CLAMP
} ImageWrapType;

typedef struct {
	ImageMapStorageType storageType;
	ImageWrapType wrapType;
	unsigned int channelCount, width, height;
} ImageMapDescription;
