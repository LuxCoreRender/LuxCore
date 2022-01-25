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

#ifndef _LUXRAYS_MEMORY_H
#define _LUXRAYS_MEMORY_H

#if !defined(__APPLE__) && !defined(__OpenBSD__) && !defined(__FreeBSD__)
#  include <malloc.h> // for _alloca, memalign
#  if !defined(WIN32) || defined(__CYGWIN__)
#    include <alloca.h>
#  else
#    define memalign(a,b) _aligned_malloc(b, a)
#    define alloca _alloca
#  endif
#elif defined(__APPLE__)
#  define memalign(a,b) valloc(b)
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
#  define memalign(a,b) malloc(b)
#endif

#include <vector>

namespace luxrays {

// Memory Allocation Functions
#if !defined(LUX_ALIGNMENT)
#ifdef LUX_USE_SSE
#define LUX_ALIGNMENT 16
#endif
#endif

#ifndef L1_CACHE_LINE_SIZE
#define L1_CACHE_LINE_SIZE 64
#endif

template<class T> inline T *AllocAligned(size_t size, std::size_t N = L1_CACHE_LINE_SIZE)
{
	return static_cast<T *>(memalign(N, size * sizeof(T)));
}
template<class T> inline void FreeAligned(T *ptr)
{
#if defined(WIN32) && !defined(__CYGWIN__) // NOBOOK
	_aligned_free(ptr);
#else // NOBOOK
	free(ptr);
#endif // NOBOOK
}

template <typename T, std::size_t N = 16> class AlignedAllocator
{
public:
	typedef T value_type;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

	typedef T *pointer;
	typedef const T *const_pointer;

	typedef T &reference;
	typedef const T &const_reference;

public:
	inline AlignedAllocator() throw() { }

	template <typename T2> inline AlignedAllocator(const AlignedAllocator<T2, N> &) throw () { }

	inline ~AlignedAllocator() throw() { }

	inline pointer adress(reference r) { return &r; }

	inline const_pointer adress(const_reference r) const { return &r; }

	inline pointer allocate(size_type n)
	{
		return AllocAligned<value_type>(n, N);
	}

	inline void deallocate(pointer p, size_type)
	{
		FreeAligned(p);
	}

	inline void construct(pointer p, const value_type &wert)
	{
		new (p) value_type(wert);
	}

	inline void destroy(pointer p)
	{
		p->~value_type();
	}

	inline size_type max_size() const throw()
	{
		return size_type(-1) / sizeof(value_type);
	}

	template <typename T2> struct rebind
	{
		typedef AlignedAllocator<T2, N> other;
	};
};

#define P_CLASS_ATTR __attribute__
#define P_CLASS_ATTR __attribute__

#if defined(WIN32) && !defined(__CYGWIN__) // NOBOOK
class __declspec(align(16)) Aligned16 {
#else // NOBOOK
class Aligned16 {
#endif // NOBOOK
public:

	/*
	Aligned16(){
		if(((int)this & 15) != 0){
			printf("bad alloc\n");
			assert(0);
		}
	}
	*/

	void *operator new(size_t s) { return AllocAligned<char>(s, 16); }

	void *operator new (size_t s, void *q) { return q; }

	void operator delete(void *p) { FreeAligned(p); }
#if defined(WIN32) && !defined(__CYGWIN__) // NOBOOK
} ;
#else // NOBOOK
} __attribute__ ((aligned(16)));
#endif // NOBOOK

/*
template <class T> class ObjectArena {
public:
	// ObjectArena Public Methods
	ObjectArena() {
		nAvailable = 0;
	}
	T *Alloc() {
		if (nAvailable == 0) {
			int nAlloc = max((unsigned long)16,
				(unsigned long)(65536/sizeof(T)));
			mem = (T *)AllocAligned(nAlloc * sizeof(T));
			nAvailable = nAlloc;
			toDelete.push_back(mem);
		}
		--nAvailable;
		return mem++;
	}
	operator T *() {
		return Alloc();
	}
	~ObjectArena() { FreeAll(); }
	void FreeAll() {
		for (u_int i = 0; i < toDelete.size(); ++i)
			FreeAligned(toDelete[i]);
		toDelete.erase(toDelete.begin(), toDelete.end());
		nAvailable = 0;
	}


private:
	// ObjectArena Private Data
	T *mem;
	int nAvailable;
	vector<T *> toDelete;
};*/

class  MemoryArena {
public:
	// MemoryArena Public Methods
	MemoryArena(size_t bs = 32768) {
		blockSize = bs;
		curBlockPos = 0;
		currentBlockIdx = 0;
		blocks.push_back(AllocAligned<int8_t>(blockSize));
		beginBlockPos = 0;
		beginBlockIdx = 0;
	}
	~MemoryArena() {
		for (size_t i = 0; i < blocks.size(); ++i)
			FreeAligned(blocks[i]);
	}
	void *Alloc(size_t sz) {
		// Round up _sz_ to minimum machine alignment
#if defined(LUX_ALIGNMENT)
		sz = ((sz + (LUX_ALIGNMENT-1)) & (~(LUX_ALIGNMENT-1)));
#else
		sz = ((sz + 7) & (~7U));
#endif
		if (curBlockPos + sz > blockSize) {
			// Get new block of memory for _MemoryArena_
			currentBlockIdx++;

			if(currentBlockIdx == blocks.size())
				blocks.push_back(AllocAligned<int8_t>(std::max(sz, blockSize)));

			curBlockPos = 0;
		}
		void *ret = blocks[currentBlockIdx] + curBlockPos;
		curBlockPos += sz;
		return ret;
	}
	void FreeAll() {
		curBlockPos = 0;
		currentBlockIdx = 0;
		beginBlockPos = 0;
		beginBlockIdx = 0;
	}

	// Those function helps the MemoryArena to only save what is needed for
	// further usage.

	// After you call Begin, everything in the MemoryArena which has not been
	// Committed is invalidated.
	// End put a synchronisation point
	// If you call Commit, everything between the Begin and the End() call is
	// guaranteed to be saved in the Arena until the next FreeAll()

	// Example:
	// AllocateSomethingOnMA(a)
	// AllocateSomethingOnMA(b)
	// arena.Begin(); // a and b are invalidated
	// AllocateSomethingOnMA(c)
	// arena.Sync();
	// AllocateSomethingOnMA(d);
	// arena.Commit(); // d is invalidated, c will stay on the arena

	void Begin()
	{
		currentBlockIdx = beginBlockIdx;
		curBlockPos = beginBlockPos;
	}

	void End()
	{
		endBlockPos = curBlockPos;
		endBlockIdx = currentBlockIdx;
	}

	void Commit()
	{
		beginBlockIdx = endBlockIdx;
		beginBlockPos = endBlockPos;
	}
private:
	// MemoryArena Private Data
	size_t curBlockPos, blockSize, beginBlockPos, endBlockPos;

	unsigned int currentBlockIdx, beginBlockIdx, endBlockIdx;
	std::vector<int8_t *> blocks;
};
#define ARENA_ALLOC(ARENA,T)  new ((ARENA).Alloc(sizeof(T))) T

template<class T, int logBlockSize = 2> class BlockedArray {
public:
	friend class boost::serialization::access;
	// BlockedArray Public Methods
	BlockedArray () {}
	BlockedArray(const BlockedArray &b, const T *d = NULL)
	{
		uRes = b.uRes;
		vRes = b.vRes;
		uBlocks = RoundUp(uRes) >> logBlockSize;
		size_t nAlloc = RoundUp(uRes) * RoundUp(vRes);
		data = AllocAligned<T>(nAlloc);
		if (!data) {
			uRes = 0;
			vRes = 0;
			return;
		}
		for (size_t i = 0; i < nAlloc; ++i)
			new (&data[i]) T(b.data[i]);
		if (d) {
			for (size_t v = 0; v < b.vRes; ++v) {
				for (size_t u = 0; u < b.uRes; ++u)
					(*this)(u, v) = d[v * uRes + u];
			}
		}
	}
	BlockedArray(size_t nu, size_t nv, const T *d = NULL) {
		uRes = nu;
		vRes = nv;
		uBlocks = RoundUp(uRes) >> logBlockSize;
		size_t nAlloc = RoundUp(uRes) * RoundUp(vRes);
		data = AllocAligned<T>(nAlloc);
		if (!data) {
			uRes = 0;
			vRes = 0;
			return;
		}
		for (size_t i = 0; i < nAlloc; ++i)
			new (&data[i]) T();
		if (d) {
			for (size_t v = 0; v < nv; ++v) {
				for (size_t u = 0; u < nu; ++u)
					(*this)(u, v) = d[v * uRes + u];
			}
		}
	}
	void Fill(const T d) {
		for (size_t v = 0; v < vRes; ++v) {
			for (size_t u = 0; u < uRes; ++u)
				(*this)(u, v) = d;
		}
	}
	size_t BlockSize() const { return 1 << logBlockSize; }
	size_t RoundUp(size_t x) const {
		return (x + BlockSize() - 1) & ~(BlockSize() - 1);
	}
	size_t uSize() const { return uRes; }
	size_t vSize() const { return vRes; }
	~BlockedArray() {
		for (size_t i = 0; i < uRes * vRes; ++i)
			data[i].~T();
		FreeAligned(data);
	}
	size_t Block(size_t a) const { return a >> logBlockSize; }
	size_t Offset(size_t a) const { return (a & (BlockSize() - 1)); }
	T &operator()(size_t u, size_t v) {
		size_t bu = Block(u), bv = Block(v);
		size_t ou = Offset(u), ov = Offset(v);
		size_t offset = BlockSize() * BlockSize() * (uBlocks * bv + bu);
		offset += BlockSize() * ov + ou;
		return data[offset];
	}
	const T &operator()(size_t u, size_t v) const {
		size_t bu = Block(u), bv = Block(v);
		size_t ou = Offset(u), ov = Offset(v);
		size_t offset = BlockSize() * BlockSize() * (uBlocks * bv + bu);
		offset += BlockSize() * ov + ou;
		return data[offset];
	}
	void GetLinearArray(T *a) const {
		for (size_t v = 0; v < vRes; ++v) {
			for (size_t u = 0; u < uRes; ++u)
				*a++ = (*this)(u, v);
		}
	}

private:
	// BlockedArray Private Data
	T *data;
	size_t uRes, vRes, uBlocks;
	
	template<class Archive> void save(Archive & ar, const unsigned int version) const
	{
		ar & uRes;
		ar & vRes;
		ar & uBlocks;

		size_t nAlloc = RoundUp(uRes) * RoundUp(vRes);
		for (size_t i = 0; i < nAlloc; ++i)
			ar & data[i];
	}

	template<class Archive>	void load(Archive & ar, const unsigned int version)
	{
		ar & uRes;
		ar & vRes;
		ar & uBlocks;

		size_t nAlloc = RoundUp(uRes) * RoundUp(vRes);
		data = AllocAligned<T>(nAlloc);
		for (size_t i = 0; i < nAlloc; ++i)
			ar & data[i];
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

}

#endif // _LUXRAYS_MEMORY_H
