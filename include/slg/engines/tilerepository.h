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

#ifndef _SLG_TILEREPOSITORY_H
#define	_SLG_TILEREPOSITORY_H

#include <boost/thread/mutex.hpp>

#include "luxrays/utils/utils.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/engines/renderengine.h"

namespace slg {

//------------------------------------------------------------------------------
// Tile
//------------------------------------------------------------------------------

class TileWork;
class TileRepository;

class Tile {
public:
	class TileCoord {
	public:
		TileCoord() { }
		TileCoord(const u_int xs, const u_int ys,
				const u_int w, const u_int h) :
				x(xs), y(ys), width(w), height(w) { }
		friend class boost::serialization::access;

		u_int x, y, width, height;

	private:
		template<class Archive> void serialize(Archive &ar, const unsigned int version);
	};

	Tile(TileRepository *tileRepository, const Film &film, const u_int tileIndex,
			const u_int xStart, const u_int yStart);
	virtual ~Tile();

	void Restart(const u_int pass = 0);

	// Read-only for every one but Tile/TileRepository classes
	TileRepository *tileRepository;
	u_int tileIndex;
	TileCoord coord;
	u_int pass, pendingPasses;
	float error;
	bool done;

	friend class TileWork;
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
	void VarianceClamp(Film &tileFilm);
	void AddPass(Film &tileFilm, const u_int passRendered);

	Film *allPassFilm, *evenPassFilm;

	float allPassFilmTotalYValue;
	bool hasEnoughWarmUpSample;
};

//------------------------------------------------------------------------------
// TileWork
//------------------------------------------------------------------------------

class TileWork {
public:
	TileWork();

	bool HasWork() const { return (tile != nullptr); }
	void Reset() { tile = nullptr; }
	const Tile::TileCoord &GetCoord() const { return tile->coord; }

	// Read-only for every one but Tile/TileRepository classes
	u_int multipassIndexToRender;
	u_int passToRender;

	friend class TileRepository;

private:
	TileWork(Tile *t);

	void Init(Tile *t);
	void AddPass(Film &tileFilm);

	Tile *tile;
};

inline std::ostream &operator<<(std::ostream &os, const TileWork &tileWork) {
	os << "TileWork[(" << tileWork.GetCoord().x << ", " << tileWork.GetCoord().y << ") => " <<
			"(" << tileWork.GetCoord().width << ", " << tileWork.GetCoord().height << "), " <<
			tileWork.passToRender << "]";

	return os;
}

//------------------------------------------------------------------------------
// TileRepository 
//------------------------------------------------------------------------------

class TileRepository {
public:
	TileRepository(const u_int tileWidth, const u_int tileHeight);
	~TileRepository();

	void Clear();
	void Restart(Film *film, const u_int startPass = 0, const u_int multipassIndex = 0);
	void GetPendingTiles(std::deque<const Tile *> &tiles);
	void GetNotConvergedTiles(std::deque<const Tile *> &tiles);
	void GetConvergedTiles(std::deque<const Tile *> &tiles);

	void InitTiles(const Film &film);
	bool NextTile(Film *film, boost::mutex *filmMutex,
		TileWork &tileWork, Film *tileFilm);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static TileRepository *FromProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();

	friend class Tile;

	u_int tileWidth, tileHeight;

	// If enableMultipassRendering is true, this will be the number of times the
	// rendering has been restarted
	u_int multipassRenderingIndex;

	float convergenceTestThreshold, convergenceTestThresholdReduction;
	u_int convergenceTestWarmUpSamples;
	VarianceClamping varianceClamping;
	bool enableMultipassRendering, enableRenderingDonePrint, enableFirstPassClear;

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
		bool operator()(const Tile *lt, const Tile *rt) const {
			return lt->pass > rt->pass;
		}
	};

	void HilberCurveTiles(
		std::vector<Tile::TileCoord> &coords,
		const Film &film,
		const u_int n, const int xo, const int yo,
		const int xd, const int yd, const int xp, const int yp,
		const int xEnd, const int yEnd);

	void SetDone(Film *film);
	bool GetNewTileWork(TileWork &tileWork);

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

BOOST_CLASS_VERSION(slg::Tile::TileCoord, 1)
BOOST_CLASS_VERSION(slg::Tile, 4)
BOOST_CLASS_VERSION(slg::TileRepository, 5)

BOOST_CLASS_EXPORT_KEY(slg::Tile::TileCoord)
BOOST_CLASS_EXPORT_KEY(slg::Tile)
BOOST_CLASS_EXPORT_KEY(slg::TileRepository)		

#endif	/* _SLG_TILEREPOSITORY_H */
