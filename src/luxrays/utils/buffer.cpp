/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/utils/buffer.h"

#include <span>

#include <execution>

using namespace luxrays;

// The minimum buffer size, in bytes, above which the copies are done in
// parallel
constexpr size_t PARALLEL_COPY_THRESHOLD = 1024 * 1024;

//------------------------------------------------------------------------------
// Buffer
//------------------------------------------------------------------------------


// Constructors
template< typename TYPE, typename SUBTYPE, std::array PAD >
Buffer<TYPE, SUBTYPE, PAD>::Buffer(size_t size) {
	Allocate(size);
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
Buffer<TYPE, SUBTYPE, PAD>::Buffer(std::span<TYPE const> objs) {
	Allocate(objs.size());
	std::copy(objs.begin(), objs.end(), asType.begin());
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
Buffer<TYPE, SUBTYPE, PAD>::Buffer(std::span<SUBTYPE const> subobjs) {
	Allocate(subobjs.size() * sizeof(SUBTYPE) / sizeof(TYPE) );
	std::copy(subobjs.begin(), subobjs.end(), asSubType.begin());
}

// Allocator
template< typename TYPE, typename SUBTYPE, std::array PAD >
void Buffer<TYPE, SUBTYPE, PAD>::Allocate(size_t count) {

	// Compute sizes
	effectiveSize = sizeof(TYPE) * count;
	totalSize = effectiveSize + padSize;

	// Allocate the buffer without initializing it: the content is always
	// overwritten by the callers, with Set or a fill pass
	data = std::unique_ptr<std::byte[]>(new std::byte[totalSize]);

	// Add padding
	// Embree requires a padding field at the end
	// This is a trick so I can check if the buffer has been really allocated
	// with AllocVerticesBuffer() or not. It is useful for debugging LuxCore
	// applications.
	std::copy(pad.begin(), pad.end(), data.get() + effectiveSize);

	// Compute spans
	auto makeSpan = [&]<typename T>() {
		size_t size = effectiveSize / sizeof(T);
		auto * ptr = reinterpret_cast<T*>(data.get());
		return std::span<T>(ptr, size);
	};

	asType = makeSpan.template operator()<TYPE>();
	asSubType = makeSpan.template operator()<SUBTYPE>();

}


// Getters
template< typename TYPE, typename SUBTYPE, std::array PAD >
std::span<TYPE> Buffer<TYPE, SUBTYPE, PAD>::GetObjects() const {
	// Compute size (without padding)
	size_t size = effectiveSize / sizeof(TYPE);

	// Make span
	auto * ptr = reinterpret_cast<TYPE*>(data.get());
	return std::span<TYPE>(ptr, size);
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
std::span<SUBTYPE> Buffer<TYPE, SUBTYPE, PAD>::GetSubObjects() const {
	// Compute size (without padding)
	size_t size = effectiveSize / sizeof(SUBTYPE);

	// Make span
	auto * ptr = reinterpret_cast<SUBTYPE*>(data.get());
	return std::span<SUBTYPE>(ptr, size);
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
std::span<std::byte> Buffer<TYPE, SUBTYPE, PAD>::GetBytes(bool withPad) const {
	return std::span<std::byte>(data.get(), withPad ? totalSize : effectiveSize);
}


template< typename TYPE, typename SUBTYPE, std::array PAD >
std::span<const std::byte> Buffer<TYPE, SUBTYPE, PAD>::GetPad() const {
	return GetBytes(true).subspan(effectiveSize);
}


// Subset
template< typename TYPE, typename SUBTYPE, std::array PAD >
std::span<TYPE> Buffer<TYPE, SUBTYPE, PAD>::Subset(std::size_t offset, std::size_t count) {
	return asType.subspan(offset, count);
}

// Set: Copy 'from' into 'this'
// Realloc if needed
template< typename TYPE, typename SUBTYPE, std::array PAD >
void Buffer<TYPE, SUBTYPE, PAD>::Set(const Buffer<TYPE, SUBTYPE, PAD>& from) {
	// Check memory and adapt if necessary
	if (effectiveSize != from.effectiveSize) {
		Allocate(from.effectiveSize);
	}

	// Copy, in parallel for the large buffers
	const auto from_bytes = std::span<std::byte>(from.data.get(), from.totalSize);
	auto to_bytes = std::span<std::byte>(data.get(), totalSize);

	if (from_bytes.size() > PARALLEL_COPY_THRESHOLD)
		std::copy(std::execution::par, from_bytes.begin(), from_bytes.end(), to_bytes.begin());
	else
		std::copy(from_bytes.begin(), from_bytes.end(), to_bytes.begin());
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
void Buffer<TYPE, SUBTYPE, PAD>::Set(std::span<const TYPE> from) {
	// Check memory and adapt if necessary
	const size_t from_count = from.size();
	if (Count() != from_count) {
		Allocate(from_count);
	}

	// Copy, in parallel for the large buffers
	if (from.size() * sizeof(TYPE) > PARALLEL_COPY_THRESHOLD)
		std::copy(std::execution::par, from.begin(), from.end(), asType.begin());
	else
		std::copy(from.begin(), from.end(), asType.begin());
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
void Buffer<TYPE, SUBTYPE, PAD>::Set(std::span<const SUBTYPE> from) {
	// Check memory and adapt if necessary
	const size_t from_size = from.size() * sizeof(SUBTYPE);  // from_size in bytes
	assert(from_size % sizeof(TYPE) == 0);
	if (effectiveSize != from_size) {
		const size_t count = from_size / sizeof(TYPE);
		Allocate(count);
	}

	// Copy, in parallel for the large buffers
	if (from.size() * sizeof(SUBTYPE) > PARALLEL_COPY_THRESHOLD)
		std::copy(std::execution::par, from.begin(), from.end(), asSubType.begin());
	else
		std::copy(from.begin(), from.end(), asSubType.begin());
}


// Indexation
template< typename TYPE, typename SUBTYPE, std::array PAD >
TYPE& Buffer<TYPE, SUBTYPE, PAD>::operator[](size_t index) {
        return asType[index];
}

template< typename TYPE, typename SUBTYPE, std::array PAD >
const TYPE& Buffer<TYPE, SUBTYPE, PAD>::operator[](size_t index) const {
        return asType[index];
}

// Implicit conversion operator
template< typename TYPE, typename SUBTYPE, std::array PAD >
Buffer<TYPE, SUBTYPE, PAD>::operator std::span<TYPE>() const {
        return asType;
}

// Element count
template< typename TYPE, typename SUBTYPE, std::array PAD >
size_t Buffer<TYPE, SUBTYPE, PAD>::Count() const {
	return asType.size();
}

// Underlying structure
template< typename TYPE, typename SUBTYPE, std::array PAD >
void * Buffer<TYPE, SUBTYPE, PAD>::Data() const {
	return data.get();
}

// Emptiness
template< typename TYPE, typename SUBTYPE, std::array PAD >
Buffer<TYPE, SUBTYPE, PAD>::operator bool() const noexcept {
	return not asType.empty();
}


// Instanciations
template class luxrays::Buffer<luxrays::Point, float, VERTEXPAD>;
template class luxrays::Buffer<luxrays::Triangle, Triangle::subtype_t, NOPAD>;
template class luxrays::Buffer<luxrays::Normal, float, NOPAD>;

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
