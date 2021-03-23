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

#include <boost/format.hpp>

#include "slg/engines/tilerepository.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TileCoord
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Tile::TileCoord)

template<class Archive> void Tile::TileCoord::serialize(Archive &ar, const unsigned int version) {
	ar & x;
	ar & y;
	ar & width;
	ar & height;
}

namespace slg {
// Explicit instantiations for portable archives
template void Tile::serialize(LuxOutputBinArchive &ar, const unsigned int version);
template void Tile::serialize(LuxInputBinArchive &ar, const unsigned int version);
template void Tile::serialize(LuxOutputTextArchive &ar, const unsigned int version);
template void Tile::serialize(LuxInputTextArchive &ar, const unsigned int version);
}

//------------------------------------------------------------------------------
// Tile
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Tile)

template<class Archive> void Tile::load(Archive &ar, const u_int version) {
	ar & coord;
	ar & pass;
	// I don't save the pending tiles
	pendingPasses = 0;
	ar & error;
	ar & done;

	// tileRepository has to be set from the code using the serialization
	tileRepository = NULL;

	ar & allPassFilm;
	// Disable OpenCL
	allPassFilm->hwEnable = false;

	ar & evenPassFilm;
	// Disable OpenCL
	evenPassFilm->hwEnable = false;

	ar & allPassFilmTotalYValue;
	ar & hasEnoughWarmUpSample;
}

template<class Archive> void Tile::save(Archive &ar, const u_int version) const {
	ar & coord;
	ar & pass;
	ar & error;
	ar & done;

	ar & allPassFilm;
	ar & evenPassFilm;
	ar & allPassFilmTotalYValue;
	ar & hasEnoughWarmUpSample;
}

namespace slg {
// Explicit instantiations for portable archives
template void Tile::save(LuxOutputBinArchive &ar, const u_int version) const;
template void Tile::load(LuxInputBinArchive &ar, const u_int version);
template void Tile::save(LuxOutputTextArchive &ar, const u_int version) const;
template void Tile::load(LuxInputTextArchive &ar, const u_int version);
}

//------------------------------------------------------------------------------
// TileRepository
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::TileRepository)

template<class Archive> void TileRepository::load(Archive &ar, const u_int version) {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	ar & tileWidth;
	ar & tileHeight;
	ar & convergenceTestThreshold;
	ar & convergenceTestThresholdReduction;
	ar & convergenceTestWarmUpSamples;
	ar & varianceClamping;
	ar & enableMultipassRendering;
	ar & enableRenderingDonePrint;
	ar & done;

	ar & startTime;
	ar & filmRegionWidth;
	ar & filmRegionHeight;
	ar & filmTotalYValue;
	ar & multipassRenderingIndex;
	ar & tileList;

	u_int todoListSize;
	ar & todoListSize;
	for (u_int i = 0; i < todoListSize; ++i) {
		Tile *todoTile;
		ar & todoTile;
		todoTiles.push(todoTile);
	}

	pendingTiles.resize(0);
	ar & convergedTiles;

	// Initialize the Tile::tileRepository field
	BOOST_FOREACH(Tile *tile, tileList)
		tile->tileRepository = this;
}

template<class Archive> void TileRepository::save(Archive &ar, const u_int version) const {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	ar & tileWidth;
	ar & tileHeight;
	ar & convergenceTestThreshold;
	ar & convergenceTestThresholdReduction;
	ar & convergenceTestWarmUpSamples;
	ar & varianceClamping;
	ar & enableMultipassRendering;
	ar & enableRenderingDonePrint;
	ar & done;

	ar & startTime;
	ar & filmRegionWidth;
	ar & filmRegionHeight;
	ar & filmTotalYValue;
	ar & multipassRenderingIndex;
	ar & tileList;

	const u_int count = todoTiles.size() + pendingTiles.size();
	ar & count;
	BOOST_FOREACH(Tile *tile, todoTiles)
		ar & tile;
	BOOST_FOREACH(Tile *tile, pendingTiles)
		ar & tile;

	ar & convergedTiles;
}

namespace slg {
// Explicit instantiations for portable archives
template void TileRepository::save(LuxOutputBinArchive &ar, const u_int version) const;
template void TileRepository::load(LuxInputBinArchive &ar, const unsigned int version);
template void TileRepository::save(LuxOutputTextArchive &ar, const u_int version) const;
template void TileRepository::load(LuxInputTextArchive &ar, const unsigned int version);
}
