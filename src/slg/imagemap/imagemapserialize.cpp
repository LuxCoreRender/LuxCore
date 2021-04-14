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

#include "slg/imagemap/imagemap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapStorage
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorage)

template<class Archive> void ImageMapStorage::serialize(Archive &ar, const u_int version) {
	ar & width;
	ar & height;
	ar & wrapType;
}

namespace slg {
// Explicit instantiations for portable archives
template void ImageMapStorage::serialize(LuxOutputArchive &ar, const u_int version);
template void ImageMapStorage::serialize(LuxInputArchive &ar, const u_int version);
}

//------------------------------------------------------------------------------
// ImageMapStorageImpl
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplUChar1)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplUChar2)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplUChar3)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplUChar4)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplHalf1)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplHalf2)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplHalf3)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplHalf4)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplFloat1)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplFloat2)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplFloat3)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapStorageImplFloat4)

//------------------------------------------------------------------------------
// ImageMap
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMap)

template<class Archive> void ImageMap::serialize(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(NamedObject);

	ar & pixelStorage;
	ar & imageMean;
	ar & imageMeanY;
}

namespace slg {
// Explicit instantiations for portable archives
template void ImageMap::serialize(LuxOutputArchive &ar, const u_int version);
template void ImageMap::serialize(LuxInputArchive &ar, const u_int version);
}
