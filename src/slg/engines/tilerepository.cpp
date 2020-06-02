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
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/linear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Tile
//------------------------------------------------------------------------------

Tile::Tile(TileRepository *repo, const Film &film, const u_int index,
		const u_int tileX, const u_int tileY) :
			tileRepository(repo), tileIndex(index), pass(0), pendingPasses(0),
			error(numeric_limits<float>::infinity()),
			done(false), allPassFilm(NULL), evenPassFilm(NULL),
			allPassFilmTotalYValue(0.f), hasEnoughWarmUpSample(false) {
	const u_int *filmSubRegion = film.GetSubRegion();

	coord.x = tileX;
	coord.y = tileY;
	coord.width = Min(tileX + tileRepository->tileWidth, filmSubRegion[1] + 1) - tileX;
	coord.height = Min(tileY + tileRepository->tileHeight, filmSubRegion[3] + 1) - tileY;

	allPassFilm = NULL;
	evenPassFilm = NULL;
	const bool hasVarianceClamping = tileRepository->varianceClamping.hasClamping();
	const bool hasConvergenceTest = (tileRepository->enableMultipassRendering && (tileRepository->convergenceTestThreshold > 0.f));

	if (hasVarianceClamping || hasConvergenceTest)
		InitTileFilm(film, &allPassFilm);
	if (hasConvergenceTest)
		InitTileFilm(film, &evenPassFilm);
}

Tile::Tile() {
}

Tile::~Tile() {
	delete allPassFilm;
	delete evenPassFilm;
}

void Tile::InitTileFilm(const Film &film, Film **tileFilm) {
	(*tileFilm) = new Film(coord.width, coord.height);
	(*tileFilm)->CopyDynamicSettings(film);
	// Remove all channels but RADIANCE_PER_PIXEL_NORMALIZED and IMAGEPIPELINE
	(*tileFilm)->RemoveChannel(Film::ALPHA);
	(*tileFilm)->RemoveChannel(Film::DEPTH);
	(*tileFilm)->RemoveChannel(Film::POSITION);
	(*tileFilm)->RemoveChannel(Film::GEOMETRY_NORMAL);
	(*tileFilm)->RemoveChannel(Film::SHADING_NORMAL);
	(*tileFilm)->RemoveChannel(Film::MATERIAL_ID);
	(*tileFilm)->RemoveChannel(Film::DIRECT_GLOSSY);
	(*tileFilm)->RemoveChannel(Film::EMISSION);
	(*tileFilm)->RemoveChannel(Film::INDIRECT_DIFFUSE);
	(*tileFilm)->RemoveChannel(Film::INDIRECT_GLOSSY);
	(*tileFilm)->RemoveChannel(Film::INDIRECT_SPECULAR);
	(*tileFilm)->RemoveChannel(Film::MATERIAL_ID_MASK);
	(*tileFilm)->RemoveChannel(Film::DIRECT_SHADOW_MASK);
	(*tileFilm)->RemoveChannel(Film::INDIRECT_SHADOW_MASK);
	(*tileFilm)->RemoveChannel(Film::UV);
	(*tileFilm)->RemoveChannel(Film::RAYCOUNT);
	(*tileFilm)->RemoveChannel(Film::BY_MATERIAL_ID);
	(*tileFilm)->RemoveChannel(Film::IRRADIANCE);
	(*tileFilm)->RemoveChannel(Film::OBJECT_ID);
	(*tileFilm)->RemoveChannel(Film::OBJECT_ID_MASK);
	(*tileFilm)->RemoveChannel(Film::BY_OBJECT_ID);
	(*tileFilm)->RemoveChannel(Film::SAMPLECOUNT);
	(*tileFilm)->RemoveChannel(Film::CONVERGENCE);
	(*tileFilm)->RemoveChannel(Film::MATERIAL_ID_COLOR);
	(*tileFilm)->RemoveChannel(Film::ALBEDO);
	(*tileFilm)->RemoveChannel(Film::NOISE);

	// Build an image pipeline with only an auto-linear tone mapping and
	// gamma correction.
	unique_ptr<ImagePipeline> imagePipeline(new ImagePipeline());
	imagePipeline->AddPlugin(new LinearToneMap(1.f));
	imagePipeline->AddPlugin(new GammaCorrectionPlugin(2.2f));
	(*tileFilm)->SetImagePipelines(imagePipeline.release());

	// Disable OpenCL
	(*tileFilm)->hwEnable = false;
	
	// Disable denoiser statistics collection
	(*tileFilm)->GetDenoiser().SetEnabled(false);

	(*tileFilm)->Init();
}

void Tile::Restart(const u_int startPass) {
	if (allPassFilm)
		allPassFilm->Reset();
	if (evenPassFilm)
		evenPassFilm->Reset();

	pass = startPass;
	pendingPasses = 0;
	error = numeric_limits<float>::infinity();
	hasEnoughWarmUpSample = false;
	done = false;
	allPassFilmTotalYValue = 0.f;
}

void Tile::VarianceClamp(Film &tileFilm) {
	allPassFilm->VarianceClampFilm(tileRepository->varianceClamping, tileFilm);
}

void Tile::AddPass(Film &tileFilm, const u_int passRendered) {
	if (tileRepository->varianceClamping.hasClamping()) {
		// Apply variance clamping
		VarianceClamp(tileFilm);
	}

	// Increase the pass count
	++pass;

	// Update the done flag
	if (tileRepository->enableMultipassRendering) {
		// Check if convergence test is enable
		if (tileRepository->convergenceTestThreshold > 0.f) {
			// Add the tile to the all pass film
			allPassFilm->AddFilm(tileFilm);

			if (!hasEnoughWarmUpSample) {
				// Update tileRepository->filmTotalYValue and hasEnoughWarmUpSample
				UpdateTileStats();
			}

			if (passRendered % 2 == 1) {
				// If it is an odd pass, add also to the even pass film
				evenPassFilm->AddFilm(tileFilm);
			} else if (hasEnoughWarmUpSample) {
				// Update linear tone mapping plugin

				// This is the same scale of AutoLinearToneMap::CalcLinearToneMapScale() with gamma set to 1.0
				const float scale = AutoLinearToneMap::CalcLinearToneMapScale(*allPassFilm, 0,
						tileRepository->filmTotalYValue / (tileRepository->filmRegionWidth * tileRepository->filmRegionHeight));

				LinearToneMap *allLT = (LinearToneMap *)allPassFilm->GetImagePipeline(0)->GetPlugin(typeid(LinearToneMap));
				allLT->scale = scale;
				LinearToneMap *evenLT = (LinearToneMap *)evenPassFilm->GetImagePipeline(0)->GetPlugin(typeid(LinearToneMap));
				evenLT->scale = scale;

				// If it is an even pass, check convergence status
				CheckConvergence();
			}
		}
	} else
		done = true;
}

void Tile::UpdateTileStats() {
	float totalYValue = 0.f;
	const size_t channelCount = allPassFilm->GetRadianceGroupCount();

	hasEnoughWarmUpSample = true;
	for (u_int j = 0; j < channelCount; ++j) {
		for (u_int y = 0; y < coord.height; ++y) {
			for (u_int x = 0; x < coord.width; ++x) {		
				const float *pixel = allPassFilm->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[j]->GetPixel(x, y);

				if (pixel[3] > 0.f) {
					if (pixel[3] < tileRepository->convergenceTestWarmUpSamples)
						hasEnoughWarmUpSample = false;

					const float w = 1.f / pixel[3];
					const float Y = Spectrum(pixel[0] * w, pixel[1] * w, pixel[2] * w).Y();
					if ((Y <= 0.f) || isinf(Y))
						continue;

					totalYValue += Y;
				} else
					hasEnoughWarmUpSample = false;
			}
		}
	}

	// Remove old avg. luminance value and add the new one
	tileRepository->filmTotalYValue += totalYValue - allPassFilmTotalYValue;
	allPassFilmTotalYValue = totalYValue;
}

void Tile::CheckConvergence() {
	float maxError2 = 0.f;

	// Get the even pass pixel values
	evenPassFilm->ExecuteImagePipeline(0);
	const Spectrum *evenPassPixel = (const Spectrum *)evenPassFilm->channel_IMAGEPIPELINEs[0]->GetPixels();

	// Get the all pass pixel values
	allPassFilm->ExecuteImagePipeline(0);
	const Spectrum *allPassPixel = (const Spectrum *)allPassFilm->channel_IMAGEPIPELINEs[0]->GetPixels();

	// Compare the pixels result only of even passes with the result
	// of all passes
	for (u_int y = 0; y < coord.height; ++y) {
		for (u_int x = 0; x < coord.width; ++x) {
			// This is an variance estimation as defined in:
			//
			// "Progressive Path Tracing with Lightweight Local Error Estimation" paper
			//
			// The estimated variance of a pixel V(Pfull) is equal to (Pfull - Peven) ^ 2
			// where Pfull is the total pixel value while Peven is the value made
			// only of even samples.

			for (u_int k = 0; k < COLOR_SAMPLES; ++k) {
				// This is an variance estimation as defined in:
				//
				// "Progressive Path Tracing with Lightweight Local Error Estimation" paper
				//
				// The estimated variance of a pixel V(Pfull) is equal to (Pfull - Peven)^2
				// where Pfull is the total pixel value while Peven is the value made
				// only of even samples.

				const float diff = Clamp(allPassPixel->c[k], 0.f, 1.f) -
						Clamp(evenPassPixel->c[k], 0.f, 1.f);
				// I'm using variance^2 to avoid a sqrtf())
				//const float variance = diff *diff;
				const float variance2 = fabsf(diff);

				// Use variance^2 as error estimation
				const float error2 = variance2;

				maxError2 = Max(error2, maxError2);
			}

			++evenPassPixel;
			++allPassPixel;
		}
	}

	error = maxError2;
	done = (maxError2 < tileRepository->convergenceTestThreshold);
}

//------------------------------------------------------------------------------
// TileWork
//------------------------------------------------------------------------------

TileWork::TileWork() : tile(nullptr) {
}

TileWork::TileWork(Tile *t) {
	Init(t);
}

void TileWork::Init(Tile *t) {
	tile = t;
	tile->pendingPasses++;

	multipassIndexToRender = tile->tileRepository->multipassRenderingIndex;
	passToRender = tile->pass + tile->pendingPasses;
}

void TileWork::AddPass(Film &tileFilm) {		
	tile->AddPass(tileFilm, passToRender);
	if (tile->pendingPasses > 0)
		--tile->pendingPasses;
}

//------------------------------------------------------------------------------
// TileRepository
//------------------------------------------------------------------------------

TileRepository::TileRepository(const u_int tileW, const u_int tileH) {
	tileWidth = tileW;
	tileHeight = tileH;

	enableMultipassRendering = false;
	convergenceTestThreshold = 6.f / 256.f;
	convergenceTestThresholdReduction = 0.f;
	convergenceTestWarmUpSamples = 32;
	enableRenderingDonePrint = true;

	done = false;
	filmTotalYValue = 0.f;
	multipassRenderingIndex = 0;
}

TileRepository::TileRepository() {
}

TileRepository::~TileRepository() {
	Clear();
}

void TileRepository::Clear() {
	// Free all tiles in the 3 lists

	BOOST_FOREACH(Tile *tile, tileList) {
		delete tile;
	}

	tileList.clear();
	todoTiles.clear();
	pendingTiles.clear();
	convergedTiles.clear();
}

void TileRepository::Restart(Film *film, const u_int startPass, const u_int multipassIndex) {
	todoTiles.clear();
	pendingTiles.clear();
	convergedTiles.clear();

	BOOST_FOREACH(Tile *tile, tileList) {
		tile->Restart(startPass);
		todoTiles.push(tile);		
	}
	
	done = false;
	// Reset the film convergence, it may have been set by previous
	// rendering (for instance in RTPATHOCL)
	film->SetConvergence(0.f);
	filmTotalYValue = 0.f;

	multipassRenderingIndex = multipassIndex;
}

void TileRepository::GetPendingTiles(deque<const Tile *> &tiles) {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	tiles.insert(tiles.end(), pendingTiles.begin(), pendingTiles.end());
}

void TileRepository::GetNotConvergedTiles(deque<const Tile *> &tiles) {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	tiles.insert(tiles.end(), todoTiles.begin(), todoTiles.end());
}

void TileRepository::GetConvergedTiles(deque<const Tile *> &tiles) {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	tiles.insert(tiles.end(), convergedTiles.begin(), convergedTiles.end());
}

void TileRepository::HilberCurveTiles(
		vector<Tile::TileCoord> &coords,
		const Film &film,
		const u_int n,
		const int xo, const int yo,
		const int xd, const int yd,
		const int xp, const int yp,
		const int xEnd, const int yEnd) {
	if (n <= 1) {
		if((xo < xEnd) && (yo < yEnd))
			coords.push_back(Tile::TileCoord(xo, yo, tileWidth, tileHeight));
	} else {
		const u_int n2 = n >> 1;

		HilberCurveTiles(coords, film, n2,
			xo,
			yo,
			xp, yp, xd, yd, xEnd, yEnd);
		HilberCurveTiles(coords, film, n2,
			xo + xd * static_cast<int>(n2),
			yo + yd * static_cast<int>(n2),
			xd, yd, xp, yp, xEnd, yEnd);
		HilberCurveTiles(coords, film, n2,
			xo + (xp + xd) * static_cast<int>(n2),
			yo + (yp + yd) * static_cast<int>(n2),
			xd, yd, xp, yp, xEnd, yEnd);
		HilberCurveTiles(coords, film, n2,
			xo + xd * static_cast<int>(n2 - 1) + xp * static_cast<int>(n - 1),
			yo + yd * static_cast<int>(n2 - 1) + yp * static_cast<int>(n - 1),
			-xp, -yp, -xd, -yd, xEnd, yEnd);
	}
}

void TileRepository::InitTiles(const Film &film) {
	const double t1 = WallClockTime();

	const u_int *filmSubRegion = film.GetSubRegion();
	filmRegionWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
	filmRegionHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

	const u_int n = RoundUp(filmRegionWidth, tileWidth) / tileWidth;
	const u_int m = RoundUp(filmRegionHeight, tileHeight) / tileHeight;

	vector<Tile::TileCoord> coords;
	HilberCurveTiles(coords, film, RoundUpPow2(n * m),
			filmSubRegion[0], filmSubRegion[2],
			0, tileHeight,
			tileWidth, 0,
			filmSubRegion[1] + 1, filmSubRegion[3] + 1);

	// Initialize the list of tiles
	// To speedup the initialization process, work in parallel
	const u_int size = coords.size();
	tileList.resize(size, NULL);
	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < size; ++i)
		tileList[i] = new Tile(this, film, i, coords[i].x, coords[i].y);

	// Initialize also the TODO list
	BOOST_FOREACH(Tile *tile, tileList)
		todoTiles.push(tile);

	done = false;
	startTime = WallClockTime();

	const double elapsedTime = WallClockTime() - t1;
	SLG_LOG(boost::format("Tiles initialization time: %.2f secs") % elapsedTime);
}

void TileRepository::SetDone(Film *film) {
	// Rendering done
	if (!done) {
		if (enableRenderingDonePrint) {
			const double elapsedTime = WallClockTime() - startTime;
			SLG_LOG(boost::format("Rendering time: %.2f secs") % elapsedTime);
		}

		done = true;
		
		film->SetConvergence(1.f);
	}
}

bool TileRepository::GetNewTileWork(TileWork &tileWork) {
	// Look for the tile with less passes in pendingTiles
	Tile *pendingTile = nullptr;
	BOOST_FOREACH(Tile *tile, pendingTiles) {
		if ((!tile->done) &&
				(!pendingTile ||
				(pendingTile->pass + pendingTile->pendingPasses > tile->pass + tile->pendingPasses)))
			pendingTile = tile;
	}

	// Look for the tile with less passes in todoTiles
	Tile *toDoTile = (todoTiles.size() > 0) ? todoTiles.top() : nullptr;

	if (pendingTile) {
		if (toDoTile) {
			if (pendingTile->pass + pendingTile->pendingPasses < toDoTile->pass + toDoTile->pendingPasses)
				tileWork.Init(pendingTile);
			else {
				tileWork.Init(toDoTile);
				
				// Remove the tile form todo list
				todoTiles.pop();
			}
		} else
			tileWork.Init(pendingTile);
	} else {
		if (toDoTile) {
			tileWork.Init(toDoTile);

			// Remove the tile form todo list
			todoTiles.pop();
		} else {
			// This should never happen
			SLG_LOG("WARNING: out of tiles to render");

			return false;
		}
	}
		
	// Add the tile to the pending list (so it is counted multiple times
	// in case it was pendingTile)
	pendingTiles.push_back(tileWork.tile);

	return true;
}

bool TileRepository::NextTile(Film *film, boost::mutex *filmMutex,
		TileWork &tileWork, Film *tileFilm) {
	// Now I have to lock the repository
	boost::unique_lock<boost::mutex> lock(tileMutex);

	// Check if I have to add the tile to the film
	if (tileWork.HasWork()) {
		Tile *tile = tileWork.tile;

		// Add the pass to the tile
		tileWork.AddPass(*tileFilm);

		// Remove the first copy of tile from pending list (there can be multiple copy of the same tile)
		pendingTiles.erase(find(pendingTiles.begin(), pendingTiles.end(), tile));

		if (tile->done) {
			// All done for this tile, add to the convergedTiles list, if it is
			// not already there
			if (find(convergedTiles.begin(), convergedTiles.end(), tile) == convergedTiles.end())
				convergedTiles.push_back(tile);
		} else {
			// Re-add to the todoTiles priority queue, if it is not already there
			if (find(todoTiles.begin(), todoTiles.end(), tile) == todoTiles.end())
				todoTiles.push(tile);
		}

		// Add the tile also to the global film
		boost::unique_lock<boost::mutex> lock(*filmMutex);

		film->AddFilm(*tileFilm,
				0, 0,
				Min(tileWidth, film->GetWidth() - tile->coord.x),
				Min(tileHeight, film->GetHeight() - tile->coord.y),
				tile->coord.x, tile->coord.y);
	}

	// For the support of film halt conditions
	if (film->GetConvergence() == 1.f) {
		if (pendingTiles.size() == 0) {
			// Rendering done
			SetDone(film);
		}

		return false;
	}

	// For support of TileRepository halt condition and multi-pass rendering
	if (todoTiles.size() == 0) {
		if (!enableMultipassRendering) {
			// Multi-pass rendering disabled

			if (pendingTiles.size() == 0) {
				// Rendering done
				SetDone(film);
			}

			return false;
		} else {
			// Multi-pass rendering enabled

			// Check the status of pending tiles (one or more of them could be a
			// copy of mine and now done)
			bool pendingAllDone = true;
			BOOST_FOREACH(Tile *tile, pendingTiles) {
				if (!tile->done) {
					pendingAllDone = false;
					break;
				}
			}

			if (pendingAllDone) {
				if (convergenceTestThresholdReduction > 0.f) {
					// Reduce the target threshold and continue the rendering				
					if (enableRenderingDonePrint) {
						const double elapsedTime = WallClockTime() - startTime;
						SLG_LOG(boost::format("Threshold256 %.4f reached: %.2f secs") % (256.f * convergenceTestThreshold) % elapsedTime);
					}

					convergenceTestThreshold *= convergenceTestThresholdReduction;

					// Restart the rendering for all tiles

					// I need to save a copy of the current pending tile list because
					// it can be not empty. I could just avoid to clear the list but is
					// more readable (an safer for the Restart() method) to work in this
					// way.
					deque<Tile *> currentPendingTiles = pendingTiles;
					Restart(film, 0, multipassRenderingIndex + 1);
					pendingTiles = currentPendingTiles;
				} else {
					if (pendingTiles.size() == 0) {
						// Rendering done
						SetDone(film);
					}

					return false;
				}
			}
		}
	}

	// Get the next tile to render
	return GetNewTileWork(tileWork);
}

Properties TileRepository::ToProperties(const Properties &cfg) {
	Properties props;

	// tile.size
	const u_int defaultSize = cfg.Get(GetDefaultProps().Get("tile.size")).Get<u_int>();
	const Property sizeX = cfg.Get(Property("tile.size.x")(defaultSize));
	const Property sizeY = cfg.Get(Property("tile.size.y")(defaultSize));

	if (sizeX.Get<u_int>() == sizeY.Get<u_int>())
		props << Property("tile.size")(sizeX.Get<u_int>());
	else
		props << sizeX << sizeY;

	// tile.multipass.convergencetest.threshold
	if (cfg.IsDefined("tile.multipass.convergencetest.threshold"))
		props << cfg.Get(GetDefaultProps().Get("tile.multipass.convergencetest.threshold"));
	else {
		const float defaultThreshold = GetDefaultProps().Get("tile.multipass.convergencetest.threshold").Get<float>();
		props << cfg.Get(Property("tile.multipass.convergencetest.threshold256")(defaultThreshold * 256.f));
	}

	props <<
			cfg.Get(GetDefaultProps().Get("tile.multipass.enable")) <<
			cfg.Get(GetDefaultProps().Get("tile.multipass.convergencetest.threshold.reduction")) <<
			cfg.Get(GetDefaultProps().Get("tile.multipass.convergencetest.warmup.count"));

	return props;
}

TileRepository *TileRepository::FromProperties(const luxrays::Properties &cfg) {
	u_int tileWidth = 32;
	u_int tileHeight = 32;
	if (cfg.IsDefined("tile.size"))
		tileWidth = tileHeight = Max(8u, cfg.Get(GetDefaultProps().Get("tile.size")).Get<u_int>());
	tileWidth = Max(8u, cfg.Get(Property("tile.size.x")(tileWidth)).Get<u_int>());
	tileHeight = Max(8u, cfg.Get(Property("tile.size.y")(tileHeight)).Get<u_int>());
	unique_ptr<TileRepository> tileRepository(new TileRepository(tileWidth, tileHeight));

	tileRepository->enableMultipassRendering = cfg.Get(GetDefaultProps().Get("tile.multipass.enable")).Get<bool>();

	if (cfg.IsDefined("tile.multipass.convergencetest.threshold"))
		tileRepository->convergenceTestThreshold = cfg.Get(GetDefaultProps().Get("tile.multipass.convergencetest.threshold")).Get<float>();
	else {
		const float defaultThreshold256 = 256.f * GetDefaultProps().Get("tile.multipass.convergencetest.threshold").Get<float>();
		tileRepository->convergenceTestThreshold = cfg.Get(Property("tile.multipass.convergencetest.threshold256")(defaultThreshold256)).Get<float>() * (1.f / 256.f);
	}

	tileRepository->convergenceTestThresholdReduction = cfg.Get(GetDefaultProps().Get("tile.multipass.convergencetest.threshold.reduction")).Get<float>();
	tileRepository->convergenceTestWarmUpSamples = cfg.Get(GetDefaultProps().Get("tile.multipass.convergencetest.warmup.count")).Get<u_int>();

	return tileRepository.release();
}

const Properties &TileRepository::GetDefaultProps() {
	static Properties props =  Properties() <<
			Property("tile.size")(32) <<
			Property("tile.multipass.enable")(true) <<
			Property("tile.multipass.convergencetest.threshold")(6.f / 256.f) <<
			Property("tile.multipass.convergencetest.threshold.reduction")(0.f) <<
			Property("tile.multipass.convergencetest.warmup.count")(32);

	return props;
}
