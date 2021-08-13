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

#ifndef _SLG_IMAGEMAPRESIZEPOLICIES_H
#define	_SLG_IMAGEMAPRESIZEPOLICIES_H

#include <string>
#include <vector>

#include <boost/unordered_map.hpp>

#include "luxrays/devices/ocldevice.h"
#include "slg/imagemap/imagemap.h"
#include "slg/core/sdl.h"

namespace slg {

class Scene;
class ImageMapCache;
class SobolSamplerSharedData;

//------------------------------------------------------------------------------
// ImageMapResizePolicy
//------------------------------------------------------------------------------

typedef enum {
	POLICY_NONE,
	POLICY_FIXED,
	POLICY_MINMEM,
	POLICY_MIPMAPMEM
} ImageMapResizePolicyType;

class ImageMapResizePolicy {
public:
	ImageMapResizePolicy() { }
	virtual ~ImageMapResizePolicy() { }
	
	virtual ImageMapResizePolicyType GetType() const = 0;

	virtual ImageMap *ApplyResizePolicy(const std::string &fileName, const ImageMapConfig &imgCfg, bool &toApply) const = 0;
	virtual void Preprocess(ImageMapCache &imc, const Scene *scene, const bool useRTMode) const = 0;

	static ImageMapResizePolicy *FromProperties(const luxrays::Properties &props);
	static ImageMapResizePolicyType String2ImageMapResizePolicyType(const std::string &type);
	static std::string ImageMapResizePolicyType2String(const ImageMapResizePolicyType type);

	static void CalcOptimalImageMapSizes(ImageMapCache &imc,
			const Scene *scene, const std::vector<u_int> &imgMapsIndices);
	
	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
	}

	static void RenderFunc(const u_int threadIndex,
		ImageMapCache *imc, const std::vector<u_int> *imgMapsIndices, u_int *workCounter,
		const Scene *scene, SobolSamplerSharedData *sobolSharedData,
		boost::barrier *threadsSyncBarrier);
};

//------------------------------------------------------------------------------
// ImageMapResizeNonePolicy
//------------------------------------------------------------------------------

class ImageMapResizeNonePolicy : public ImageMapResizePolicy {
public:
	ImageMapResizeNonePolicy() { }
	~ImageMapResizeNonePolicy() { }
	
	virtual ImageMapResizePolicyType GetType() const { return POLICY_NONE; }
	virtual ImageMap *ApplyResizePolicy(const std::string &fileName, const ImageMapConfig &imgCfg, bool &toApply) const;
	virtual void Preprocess(ImageMapCache &imc, const Scene *scene, const bool useRTMode) const { };

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImageMapResizePolicy);
	}
};

//------------------------------------------------------------------------------
// ImageMapResizeFixedPolicy
//------------------------------------------------------------------------------

class ImageMapResizeFixedPolicy : public ImageMapResizePolicy {
public:
	ImageMapResizeFixedPolicy(const float s, const u_int m = 128) : scale(s), minSize(m) { }
	~ImageMapResizeFixedPolicy() { }
	
	virtual ImageMapResizePolicyType GetType() const { return POLICY_FIXED; }
	virtual ImageMap *ApplyResizePolicy(const std::string &fileName, const ImageMapConfig &imgCfg, bool &toApply) const;
	virtual void Preprocess(ImageMapCache &imc, const Scene *scene, const bool useRTMode) const { };

	friend class boost::serialization::access;

	float scale;
	u_int minSize;

private:
	// Used by serialization
	ImageMapResizeFixedPolicy() : scale(1.f), minSize(128) {		
	}

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImageMapResizePolicy);
		
		ar & scale;
		ar & minSize;
	}
};

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy
//------------------------------------------------------------------------------

class ImageMapResizeMinMemPolicy : public ImageMapResizePolicy {
public:
	ImageMapResizeMinMemPolicy(const float s = 1.f, const u_int m = 32) : scale(s), minSize(m) { }
	~ImageMapResizeMinMemPolicy() { }
	
	virtual ImageMapResizePolicyType GetType() const { return POLICY_MINMEM; }
	virtual ImageMap *ApplyResizePolicy(const std::string &fileName, const ImageMapConfig &imgCfg, bool &toApply) const;
	virtual void Preprocess(ImageMapCache &imc, const Scene *scene, const bool useRTMode) const;

	friend class boost::serialization::access;

	float scale;
	u_int minSize;

private:
	// Used by serialization
	ImageMapResizeMinMemPolicy() : scale(1.f), minSize(32) {		
	}

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImageMapResizePolicy);

		ar & scale;
		ar & minSize;
	}
};

//------------------------------------------------------------------------------
// ImageMapResizeMipMapMemPolicy
//------------------------------------------------------------------------------

class ImageMapResizeMipMapMemPolicy : public ImageMapResizePolicy {
public:
	ImageMapResizeMipMapMemPolicy(const float s = 1.f, const u_int m = 32) : scale(s), minSize(m) { }
	~ImageMapResizeMipMapMemPolicy() { }
	
	virtual ImageMapResizePolicyType GetType() const { return POLICY_MIPMAPMEM; }
	virtual ImageMap *ApplyResizePolicy(const std::string &fileName, const ImageMapConfig &imgCfg, bool &toApply) const;
	virtual void Preprocess(ImageMapCache &imc, const Scene *scene, const bool useRTMode)const;

	friend class boost::serialization::access;

	float scale;
	u_int minSize;

private:
	// Used by serialization
	ImageMapResizeMipMapMemPolicy() : scale(1.f), minSize(32) {		
	}

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImageMapResizePolicy);

		ar & scale;
		ar & minSize;
	}
};

}

BOOST_CLASS_VERSION(slg::ImageMapResizePolicy, 1)
BOOST_CLASS_VERSION(slg::ImageMapResizeNonePolicy, 1)
BOOST_CLASS_VERSION(slg::ImageMapResizeFixedPolicy, 1)
BOOST_CLASS_VERSION(slg::ImageMapResizeMinMemPolicy, 1)
BOOST_CLASS_VERSION(slg::ImageMapResizeMipMapMemPolicy, 1)

BOOST_CLASS_EXPORT_KEY(slg::ImageMapResizePolicy)
BOOST_CLASS_EXPORT_KEY(slg::ImageMapResizeNonePolicy)
BOOST_CLASS_EXPORT_KEY(slg::ImageMapResizeFixedPolicy)
BOOST_CLASS_EXPORT_KEY(slg::ImageMapResizeMinMemPolicy)
BOOST_CLASS_EXPORT_KEY(slg::ImageMapResizeMipMapMemPolicy)

#endif	/* _SLG_IMAGEMAPRESIZEPOLICIES_H */
