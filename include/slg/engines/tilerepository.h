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

#ifndef _SLG_TILEREPOSITORY_H
#define	_SLG_TILEREPOSITORY_H

#include "luxrays/core/utils.h"

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

		void Restart();
		void VarianceClamp(Film &tileFilm);
		void AddPass(const Film &tileFilm);
		
		// Read-only for every one but Tile/TileRepository classes
		u_int xStart, yStart, tileWidth, tileHeight;
		u_int pass;
		float error;
		bool done;

	private:
		void InitTileFilm(const Film &film, Film **tileFilm);
		void CheckConvergence();
		void UpdateTileStats();

		TileRepository *tileRepository;
		Film *allPassFilm, *evenPassFilm;

		float allPassFilmTotalYValue;
		bool hasEnoughWarmUpSample;
	};

	TileRepository(const u_int tileWidth, const u_int tileHeight);
	~TileRepository();

	void Clear();
	void Restart();
	void GetPendingTiles(std::deque<const Tile *> &tiles);
	void GetNotConvergedTiles(std::deque<const Tile *> &tiles);
	void GetConvergedTiles(std::deque<const Tile *> &tiles);

	void InitTiles(const Film &film);
	bool NextTile(Film *film, boost::mutex *filmMutex,
		Tile **tile, Film *tileFilm);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static TileRepository *FromProperties(const luxrays::Properties &cfg);
	static luxrays::Properties GetDefaultProps();

	friend class Tile;

	u_int tileWidth, tileHeight;

	u_int maxPassCount;
	float convergenceTestThreshold, convergenceTestThresholdReduction;
	u_int convergenceTestWarmUpSamples;
	VarianceClamping varianceClamping;
	bool enableMultipassRendering, enableRenderingDonePrint;

	bool done;

private:
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

	boost::mutex tileMutex;
	double startTime;

	u_int filmWidth, filmHeight;
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

#endif	/* _SLG_TILEREPOSITORY_H */
