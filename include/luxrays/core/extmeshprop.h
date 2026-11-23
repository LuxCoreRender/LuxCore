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

#ifndef _LUXRAYS_EXTMESHPROP_H
#define	_LUXRAYS_EXTMESHPROP_H

#include <cassert>
#include <array>
#include <memory>
#include <span>
#include <execution>
#include <optional>
#include <tuple>

// EXTMESH_MAX_DATA_COUNT is defined here (rather than in exttrianglemesh_types.cl)
// so that this header stays self-contained: the .cl file also carries OpenCL
// kernel struct definitions that depend on geometry types not available here.
#define EXTMESH_MAX_DATA_COUNT 8u

#include "luxrays/luxrays.h"
#include "luxrays/utils/serializationutils.h"

namespace luxrays {

// ExtMeshProp: a container for a property of Extended Mesh (uv, colors, alphas...)
//
// We use shared pointer, as it allows shallow copy (can be useful for such
// large data sets)
// All methods run on a per layer basis, except those suffixed by 'All', which
// run on all layers
template <typename T>
class ExtMeshProp : std::array<std::shared_ptr<T[]>, EXTMESH_MAX_DATA_COUNT> {
public:
	using Layer = std::shared_ptr<T[]>;
	using Base = std::array<Layer, EXTMESH_MAX_DATA_COUNT>;

	ExtMeshProp() { this->fill(nullptr); }

	// Construct from a single layer of data
	ExtMeshProp(Layer layer, size_t layerSize = 0) : ExtMeshProp() {
		if (layer) {
			(*this)[0] = layer;
			if (layerSize) _size = layerSize;
		}
	}

	// Per-layer
	void AllocateLayer(const u_int layerIndex, size_t layerSize) {
		assert(!_size || _size == layerSize);  // Should always be the same size once it
											   // has been set
		// A zero-size array is not null: keep the layer null or LayerHasValues
		// would report it as having values
		(*this)[layerIndex] = (layerSize > 0) ? std::make_shared<T[]>(layerSize) : Layer(nullptr);
		if (layerSize) _size = layerSize;
	}
	auto GetLayer(const u_int layerIndex) const {
		assert(layerIndex < GetMaxLayerNumber());
		return (*this)[layerIndex];
	}
	void SetLayer(const u_int layerIndex, Layer values, size_t layerSize) {
		assert(!_size || _size == layerSize);
		(*this)[layerIndex] = values;
		if (layerSize) _size = layerSize;
	}
	void SetLayer(const u_int layerIndex, std::tuple<Layer, size_t> in) {
		auto [layer, layerSize] = in;
		SetLayer(layerIndex, layer, layerSize);
	}
	void SetLayer(const u_int layerIndex, std::span<T> in) {
		auto layerSize = in.size();
		assert(!_size || _size == layerSize);

		AllocateLayer(layerIndex, layerSize);
		auto layerSpan = GetLayerSpan(layerIndex);

		std::copy(
#if !defined(__clang__)
			std::execution::par,
#endif
			in.begin(),
			in.end(),
			layerSpan.begin()
		);
		if (layerSize) _size = layerSize;
	}

	void DeleteLayer(const u_int layerIndex) {
		(*this)[layerIndex].reset();
		(*this)[layerIndex] = nullptr;
	}
	bool LayerHasValues(const u_int layerIndex) const {
		return (*this)[layerIndex] != nullptr;
	}

	std::span<T> GetLayerSpan(const u_int layerIndex) const {
		auto& layer = (*this)[layerIndex];
		if (!layer) return std::span<T>();
		return std::span<T>(layer.get(), _size);
	}

	std::tuple<Layer, size_t> CopyLayer(
		const u_int layerIndex,
		const std::optional<ExtMeshProp<T>> force = std::nullopt
	) const {
		if (force) {
			return std::make_tuple(force->GetLayer(layerIndex), force->GetLayerSize());
		}

		if (LayerHasValues(layerIndex)) {
			auto res = std::make_shared<T[]>(GetLayerSize());

			auto srcSpan = GetLayerSpan(layerIndex);
			auto dstSpan = std::span(res.get(), GetLayerSize());
			std::copy(
#if !defined(__clang__)
				std::execution::par,
#endif
				srcSpan.begin(),
				srcSpan.end(),
				dstSpan.begin()
			);

			return std::make_tuple(res, GetLayerSize());
		}

		return std::make_tuple(Layer(nullptr), 0);
	};

	// All layers
	void DeleteAll() {
		for (size_t i = 0; i < this->size(); ++i)
			DeleteLayer(i);
	}
	size_t GetLayerSize() const { return _size; }

	// Serialization
	template<typename Archive>
	void Serialize(const u_int layerIndex, Archive& ar, size_t count) const {
		const bool hasValues = LayerHasValues(layerIndex);
		ar & hasValues;
		if (hasValues) {
			auto data = (*this)[layerIndex].get();
			ar & boost::serialization::make_array(data, count);
		}
	}

	template<typename Archive>
	void Deserialize(const u_int layerIndex, Archive& ar, size_t count) {
		bool hasValues;
		ar & hasValues;
		if (hasValues) {
			AllocateLayer(layerIndex, count);
			auto data = (*this)[layerIndex].get();
			ar & boost::serialization::make_array(data, count);
		} else {
			(*this)[layerIndex] = nullptr;
		}
	}

	// Base
	using Base::operator[];  // TODO Make private
	auto GetMaxLayerNumber() const { return Base::size(); }  // Size of the array

private:
	size_t _size = 0;

};

}  // namespace luxrays

#endif	/* _LUXRAYS_EXTMESHPROP_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
