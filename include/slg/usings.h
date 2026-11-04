/***************************************************************************
 * Copyright 1998-2025 by authors (see AUTHORS.txt)                        *
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

// This file is intended to gather all convenient usings (pointers, refs etc.)
// for slg classes

#pragma once

#include <memory>
#include "luxrays/usings.h"

// Tip:You'll find DECLARE_SUBTYPES definition in luxrays/usings.h


namespace slg {

DECLARE_SUBTYPES(Camera);
DECLARE_SUBTYPES(CPURenderThread);
DECLARE_SUBTYPES(EditActionList);
DECLARE_SUBTYPES(EnvLightSource);
DECLARE_SUBTYPES(EnvLightVisibilityCache);
DECLARE_SUBTYPES(Film);
DECLARE_SUBTYPES(FilmSampleSplatter);
DECLARE_SUBTYPES(Filter);
DECLARE_SUBTYPES(FilterLUT);
DECLARE_SUBTYPES(FilterLUTs);
DECLARE_SUBTYPES(FresnelTexture);
DECLARE_SUBTYPES(ImageMap);
DECLARE_SUBTYPES(ImageMapCache);
DECLARE_SUBTYPES(ImageMapStorage);
DECLARE_SUBTYPES(ImageMapTexture);
DECLARE_SUBTYPES(LightSource);
DECLARE_SUBTYPES(LightSourceDefinitions);
DECLARE_SUBTYPES(LightStrategy);
DECLARE_SUBTYPES(LightStrategyLogPower);
DECLARE_SUBTYPES(Material);
DECLARE_SUBTYPES(MaterialDefinitions);
DECLARE_SUBTYPES(RenderConfig);
DECLARE_SUBTYPES(RenderEngine);
DECLARE_SUBTYPES(RenderSession);
DECLARE_SUBTYPES(RenderState);
DECLARE_SUBTYPES(Sampler);
DECLARE_SUBTYPES(SceneObject);
DECLARE_SUBTYPES(SampleableSphericalFunction);
DECLARE_SUBTYPES(SamplerSharedData);
DECLARE_SUBTYPES(Scene);
DECLARE_SUBTYPES(SceneObjectDefinitions);
DECLARE_SUBTYPES(Shape);
DECLARE_SUBTYPES(SobolSamplerSharedData);
DECLARE_SUBTYPES(SphericalFunction);
DECLARE_SUBTYPES(Texture);
DECLARE_SUBTYPES(TextureDefinitions);
DECLARE_SUBTYPES(TextureMapping2D);
DECLARE_SUBTYPES(TextureMapping3D);
DECLARE_SUBTYPES(TriangleLight);
DECLARE_SUBTYPES(Volume);

}  // namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
