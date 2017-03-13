/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_TILEREPOSITORY_H
#define	_SLG_TILEREPOSITORY_H

#include <boost/thread/mutex.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/set.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "luxrays/utils/utils.h"

#include "slg/slg.h"
#include "slg/engines/renderengine.h"

namespace slg {

//------------------------------------------------------------------------------
// TileRepository 
//------------------------------------------------------------------------------

class TileRepository {
public:
	class Tile {
	public:
		Tile(TileRepository *tileRepository, const Film &film,
				const u_int xStart, const u_int yStart);
		virtual ~Tile();

		void Restart(const u_int pass = 0);
		void VarianceClamp(Film &tileFilm);
		void AddPass(const Film &tileFilm);
		
		// Read-only for every one but Tile/TileRepository classes
		TileRepository *tileRepository;
		u_int xStart, yStart, tileWidth, tileHeight;
		u_int pass;
		float error;
		bool done;

		friend class boost::serialization::access;

	private:
		// Used by serialization
		Tile();

		template<class Archive> void save(Archive &ar, const unsigned int version) const;
		template<class Archive>	void load(Archive &ar, const unsigned int version);
		BOOST_SERIALIZATION_SPLIT_MEMBER()

		void InitTileFilm(const Film &film, Film **tileFilm);
		void CheckConvergence();
		void UpdateTileStats();

		Film *allPassFilm, *evenPassFilm;

		float allPassFilmTotalYValue;
		bool hasEnoughWarmUpSample;
	};

	TileRepository(const u_int tileWidth, const u_int tileHeight);
	~TileRepository();

	void Clear();
	void Restart(const u_int pass = 0);
	void GetPendingTiles(std::deque<const Tile *> &tiles);
	void GetNotConvergedTiles(std::deque<const Tile *> &tiles);
	void GetConvergedTiles(std::deque<const Tile *> &tiles);

	void InitTiles(const Film &film);
	bool NextTile(Film *film, boost::mutex *filmMutex,
		Tile **tile, Film *tileFilm);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static TileRepository *FromProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();

	friend class Tile;

	u_int tileWidth, tileHeight;

	u_int maxPassCount;
	float convergenceTestThreshold, convergenceTestThresholdReduction;
	u_int convergenceTestWarmUpSamples;
	VarianceClamping varianceClamping;
	bool enableMultipassRendering, enableRenderingDonePrint;

	bool done;

	friend class boost::serialization::access;

private:
	// Used by serialization
	TileRepository();

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	class CompareTilesPtr {
	public:
		bool operator()(const TileRepository::Tile *lt, const TileRepository::Tile *rt) const {
			return lt->pass > rt->pass;
		}
	};

	void HilberCurveTiles(
		const Film &film,
		const u_int n, const int xo, const int yo,
		const int xd, const int yd, const int xp, const int yp,
		const int xEnd, const int yEnd);

	void SetDone();
	bool GetToDoTile(Tile **tile);

	mutable boost::mutex tileMutex;
	double startTime;

	u_int filmRegionWidth, filmRegionHeight;
	float filmTotalYValue; // Updated only if convergence test is enabled

	std::vector<Tile *> tileList;

	boost::heap::priority_queue<Tile *,
			boost::heap::compare<CompareTilesPtr>,
			boost::heap::stable<true>
		> todoTiles;
	std::deque<Tile *> pendingTiles;
	std::deque<Tile *> convergedTiles;
};

}

BOOST_CLASS_VERSION(slg::TileRepository, 1)
BOOST_CLASS_VERSION(slg::TileRepository::Tile, 1)

BOOST_CLASS_EXPORT_KEY(slg::TileRepository)		
BOOST_CLASS_EXPORT_KEY(slg::TileRepository::Tile)

#endif	/* _SLG_TILEREPOSITORY_H */
