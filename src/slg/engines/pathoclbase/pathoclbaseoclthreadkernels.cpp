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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/sysctl.h>
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread kernels related methods
//------------------------------------------------------------------------------

void PathOCLBaseOCLRenderThread::CompileKernel(cl::Program *program, cl::Kernel **kernel,
		size_t *workgroupSize, const string &name) {
	delete *kernel;
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	*kernel = new cl::Kernel(*program, name.c_str());

	if (intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
		*workgroupSize = intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize();
	else {
		cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
		(*kernel)->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, workgroupSize);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workgroupSize);
	}
}

string PathOCLBaseOCLRenderThread::SamplerKernelDefinitions() {
	if ((renderEngine->oclSampler->type == slg::ocl::SOBOL) ||
			((renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) && (renderEngine->GetType() == TILEPATHOCL))) {
		// Generate the Sobol vectors
		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Sobol table size: " << (sampleDimensions * SOBOL_BITS * sizeof(u_int)) / 1024 << "Kbytes");
		u_int *directions = new u_int[sampleDimensions * SOBOL_BITS];

		SobolSequence::GenerateDirectionVectors(directions, sampleDimensions);

		stringstream sobolTableSS;
		sobolTableSS << "#line 2 \"Sobol Table in pathoclthreadstatebase.cpp\"\n";
		sobolTableSS << "__constant uint SOBOL_DIRECTIONS[" << sampleDimensions * SOBOL_BITS << "] = {\n";
		for (u_int i = 0; i < sampleDimensions * SOBOL_BITS; ++i) {
			if (i != 0)
				sobolTableSS << ", ";
			sobolTableSS << directions[i] << "u";
		}
		sobolTableSS << "};\n";

		delete[] directions;

		return sobolTableSS.str();
	} else
		return "";
}

void PathOCLBaseOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	// Set #define symbols
	stringstream ssParams;
	ssParams.precision(6);
	ssParams << scientific <<
			" -D LUXRAYS_OPENCL_KERNEL" <<
			" -D SLG_OPENCL_KERNEL" <<
			" -D RENDER_ENGINE_" << RenderEngine::RenderEngineType2String(renderEngine->GetType()) <<
			" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
			" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f"
			" -D PARAM_LIGHT_WORLD_RADIUS_SCALE=" << InfiniteLightSource::LIGHT_WORLD_RADIUS_SCALE << "f"
			;

	if (cscene->hasTriangleLightWithVertexColors)
		ssParams << " -D PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR";

	switch (intersectionDevice->GetAccelerator()->GetType()) {
		case ACCEL_BVH:
			ssParams << " -D PARAM_ACCEL_BVH";
			break;
		case ACCEL_MBVH:
			ssParams << " -D PARAM_ACCEL_MBVH";
			break;
		case ACCEL_EMBREE:
			throw runtime_error("EMBREE accelerator is not supported in PathOCLBaseRenderThread::InitKernels()");
		default:
			throw runtime_error("Unknown accelerator in PathOCLBaseRenderThread::InitKernels()");
	}

	// Film related parameters

	// All thread films are supposed to have the same parameters
	const Film *threadFilm = threadFilms[0]->film;

	for (u_int i = 0; i < threadFilms[0]->channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		ssParams << " -D PARAM_FILM_RADIANCE_GROUP_" << i;
	ssParams << " -D PARAM_FILM_RADIANCE_GROUP_COUNT=" << threadFilms[0]->channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size();
	if (threadFilm->HasChannel(Film::ALPHA))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_ALPHA";
	if (threadFilm->HasChannel(Film::DEPTH))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DEPTH";
	if (threadFilm->HasChannel(Film::POSITION))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_POSITION";
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL";
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL";
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID";
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE";
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY";
	if (threadFilm->HasChannel(Film::EMISSION))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_EMISSION";
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE";
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY";
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR";
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK" <<
				" -D PARAM_FILM_MASK_MATERIAL_ID=" << threadFilm->GetMaskMaterialID(0);
	}
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK";
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK";
	if (threadFilm->HasChannel(Film::UV))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_UV";
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_RAYCOUNT";
	if (threadFilm->HasChannel(Film::BY_MATERIAL_ID)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID" <<
				" -D PARAM_FILM_BY_MATERIAL_ID=" << threadFilm->GetByMaterialID(0);
	}
	if (threadFilm->HasChannel(Film::IRRADIANCE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_IRRADIANCE";
	if (threadFilm->HasChannel(Film::OBJECT_ID))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_OBJECT_ID";
	if (threadFilm->HasChannel(Film::OBJECT_ID_MASK)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK" <<
				" -D PARAM_FILM_MASK_OBJECT_ID=" << threadFilm->GetMaskObjectID(0);
	}
	if (threadFilm->HasChannel(Film::BY_OBJECT_ID)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID" <<
				" -D PARAM_FILM_BY_OBJECT_ID=" << threadFilm->GetMaskObjectID(0);
	}
	if (threadFilm->HasChannel(Film::SAMPLECOUNT))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_SAMPLECOUNT";
	if (threadFilm->HasChannel(Film::CONVERGENCE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_CONVERGENCE";
	if (threadFilm->HasChannel(Film::MATERIAL_ID_COLOR))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR";
	if (threadFilm->HasChannel(Film::ALBEDO))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_ALBEDO";
	if (threadFilm->HasChannel(Film::AVG_SHADING_NORMAL))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL";
	if (threadFilm->HasChannel(Film::NOISE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_NOISE";

	if (threadFilm->GetDenoiser().IsEnabled())
		ssParams << " -D PARAM_FILM_DENOISER";

	if (cscene->IsTextureCompiled(CONST_FLOAT))
		ssParams << " -D PARAM_ENABLE_TEX_CONST_FLOAT";
	if (cscene->IsTextureCompiled(CONST_FLOAT3))
		ssParams << " -D PARAM_ENABLE_TEX_CONST_FLOAT3";
	if (cscene->IsTextureCompiled(IMAGEMAP))
		ssParams << " -D PARAM_ENABLE_TEX_IMAGEMAP";
	if (cscene->IsTextureCompiled(SCALE_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_SCALE";
	if (cscene->IsTextureCompiled(FRESNEL_APPROX_N))
		ssParams << " -D PARAM_ENABLE_FRESNEL_APPROX_N";
	if (cscene->IsTextureCompiled(FRESNEL_APPROX_K))
		ssParams << " -D PARAM_ENABLE_FRESNEL_APPROX_K";
	if (cscene->IsTextureCompiled(CHECKERBOARD2D))
		ssParams << " -D PARAM_ENABLE_CHECKERBOARD2D";
	if (cscene->IsTextureCompiled(CHECKERBOARD3D))
		ssParams << " -D PARAM_ENABLE_CHECKERBOARD3D";
	if (cscene->IsTextureCompiled(MIX_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_MIX";
	if (cscene->IsTextureCompiled(CLOUD_TEX))
		ssParams << " -D PARAM_ENABLE_CLOUD_TEX";
	if (cscene->IsTextureCompiled(FBM_TEX))
		ssParams << " -D PARAM_ENABLE_FBM_TEX";
	if (cscene->IsTextureCompiled(MARBLE))
		ssParams << " -D PARAM_ENABLE_MARBLE";
	if (cscene->IsTextureCompiled(DOTS))
		ssParams << " -D PARAM_ENABLE_DOTS";
	if (cscene->IsTextureCompiled(BRICK))
		ssParams << " -D PARAM_ENABLE_BRICK";
	if (cscene->IsTextureCompiled(ADD_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_ADD";
	if (cscene->IsTextureCompiled(SUBTRACT_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_SUBTRACT";
	if (cscene->IsTextureCompiled(WINDY))
		ssParams << " -D PARAM_ENABLE_WINDY";
	if (cscene->IsTextureCompiled(WRINKLED))
		ssParams << " -D PARAM_ENABLE_WRINKLED";
	if (cscene->IsTextureCompiled(BLENDER_BLEND))
		ssParams << " -D PARAM_ENABLE_BLENDER_BLEND";
 	if (cscene->IsTextureCompiled(BLENDER_CLOUDS))
 		ssParams << " -D PARAM_ENABLE_BLENDER_CLOUDS";
	if (cscene->IsTextureCompiled(BLENDER_DISTORTED_NOISE))
		ssParams << " -D PARAM_ENABLE_BLENDER_DISTORTED_NOISE";
	if (cscene->IsTextureCompiled(BLENDER_MAGIC))
		ssParams << " -D PARAM_ENABLE_BLENDER_MAGIC";
	if (cscene->IsTextureCompiled(BLENDER_MARBLE))
		ssParams << " -D PARAM_ENABLE_BLENDER_MARBLE";
	if (cscene->IsTextureCompiled(BLENDER_MUSGRAVE))
		ssParams << " -D PARAM_ENABLE_BLENDER_MUSGRAVE";
	if (cscene->IsTextureCompiled(BLENDER_NOISE))
		ssParams << " -D PARAM_ENABLE_BLENDER_NOISE";
	if (cscene->IsTextureCompiled(BLENDER_STUCCI))
		ssParams << " -D PARAM_ENABLE_BLENDER_STUCCI";
 	if (cscene->IsTextureCompiled(BLENDER_WOOD))
 		ssParams << " -D PARAM_ENABLE_BLENDER_WOOD";
	if (cscene->IsTextureCompiled(BLENDER_VORONOI))
		ssParams << " -D PARAM_ENABLE_BLENDER_VORONOI";
    if (cscene->IsTextureCompiled(UV_TEX))
        ssParams << " -D PARAM_ENABLE_TEX_UV";
	if (cscene->IsTextureCompiled(BAND_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BAND";
	if (cscene->IsTextureCompiled(HITPOINTCOLOR))
		ssParams << " -D PARAM_ENABLE_TEX_HITPOINTCOLOR";
	if (cscene->IsTextureCompiled(HITPOINTALPHA))
		ssParams << " -D PARAM_ENABLE_TEX_HITPOINTALPHA";
	if (cscene->IsTextureCompiled(HITPOINTGREY))
		ssParams << " -D PARAM_ENABLE_TEX_HITPOINTGREY";
	if (cscene->IsTextureCompiled(NORMALMAP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_NORMALMAP";
	if (cscene->IsTextureCompiled(BLACKBODY_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BLACKBODY";
	if (cscene->IsTextureCompiled(IRREGULARDATA_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_IRREGULARDATA";
	if (cscene->IsTextureCompiled(DENSITYGRID_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_DENSITYGRID";
	if (cscene->IsTextureCompiled(FRESNELCOLOR_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_FRESNELCOLOR";
	if (cscene->IsTextureCompiled(FRESNELCONST_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_FRESNELCONST";
	if (cscene->IsTextureCompiled(ABS_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_ABS";
	if (cscene->IsTextureCompiled(CLAMP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_CLAMP";
	if (cscene->IsTextureCompiled(BILERP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BILERP";
	if (cscene->IsTextureCompiled(COLORDEPTH_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_COLORDEPTH";
	if (cscene->IsTextureCompiled(HSV_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_HSV";
	if (cscene->IsTextureCompiled(DIVIDE_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_DIVIDE";
	if (cscene->IsTextureCompiled(REMAP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_REMAP";
	if (cscene->IsTextureCompiled(OBJECTID_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_OBJECTID";
	if (cscene->IsTextureCompiled(OBJECTID_COLOR_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_OBJECTID_COLOR";
	if (cscene->IsTextureCompiled(OBJECTID_NORMALIZED_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_OBJECTID_NORMALIZED";
	if (cscene->IsTextureCompiled(DOT_PRODUCT_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_DOT_PRODUCT";
	if (cscene->IsTextureCompiled(GREATER_THAN_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_GREATER_THAN";
	if (cscene->IsTextureCompiled(LESS_THAN_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_LESS_THAN";
	if (cscene->IsTextureCompiled(POWER_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_POWER";
	if (cscene->IsTextureCompiled(SHADING_NORMAL_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_SHADING_NORMAL";
	if (cscene->IsTextureCompiled(POSITION_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_POSITION";
	if (cscene->IsTextureCompiled(SPLIT_FLOAT3))
		ssParams << " -D PARAM_ENABLE_TEX_SPLIT_FLOAT3";
	if (cscene->IsTextureCompiled(MAKE_FLOAT3))
		ssParams << " -D PARAM_ENABLE_TEX_MAKE_FLOAT3";
    if (cscene->IsTextureCompiled(ROUNDING_TEX))
        ssParams << " -D PARAM_ENABLE_TEX_ROUNDING";
	if (cscene->IsTextureCompiled(MODULO_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_MODULO";
	if (cscene->IsTextureCompiled(BRIGHT_CONTRAST_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BRIGHT_CONTRAST";

	if (cscene->IsMaterialCompiled(MATTE))
		ssParams << " -D PARAM_ENABLE_MAT_MATTE";
	if (cscene->IsMaterialCompiled(ROUGHMATTE))
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHMATTE";
	if (cscene->IsMaterialCompiled(VELVET))
		ssParams << " -D PARAM_ENABLE_MAT_VELVET";
	if (cscene->IsMaterialCompiled(MIRROR))
		ssParams << " -D PARAM_ENABLE_MAT_MIRROR";
	if (cscene->IsMaterialCompiled(GLASS))
		ssParams << " -D PARAM_ENABLE_MAT_GLASS";
	if (cscene->IsMaterialCompiled(ARCHGLASS))
		ssParams << " -D PARAM_ENABLE_MAT_ARCHGLASS";
	if (cscene->IsMaterialCompiled(MIX))
		ssParams << " -D PARAM_ENABLE_MAT_MIX";
	if (cscene->IsMaterialCompiled(NULLMAT))
		ssParams << " -D PARAM_ENABLE_MAT_NULL";
	if (cscene->IsMaterialCompiled(MATTETRANSLUCENT))
		ssParams << " -D PARAM_ENABLE_MAT_MATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(ROUGHMATTETRANSLUCENT))
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(GLOSSY2)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2";

		if (cscene->IsMaterialCompiled(GLOSSY2_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSY2_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSY2_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSY2_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(METAL2)) {
		ssParams << " -D PARAM_ENABLE_MAT_METAL2";
		if (cscene->IsMaterialCompiled(METAL2_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_METAL2_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(ROUGHGLASS)) {
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHGLASS";
		if (cscene->IsMaterialCompiled(ROUGHGLASS_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(CLOTH))
		ssParams << " -D PARAM_ENABLE_MAT_CLOTH";
	if (cscene->IsMaterialCompiled(CARPAINT))
		ssParams << " -D PARAM_ENABLE_MAT_CARPAINT";
	if (cscene->IsMaterialCompiled(CLEAR_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_CLEAR_VOL";
	if (cscene->IsMaterialCompiled(HOMOGENEOUS_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_HOMOGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(HETEROGENEOUS_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_HETEROGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT";

		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(GLOSSYCOATING)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING";

		if (cscene->IsMaterialCompiled(GLOSSYCOATING_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_MULTIBOUNCE";
	}

	if (cscene->IsMaterialCompiled(DISNEY))
		ssParams << " -D PARAM_ENABLE_MAT_DISNEY";

	if (cscene->RequiresPassThrough())
		ssParams << " -D PARAM_HAS_PASSTHROUGH";

	switch (cscene->cameraType) {
		case slg::ocl::PERSPECTIVE:
			ssParams << " -D PARAM_CAMERA_TYPE=0";
			break;
		case slg::ocl::ORTHOGRAPHIC:
			ssParams << " -D PARAM_CAMERA_TYPE=1";
			break;
		case slg::ocl::STEREO:
			ssParams << " -D PARAM_CAMERA_TYPE=2";
			break;
		case slg::ocl::ENVIRONMENT:
			ssParams << " -D PARAM_CAMERA_TYPE=3";
			break;
		default:
			throw runtime_error("Unknown camera type in PathOCLBaseRenderThread::InitKernels()");
	}

	if (cscene->enableCameraClippingPlane)
		ssParams << " -D PARAM_CAMERA_ENABLE_CLIPPING_PLANE";
	if (cscene->enableCameraOculusRiftBarrel)
		ssParams << " -D PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL";

	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL))
		ssParams << " -D PARAM_HAS_INFINITELIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL_CONSTANT))
		ssParams << " -D PARAM_HAS_CONSTANTINFINITELIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL_SKY))
		ssParams << " -D PARAM_HAS_SKYLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL_SKY2))
		ssParams << " -D PARAM_HAS_SKYLIGHT2";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SUN))
		ssParams << " -D PARAM_HAS_SUNLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SHARPDISTANT))
		ssParams << " -D PARAM_HAS_SHARPDISTANTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_DISTANT))
		ssParams << " -D PARAM_HAS_DISTANTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_POINT))
		ssParams << " -D PARAM_HAS_POINTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_MAPPOINT))
		ssParams << " -D PARAM_HAS_MAPPOINTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SPOT))
		ssParams << " -D PARAM_HAS_SPOTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_PROJECTION))
		ssParams << " -D PARAM_HAS_PROJECTIONLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_LASER))
		ssParams << " -D PARAM_HAS_LASERLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_TRIANGLE))
		ssParams << " -D PARAM_HAS_TRIANGLELIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SPHERE))
		ssParams << " -D PARAM_HAS_SPHERELIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_MAPSPHERE))
		ssParams << " -D PARAM_HAS_MAPSPHERELIGHT";

	if (renderEngine->compiledScene->hasEnvLights)
		ssParams << " -D PARAM_HAS_ENVLIGHTS";

	if (imageMapDescsBuff) {
		ssParams << " -D PARAM_HAS_IMAGEMAPS";
		if (imageMapsBuff.size() > 8)
			throw runtime_error("Too many memory pages required for image maps");
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			ssParams << " -D PARAM_IMAGEMAPS_PAGE_" << i;
		ssParams << " -D PARAM_IMAGEMAPS_COUNT=" << imageMapsBuff.size();

		if (renderEngine->compiledScene->IsImageMapFormatCompiled(ImageMapStorage::BYTE))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_BYTE_FORMAT";
		if (renderEngine->compiledScene->IsImageMapFormatCompiled(ImageMapStorage::HALF))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_HALF_FORMAT";
		if (renderEngine->compiledScene->IsImageMapFormatCompiled(ImageMapStorage::FLOAT))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_FLOAT_FORMAT";

		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(1))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_1xCHANNELS";
		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(2))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_2xCHANNELS";
		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(3))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_3xCHANNELS";
		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(4))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_4xCHANNELS";

		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::REPEAT))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_REPEAT";
		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::BLACK))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_BLACK";
		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::WHITE))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_WHITE";
		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::CLAMP))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_CLAMP";
	}

	if (renderEngine->compiledScene->HasBumpMaps())
		ssParams << " -D PARAM_HAS_BUMPMAPS";

	if (renderEngine->compiledScene->HasVolumes()) {
		ssParams << " -D PARAM_HAS_VOLUMES";
		ssParams << " -D SCENE_DEFAULT_VOLUME_INDEX=" << renderEngine->compiledScene->defaultWorldVolumeIndex;
	}

	ssParams <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->pathTracer.maxPathDepth.depth <<
			" -D PARAM_MAX_PATH_DEPTH_DIFFUSE=" << renderEngine->pathTracer.maxPathDepth.diffuseDepth <<
			" -D PARAM_MAX_PATH_DEPTH_GLOSSY=" << renderEngine->pathTracer.maxPathDepth.glossyDepth <<
			" -D PARAM_MAX_PATH_DEPTH_SPECULAR=" << renderEngine->pathTracer.maxPathDepth.specularDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->pathTracer.rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->pathTracer.rrImportanceCap << "f" <<
			" -D PARAM_SQRT_VARIANCE_CLAMP_MAX_VALUE=" << renderEngine->pathTracer.sqrtVarianceClampMaxValue << "f";

	const slg::ocl::Filter *filter = renderEngine->oclPixelFilter;
	switch (filter->type) {
		case slg::ocl::FILTER_NONE:
			ssParams << " -D PARAM_IMAGE_FILTER_TYPE=0"
					" -D PARAM_IMAGE_FILTER_WIDTH_X=.5f"
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=.5f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=0" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=0";
			break;
		case slg::ocl::FILTER_BOX:
			ssParams << " -D PARAM_IMAGE_FILTER_TYPE=1" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->box.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->box.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->box.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->box.widthY * .5f + .5f);
			break;
		case slg::ocl::FILTER_GAUSSIAN:
			ssParams << " -D PARAM_IMAGE_FILTER_TYPE=2" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->gaussian.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->gaussian.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->gaussian.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->gaussian.widthY * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA=" << filter->gaussian.alpha << "f";
			break;
		case slg::ocl::FILTER_MITCHELL:
			ssParams << " -D PARAM_IMAGE_FILTER_TYPE=3" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->mitchell.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->mitchell.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->mitchell.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->mitchell.widthY * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << filter->mitchell.B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << filter->mitchell.C << "f";
			break;
		case slg::ocl::FILTER_MITCHELL_SS:
			ssParams << " -D PARAM_IMAGE_FILTER_TYPE=4" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->mitchellss.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->mitchellss.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->mitchellss.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->mitchellss.widthY * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << filter->mitchellss.B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << filter->mitchellss.C << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_A0=" << filter->mitchellss.a0 << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_A1=" << filter->mitchellss.a1 << "f";
			break;
		case slg::ocl::FILTER_BLACKMANHARRIS:
			ssParams << " -D PARAM_IMAGE_FILTER_TYPE=5" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->blackmanharris.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->blackmanharris.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->blackmanharris.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->blackmanharris.widthY * .5f + .5f);
			break;
		default:
			throw runtime_error("Unknown pixel filter type: "  + boost::lexical_cast<string>(filter->type));
	}

	if (renderEngine->usePixelAtomics)
		ssParams << " -D PARAM_USE_PIXEL_ATOMICS";

	if (renderEngine->pathTracer.forceBlackBackground)
		ssParams << " -D PARAM_FORCE_BLACK_BACKGROUND";

	if (renderEngine->pathTracer.hybridBackForwardEnable) {
		ssParams << " -D PARAM_HYBRID_BACKFORWARD" <<
				" -D PARAM_HYBRID_BACKFORWARD_GLOSSINESSTHRESHOLD=" << renderEngine->pathTracer.hybridBackForwardGlossinessThreshold << "f";
	}

	const slg::ocl::Sampler *sampler = renderEngine->oclSampler;
	switch (sampler->type) {
		case slg::ocl::RANDOM:
			ssParams << " -D PARAM_SAMPLER_TYPE=0";
			break;
		case slg::ocl::METROPOLIS:
			ssParams << " -D PARAM_SAMPLER_TYPE=1" <<
					" -D PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE=" << sampler->metropolis.largeMutationProbability << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE=" << sampler->metropolis.imageMutationRange << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << sampler->metropolis.maxRejects;
			break;
		case slg::ocl::SOBOL: {
			ssParams << " -D PARAM_SAMPLER_TYPE=2" <<
					" -D PARAM_SAMPLER_SOBOL_STARTOFFSET=" << SOBOL_STARTOFFSET;
			break;
		}
		case slg::ocl::TILEPATHSAMPLER:
			ssParams << " -D PARAM_SAMPLER_TYPE=3";
			break;
		default:
			throw runtime_error("Unknown sampler type in PathOCLBaseRenderThread::AdditionalKernelOptions(): " + boost::lexical_cast<string>(sampler->type));
	}

	if (cscene->photonGICache) {
		ssParams << " -D PARAM_PGIC_ENABLED";

		switch (cscene->pgicDebugType) {
			case PGIC_DEBUG_NONE:
				ssParams << " -D PARAM_PGIC_DEBUG_NONE";
				break;
			case PGIC_DEBUG_SHOWINDIRECT:
				ssParams << " -D PARAM_PGIC_DEBUG_SHOWINDIRECT";
				break;
			case PGIC_DEBUG_SHOWCAUSTIC:
				ssParams << " -D PARAM_PGIC_DEBUG_SHOWCAUSTIC";
				break;
			case PGIC_DEBUG_SHOWINDIRECTPATHMIX:
				ssParams << " -D PARAM_PGIC_DEBUG_SHOWINDIRECTPATHMIX";
				break;
			default:
				break;
		}

		if (cscene->photonGICache->GetParams().indirect.enabled)
			ssParams << " -D PARAM_PGIC_INDIRECT_ENABLED";

		if (cscene->photonGICache->GetParams().caustic.enabled)
			ssParams << " -D PARAM_PGIC_CAUSTIC_ENABLED";
	}
	
	ssParams << AdditionalKernelOptions();

	//--------------------------------------------------------------------------

	// Check the OpenCL vendor and use some specific compiler options

#if defined(__APPLE__)
	// Starting with 10.10 (darwin 14.x.x), opencl mix() function is fixed
	char darwin_ver[10];
	size_t len = sizeof(darwin_ver);
	sysctlbyname("kern.osrelease", &darwin_ver, &len, NULL, 0);
	if(darwin_ver[0] == '1' && darwin_ver[1] < '4') {
		ssParams << " -D __APPLE_CL__";
	}
#else
        if (intersectionDevice->GetDeviceDesc()->IsAMDPlatform())
            ssParams << " -D LUXCORE_AMD_OPENCL";
        else if (intersectionDevice->GetDeviceDesc()->IsNVIDIAPlatform())
            ssParams << " -D LUXCORE_NVIDIA_OPENCL";
        else
            ssParams << " -D LUXCORE_GENERIC_OPENCL";
#endif

	//--------------------------------------------------------------------------

	// This is a workaround to NVIDIA long compilation time
	string forceInlineDirective;
	if (intersectionDevice->GetDeviceDesc()->IsNVIDIAPlatform()) {
		//SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] NVIDIA platform: using inline workaround");
		forceInlineDirective =
				"#define OPENCL_FORCE_NOT_INLINE __attribute__((noinline))\n"
				"#define OPENCL_FORCE_INLINE __attribute__((always_inline))\n";
	} else {
		//SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Not NVIDIA platform: not using inline workaround");
		forceInlineDirective =
				"#define OPENCL_FORCE_NOT_INLINE\n"
				"#define OPENCL_FORCE_INLINE\n";
	}

	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	kernelsParameters = ssParams.str();
	// This is a workaround for an Apple OpenCL by Arve Nygard. The double space
	// causes clBuildProgram() to fail with CL_INVALID_BUILD_OPTIONS on OSX.
	boost::replace_all(kernelsParameters, "  ", " ");

	// Compile sources
	stringstream ssKernel;
	ssKernel <<
			forceInlineDirective <<
			AdditionalKernelDefinitions() <<
			SamplerKernelDefinitions() <<
			// OpenCL LuxRays Types
			luxrays::ocl::KernelSource_luxrays_types <<
			luxrays::ocl::KernelSource_randomgen_types <<
			luxrays::ocl::KernelSource_uv_types <<
			luxrays::ocl::KernelSource_point_types <<
			luxrays::ocl::KernelSource_vector_types <<
			luxrays::ocl::KernelSource_normal_types <<
			luxrays::ocl::KernelSource_triangle_types <<
			luxrays::ocl::KernelSource_ray_types <<
			luxrays::ocl::KernelSource_bbox_types <<
			luxrays::ocl::KernelSource_epsilon_types <<
			luxrays::ocl::KernelSource_color_types <<
			luxrays::ocl::KernelSource_frame_types <<
			luxrays::ocl::KernelSource_matrix4x4_types <<
			luxrays::ocl::KernelSource_quaternion_types <<
			luxrays::ocl::KernelSource_transform_types <<
			luxrays::ocl::KernelSource_motionsystem_types <<
			luxrays::ocl::KernelSource_trianglemesh_types <<
			// OpenCL LuxRays Funcs
			luxrays::ocl::KernelSource_randomgen_funcs <<
			luxrays::ocl::KernelSource_atomic_funcs <<
			luxrays::ocl::KernelSource_epsilon_funcs <<
			luxrays::ocl::KernelSource_utils_funcs <<
			luxrays::ocl::KernelSource_mc_funcs <<
			luxrays::ocl::KernelSource_vector_funcs <<
			luxrays::ocl::KernelSource_ray_funcs <<
			luxrays::ocl::KernelSource_bbox_funcs <<
			luxrays::ocl::KernelSource_color_funcs <<
			luxrays::ocl::KernelSource_frame_funcs <<
			luxrays::ocl::KernelSource_matrix4x4_funcs <<
			luxrays::ocl::KernelSource_quaternion_funcs <<
			luxrays::ocl::KernelSource_transform_funcs <<
			luxrays::ocl::KernelSource_motionsystem_funcs <<
			luxrays::ocl::KernelSource_triangle_funcs <<
			luxrays::ocl::KernelSource_trianglemesh_funcs <<
			// OpenCL SLG Types
			slg::ocl::KernelSource_hitpoint_types <<
			slg::ocl::KernelSource_mapping_types <<
			slg::ocl::KernelSource_texture_types <<
			slg::ocl::KernelSource_bsdf_types <<
			slg::ocl::KernelSource_material_types <<
			slg::ocl::KernelSource_volume_types <<
			slg::ocl::KernelSource_film_types <<
			slg::ocl::KernelSource_filter_types <<
			slg::ocl::KernelSource_sampler_types <<
			slg::ocl::KernelSource_camera_types <<
			slg::ocl::KernelSource_light_types <<
			slg::ocl::KernelSource_indexbvh_types <<
			slg::ocl::KernelSource_dlsc_types <<
			slg::ocl::KernelSource_elvc_types <<
			slg::ocl::KernelSource_sceneobject_types <<
			slg::ocl::KernelSource_pgic_types <<
			// OpenCL SLG Funcs
			slg::ocl::KernelSource_mapping_funcs <<
			slg::ocl::KernelSource_imagemap_types <<
			slg::ocl::KernelSource_imagemap_funcs <<
			slg::ocl::KernelSource_texture_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs2 <<
			slg::ocl::KernelSource_texture_blender_funcs <<
			slg::ocl::KernelSource_texture_abs_funcs <<
			slg::ocl::KernelSource_texture_bilerp_funcs <<
			slg::ocl::KernelSource_texture_blackbody_funcs <<
			slg::ocl::KernelSource_texture_clamp_funcs <<
			slg::ocl::KernelSource_texture_colordepth_funcs <<
			slg::ocl::KernelSource_texture_densitygrid_funcs <<
			slg::ocl::KernelSource_texture_fresnelcolor_funcs <<
			slg::ocl::KernelSource_texture_fresnelconst_funcs <<
			slg::ocl::KernelSource_texture_hsv_funcs <<
			slg::ocl::KernelSource_texture_irregulardata_funcs <<
			slg::ocl::KernelSource_texture_funcs;

	// Generate the code to evaluate the textures
	ssKernel <<
			"#line 2 \"Texture evaluation code form CompiledScene::GetTexturesEvaluationSourceCode()\"\n" <<
			cscene->GetTexturesEvaluationSourceCode() <<
			"\n";

	ssKernel <<
			slg::ocl::KernelSource_materialdefs_funcs_generic <<
			slg::ocl::KernelSource_materialdefs_funcs_default <<
			slg::ocl::KernelSource_materialdefs_funcs_archglass <<
			slg::ocl::KernelSource_materialdefs_funcs_carpaint <<
			slg::ocl::KernelSource_materialdefs_funcs_clearvol <<
			slg::ocl::KernelSource_materialdefs_funcs_cloth <<
			slg::ocl::KernelSource_materialdefs_funcs_disney <<
			slg::ocl::KernelSource_materialdefs_funcs_glass <<
			slg::ocl::KernelSource_materialdefs_funcs_glossy2 <<
			slg::ocl::KernelSource_materialdefs_funcs_glossytranslucent <<
			slg::ocl::KernelSource_materialdefs_funcs_heterogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_homogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_matte <<
			slg::ocl::KernelSource_materialdefs_funcs_matte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_metal2 <<
			slg::ocl::KernelSource_materialdefs_funcs_mirror <<
			slg::ocl::KernelSource_materialdefs_funcs_null <<
			slg::ocl::KernelSource_materialdefs_funcs_roughglass <<
			slg::ocl::KernelSource_materialdefs_funcs_roughmatte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_velvet <<
			slg::ocl::KernelSource_material_main_withoutdynamic;

	// Generate the code to evaluate the materials
	ssKernel <<
			// This is the dynamic generated code (aka "WithDynamic")
			"#line 2 \"Material evaluation code form CompiledScene::GetMaterialsEvaluationSourceCode()\"\n" <<
			cscene->GetMaterialsEvaluationSourceCode() <<
			"\n" <<
			slg::ocl::KernelSource_material_main;

	ssKernel <<
			slg::ocl::KernelSource_bsdfutils_funcs << // Must be before volumeinfo_funcs
			slg::ocl::KernelSource_volume_funcs <<
			slg::ocl::KernelSource_volumeinfo_funcs <<
			slg::ocl::KernelSource_camera_funcs <<
			slg::ocl::KernelSource_dlsc_funcs <<
			slg::ocl::KernelSource_elvc_funcs <<
			slg::ocl::KernelSource_lightstrategy_funcs <<
			slg::ocl::KernelSource_light_funcs <<
			slg::ocl::KernelSource_filter_funcs <<
			slg::ocl::KernelSource_sampleresult_funcs <<
			slg::ocl::KernelSource_filmdenoiser_funcs <<
			slg::ocl::KernelSource_film_funcs <<
			slg::ocl::KernelSource_pathdepthinfo_types <<
			slg::ocl::KernelSource_varianceclamping_funcs <<
			slg::ocl::KernelSource_sampler_random_funcs <<
			slg::ocl::KernelSource_sampler_sobol_funcs <<
			slg::ocl::KernelSource_sampler_metropolis_funcs <<
			slg::ocl::KernelSource_sampler_tilepath_funcs <<
			slg::ocl::KernelSource_bsdf_funcs <<
			slg::ocl::KernelSource_scene_funcs <<
			slg::ocl::KernelSource_pgic_funcs <<
			// PathOCL Funcs
			slg::ocl::KernelSource_pathoclbase_datatypes <<
			slg::ocl::KernelSource_pathoclbase_funcs <<
			slg::ocl::KernelSource_pathoclbase_kernels_micro;

	ssKernel << AdditionalKernelSources();

	string kernelSource = ssKernel.str();

	// Build the kernel source/parameters hash
	const string newKernelSrcHash = oclKernelPersistentCache::HashString(kernelsParameters) + "-" +
			oclKernelPersistentCache::HashString(kernelSource);
	if (newKernelSrcHash == kernelSrcHash) {
		// There is no need to re-compile the kernel
		return;
	} else
		kernelSrcHash = newKernelSrcHash;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling kernels ");

	if (renderEngine->writeKernelsToFile) {
		// Some debug code to write the OpenCL kernel source to a file
		const string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".cl";
		ofstream kernelFile(kernelFileName.c_str());
		string kernelDefs = kernelsParameters;
		boost::replace_all(kernelDefs, "-D", "\n#define");
		boost::replace_all(kernelDefs, "=", " ");
		kernelFile << kernelDefs << endl << endl << kernelSource << endl;
		kernelFile.close();
	}

	bool cached;
	cl::STRING_CLASS error;
	cl::Program *program = kernelCache->Compile(oclContext, oclDevice,
			kernelsParameters, kernelSource,
			&cached, &error);

	if (!program) {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] PathOCL kernel compilation error" << endl << error);

		throw runtime_error("PathOCLBase kernel compilation error");
	}

	if (cached) {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels cached");
	} else {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels not cached");
	}

	// Film clear kernel
	CompileKernel(program, &filmClearKernel, &filmClearWorkGroupSize, "Film_Clear");

	// Init kernel

	CompileKernel(program, &initSeedKernel, &initWorkGroupSize, "InitSeed");
	CompileKernel(program, &initKernel, &initWorkGroupSize, "Init");

	// AdvancePaths kernel (Micro-Kernels)

	CompileKernel(program, &advancePathsKernel_MK_RT_NEXT_VERTEX, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_NEXT_VERTEX");
	CompileKernel(program, &advancePathsKernel_MK_HIT_NOTHING, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_HIT_NOTHING");
	CompileKernel(program, &advancePathsKernel_MK_HIT_OBJECT, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_HIT_OBJECT");
	CompileKernel(program, &advancePathsKernel_MK_RT_DL, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_DL");
	CompileKernel(program, &advancePathsKernel_MK_DL_ILLUMINATE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_DL_ILLUMINATE");
	CompileKernel(program, &advancePathsKernel_MK_DL_SAMPLE_BSDF, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_DL_SAMPLE_BSDF");
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY");
	CompileKernel(program, &advancePathsKernel_MK_SPLAT_SAMPLE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_SPLAT_SAMPLE");
	CompileKernel(program, &advancePathsKernel_MK_NEXT_SAMPLE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_NEXT_SAMPLE");
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_CAMERA_RAY, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_GENERATE_CAMERA_RAY");

	// Additional kernels
	CompileAdditionalKernels(program);

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

	delete program;
}

void PathOCLBaseOCLRenderThread::SetInitKernelArgs(const u_int filmIndex) {
	CompiledScene *cscene = renderEngine->compiledScene;

	// initSeedKernel kernel
	u_int argIndex = 0;
	initSeedKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	initSeedKernel->setArg(argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);

	// initKernel kernel
	argIndex = 0;
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksDirectLightBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksStateBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), taskStatsBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), samplerSharedDataBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), samplesBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleDataBuff);
	if (cscene->HasVolumes())
		initKernel->setArg(argIndex++, sizeof(cl::Buffer), pathVolInfosBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), pixelFilterBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), raysBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), cameraBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*initKernel, argIndex);

	initKernelArgsCount = argIndex;
}

void PathOCLBaseOCLRenderThread::SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel, const u_int filmIndex) {
	CompiledScene *cscene = renderEngine->compiledScene;

	u_int argIndex = 0;
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksDirectLightBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksStateBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), taskStatsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pixelFilterBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), samplerSharedDataBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), samplesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleDataBuff);
	if (cscene->HasVolumes()) {
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pathVolInfosBuff);
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), directLightVolInfosBuff);
	}
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), raysBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), hitsBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*advancePathsKernel, argIndex);

	// Scene parameters
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), materialsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), texturesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), scnObjsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), meshDescsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), vertsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), normalsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), uvsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), colsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), alphasBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), trianglesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), cameraBuff);
	// Lights
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), envLightIndicesBuff);
	advancePathsKernel->setArg(argIndex++, (u_int)cscene->envLightIndices.size());
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightIndexOffsetByMeshIndexBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightIndexByTriIndexBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), envLightDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightsDistributionBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), infiniteLightSourcesDistributionBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), dlscAllEntriesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), dlscDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), dlscBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, cscene->dlscRadius2);
	advancePathsKernel->setArg(argIndex++, cscene->dlscNormalCosAngle);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), elvcAllEntriesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), elvcDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), elvcBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, cscene->elvcRadius2);
	advancePathsKernel->setArg(argIndex++, cscene->elvcNormalCosAngle);

	// Images
	if (imageMapDescsBuff) {
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), imageMapDescsBuff);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), (imageMapsBuff[i]));
	}

	// PhotonGI cache
	if (cscene->photonGICache) {
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicRadiancePhotonsBuff);
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicRadiancePhotonsBVHNodesBuff);
		advancePathsKernel->setArg(argIndex++, cscene->pgicIndirectLookUpRadius);
		advancePathsKernel->setArg(argIndex++, cscene->pgicIndirectLookUpNormalCosAngle);
		advancePathsKernel->setArg(argIndex++, cscene->pgicIndirectGlossinessUsageThreshold);
		advancePathsKernel->setArg(argIndex++, cscene->pgicIndirectUsageThresholdScale);

		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicCausticPhotonsBuff);
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicCausticPhotonsBVHNodesBuff);
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicCausticNearPhotonsBuff);
		advancePathsKernel->setArg(argIndex++, cscene->pgicCausticPhotonTracedCount);
		advancePathsKernel->setArg(argIndex++, cscene->pgicCausticLookUpRadius);
		advancePathsKernel->setArg(argIndex++, cscene->pgicCausticLookUpNormalCosAngle);
		advancePathsKernel->setArg(argIndex++, cscene->pgicCausticLookUpMaxCount);
	}
}

void PathOCLBaseOCLRenderThread::SetAllAdvancePathsKernelArgs(const u_int filmIndex) {
	if (advancePathsKernel_MK_RT_NEXT_VERTEX)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_NEXT_VERTEX, filmIndex);
	if (advancePathsKernel_MK_HIT_NOTHING)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_NOTHING, filmIndex);
	if (advancePathsKernel_MK_HIT_OBJECT)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_OBJECT, filmIndex);
	if (advancePathsKernel_MK_RT_DL)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_DL, filmIndex);
	if (advancePathsKernel_MK_DL_ILLUMINATE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_ILLUMINATE, filmIndex);
	if (advancePathsKernel_MK_DL_SAMPLE_BSDF)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_SAMPLE_BSDF, filmIndex);
	if (advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, filmIndex);
	if (advancePathsKernel_MK_SPLAT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_SPLAT_SAMPLE, filmIndex);
	if (advancePathsKernel_MK_NEXT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_NEXT_SAMPLE, filmIndex);
	if (advancePathsKernel_MK_GENERATE_CAMERA_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_CAMERA_RAY, filmIndex);
}

void PathOCLBaseOCLRenderThread::SetKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only non thread safe function in OpenCL 1.1 so
	// I need to use a mutex here

	boost::unique_lock<boost::mutex> lock(renderEngine->setKernelArgsMutex);

	//--------------------------------------------------------------------------
	// advancePathsKernels
	//--------------------------------------------------------------------------

	SetAllAdvancePathsKernelArgs(0);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	SetInitKernelArgs(0);
}

void PathOCLBaseOCLRenderThread::EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue) {
	const u_int taskCount = renderEngine->taskCount;

	// Micro kernels version
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_NEXT_VERTEX, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_NOTHING, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_OBJECT, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_DL, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_ILLUMINATE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_SAMPLE_BSDF, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_SPLAT_SAMPLE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_NEXT_SAMPLE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_CAMERA_RAY, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
}

#endif
