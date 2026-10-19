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

#pragma once


#include <array>
#include <span>
#include <bit>
#include <memory>

namespace luxrays {


inline constexpr auto NOPAD = std::array<std::byte,0>();

// A container for mesh components: points, normals etc.
// Can be end-padded, for the sake of embree or integrity check
//
// The container can be accessed via 3 levels:
// - Main objects, known as TYPE: points, triangles etc.
// - Underlying objects of main objects, known as SUBTYPE: float, size_t etc.
// - bytes 
//
// To spare compile time, the template is delibaretely intended not to
// be implicitely instantiable.
// Definition and instantiations are in cpp file
//
// As a convention:
// "size" is in bytes
// "count" is in TYPE elements
// "subcount" is in SUBTYPE elements
template< typename TYPE, typename SUBTYPE, std::array PAD=NOPAD >
class Buffer {

public:

	// Constructors
	inline Buffer() = default;
	explicit Buffer(std::size_t);
	explicit Buffer(std::span<const TYPE>);
	explicit Buffer(std::span<const SUBTYPE>);

	// Move is ok
	inline Buffer(Buffer&&) = default;
	inline Buffer& operator=(Buffer&&) = default;

	// No copy allowed
	Buffer(Buffer&) = delete;
	Buffer& operator=(Buffer&) = delete;

	// Allocate internal container for count objects of type TYPE
	void Allocate(std::size_t count);

	// Returns a Buffer sharing the storage of 'from', without copying the
	// content: the shared content must never be modified afterwards
	static Buffer Share(const Buffer &from);

	// Getters
	std::span<TYPE> GetObjects() const;
	std::span<SUBTYPE> GetSubObjects() const;
	std::span<std::byte> GetBytes(bool withPad=false) const;

	// Get pad value
	// Pad is directly read in buffer, so as it allows to check integrity
	std::span<const std::byte> GetPad() const;

	// Setters
	void Set(const Buffer<TYPE, SUBTYPE, PAD>& from);
	void Set(std::span<const TYPE> from);
	void Set(std::span<const SUBTYPE> from);

	// Subset
	std::span<TYPE> Subset(std::size_t offset, std::size_t count = std::dynamic_extent);

	// Indexation
	TYPE& operator[](size_t index);
	const TYPE& operator[](size_t index) const;

	// Implicit conversion operator
	operator std::span<TYPE>() const;

	// Element count (in TYPE elements)
	size_t Count() const;

	// Underlying structure (const)
	void * Data() const;

	// Emptiness
	explicit operator bool() const noexcept;


private:
	// Underlying storage (shared, to allow Buffer::Share)
	std::shared_ptr<std::byte[]> data;
	static constexpr std::array pad{PAD};

	// Sizes (in bytes)
	size_t totalSize = 0;
	size_t effectiveSize = 0;
	static constexpr size_t padSize = std::size(PAD);

	// Spans
	std::span<TYPE> asType;
	std::span<SUBTYPE> asSubType;

};


// To compute padding for Buffer
template <typename T>
constexpr std::array<std::byte, sizeof(T)> to_bytes(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "to_bytes requires a trivially copyable type");
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}
inline constexpr auto VERTEXPAD = to_bytes(1234.1234f);

// Containers for points and triangles
class Point;
using VertexBuffer = Buffer<Point, float, VERTEXPAD>;
class Triangle;
using TriangleBuffer = Buffer<Triangle, unsigned int, NOPAD>;
class Normal;
using NormalBuffer = Buffer<Normal, float, NOPAD>;



}  // Namespace luxrays

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
