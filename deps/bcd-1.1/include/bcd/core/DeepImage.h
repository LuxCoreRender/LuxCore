// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef DEEP_IMAGE_H
#define DEEP_IMAGE_H



#include <vector>
#include <iostream>

namespace bcd
{

	class PixelPosition
	{
	public:
		PixelPosition();
		PixelPosition(int i_line, int i_column);
		PixelPosition(const PixelPosition& i_rPos);
		PixelPosition& operator=(const PixelPosition& i_rPos);

	public:
		int m_line;
		int m_column;

	public:
		void get(int& o_rLine, int& o_rColumn) const;
		PixelPosition operator+(const PixelPosition& i_rPix) const;
		PixelPosition operator-(const PixelPosition& i_rPix) const;
		bool operator==(const PixelPosition& i_rPix) const;
		bool operator!=(const PixelPosition& i_rPix) const;
	};

	typedef PixelPosition PixelVector;
	typedef PixelPosition PixelWindowSize;
	typedef PixelPosition PatchSize;
	typedef PixelPosition ImageWindowSize;

	class PixelWindow
	{
	public:
		class iterator
		{
		public:
			iterator();
			iterator(PixelPosition i_centralPixel, int i_radius);
			iterator(
					int i_bufferWidth,
					int i_bufferHeight,
					PixelPosition i_centralPixel,
					int i_radius,
					int i_border = 0);
			iterator(
					PixelPosition i_minCorner,
					PixelPosition i_maxCorner,
					PixelPosition i_currentPixel);
			iterator(const iterator& i_rIt) = default;

		public:
			void reset(PixelPosition i_centralPixel, int i_radius);
			void reset(
					int i_bufferWidth,
					int i_bufferHeight,
					PixelPosition i_centralPixel,
					int i_radius,
					int i_border = 0);
			PixelWindowSize getSize() const;
			const PixelPosition& operator*() const;
			iterator& operator++();
			bool hasEnded() const;
			bool operator!=(const iterator& i_rIt) const;

		private:
			PixelPosition m_minCorner;
			PixelPosition m_maxCorner;
			PixelPosition m_currentPixel;
		};


	public:
		PixelWindow();
		PixelWindow(
				int i_bufferWidth,
				int i_bufferHeight,
				PixelPosition i_centralPixel,
				int i_radius,
				int i_border = 0);

	public:
		void reset(
				int i_bufferWidth,
				int i_bufferHeight,
				PixelPosition i_centralPixel,
				int i_radius,
				int i_border = 0);

		PixelWindowSize getSize() const;

		iterator begin() const;
		iterator end() const;

	private:
		int m_width;
		int m_height;
		PixelPosition m_minCorner;
		PixelPosition m_maxCorner;
	};

	typedef PixelWindow PixelPatch;
	typedef PixelWindow::iterator PixWinIt;
	typedef PixelPatch::iterator PixPatchIt;

	/// @brief Template class for storing a 2D buffer of any vector type (any number of scalars per pixel)
	/// but seen as a 3D buffer of scalars
	///
	/// TODO : detailed desciption
	template <typename scalar = float>
	class DeepImage
	{

	public:
		class iterator
		{
		public:
			iterator();
			iterator(scalar* i_pPixelDataPtr, int i_nbOfScalarsInPixelData);
			iterator(const iterator&) = default;
			scalar* operator*() const;
			iterator& operator++();
			scalar& operator[](int i_dimensionIndex) const;
			bool operator!=(const iterator& i_rIt) const;

		private:
			scalar* m_pCurrentPixelDataPtr;
			int m_nbOfScalarsInPixelData;
		};

		class const_iterator
		{
		public:
			const_iterator();
			const_iterator(const scalar* i_pPixelDataPtr, int i_nbOfScalarsInPixelData);
			const_iterator(const const_iterator&) = default;
			const scalar* operator*() const;
			const_iterator& operator++();
			const scalar& operator[](int i_dimensionIndex) const;
			bool operator!=(const const_iterator& i_rIt) const;

		private:
			const scalar* m_pCurrentPixelDataPtr;
			int m_nbOfScalarsInPixelData;
		};

	public:
		DeepImage(); ///< Default constructor
		DeepImage(int i_width, int i_height, int i_depth); ///< Constructor with the size of the 3D buffer
		DeepImage(const DeepImage<scalar>&) = default; ///< Copy constructor
		DeepImage(DeepImage<scalar>&& i_tImage); ///< Move constructor
		DeepImage& operator=(const DeepImage<scalar>&) = default; ///< Assignment operator
		DeepImage& operator=(DeepImage<scalar>&& i_tImage); ///< Move assignment operator
		~DeepImage() = default; ///< Destructor

	public:

		/// @brief Resizes the buffer
		void resize(int i_width, int i_height, int i_depth);

		/// @brief Copies data from a pointer to the buffer.
		void copyDataFrom(const scalar* i_pData);

		/// @brief Copies data from the buffer to a pointer.
		void copyDataTo(scalar* i_pData) const;

		int getWidth() const; ///< Returns the width of the 3D buffer
		int getHeight() const; ///< Returns the height of the 3D buffer
		int getDepth() const; ///< Returns the depth of the 3D buffer
		int getSize() const; ///< Returns the number of scalars stored in the buffer
		scalar* getDataPtr();
		const scalar* getDataPtr() const;

		/// @brief Returns the 1D storage index in the buffer from 3 coordinates
		int glueIndices(int i_line, int i_column, int i_dimensionIndex) const;

		/// @brief Returns the 1D storage index in the buffer from 3 coordinates
		static int glueIndices(
				int i_width, int i_height, int i_depth,
				int i_line, int i_column, int i_dimensionIndex);

		/// @brief Returns the 3 coordinates from the 1D storage index in the buffer
		void splitIndex(
				int& o_rLine, int& o_rColumn, int& o_rDimensionIndex,
				int i_buffer1DIndex) const;

		/// @brief Returns the 3 coordinates from the 1D storage index in the buffer
		static void splitIndex(
				int& o_rLine, int& o_rColumn, int& o_rDimensionIndex,
				int i_buffer1DIndex,
				int i_width, int i_height, int i_depth);

		const scalar& get(int i_line, int i_column, int i_dimensionIndex) const;

		scalar& get(int i_line, int i_column, int i_dimensionIndex);

		const scalar& get(PixelPosition i_pixel, int i_dimensionIndex) const;

		scalar& get(PixelPosition i_pixel, int i_dimensionIndex);

		const scalar& get(int i_buffer1DIndex) const;

		scalar& get(int i_buffer1DIndex);

		const scalar getValue(PixelPosition i_pixel, int i_dimensionIndex) const; ///< returns a copy of the value; useful for scalar=bool

		scalar getValue(PixelPosition i_pixel, int i_dimensionIndex); ///< returns a copy of the value; useful for scalar=bool

		void set(int i_line, int i_column, int i_dimensionIndex, scalar i_value);

		void set(PixelPosition i_pixel, int i_dimensionIndex, scalar i_value);

		void set(int i_buffer1DIndex, scalar i_value);

		void set(int i_line, int i_column, const scalar* i_pVectorValue);

		void set(PixelPosition i_pixel, const scalar* i_pVectorValue);

		void isotropicalScale(scalar i_scaleFactor);

		void anisotropicalScale(const scalar* i_scaleFactors);

		void fill(scalar f);

		bool isEmpty() const;

		void clearAndFreeMemory();

		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;

		DeepImage<scalar>& operator+=(const DeepImage& i_rImage);
		DeepImage<scalar>& operator-=(const DeepImage& i_rImage);

	private:
		int m_width;
		int m_height;
		int m_depth;
		int m_widthTimesDepth; // equals to m_width times m_depth
		std::vector<scalar> m_data;

	public:

	};

	typedef DeepImage<float> Deepimf;
	typedef Deepimf::iterator ImfIt;
	typedef Deepimf::const_iterator ImfConstIt;

	template <typename scalar = float>
	class ImageWindow
	{
	public:
		class iterator
		{
		public:
			iterator();
			iterator(
					DeepImage< scalar >& i_rImage,
					PixelPosition i_centralPixel,
					int i_radius,
					int i_border = 0);
			iterator(
					int i_width,
					int i_height,
					int i_depth,
					PixelPosition i_minCorner,
					PixelPosition i_maxCorner,
					PixelPosition i_currentPixel,
					scalar* i_pCurrentDataPointer);
			iterator(const iterator& i_rIt) = default;

		public:
			void reset(
					DeepImage< scalar >& i_rImage,
					PixelPosition i_centralPixel,
					int i_radius,
					int i_border = 0);
			ImageWindowSize getSize() const;
			scalar* operator*() const;
			iterator& operator++();
			bool hasEnded() const;
			scalar& operator[](int i_dimensionIndex) const;
			bool operator!=(const iterator& i_rIt) const;

		private:
			int m_width;
			int m_height;
			int m_depth;
			PixelPosition m_minCorner;
			PixelPosition m_maxCorner;
			PixelPosition m_currentPixel;
			scalar* m_pCurrentDataPointer;
		};


	public:
		ImageWindow();
		ImageWindow(
				DeepImage< scalar >& i_rImage,
				PixelPosition i_centralPixel,
				int i_radius,
				int i_border = 0);

	public:
		void reset(
				DeepImage< scalar >& i_rImage,
				PixelPosition i_centralPixel,
				int i_radius,
				int i_border = 0);

		ImageWindowSize getSize() const;

		iterator begin() const;
		iterator end() const;

	private:
		int m_width;
		int m_height;
		int m_depth;
		PixelPosition m_minCorner;
		PixelPosition m_maxCorner;
		scalar* m_pMinCornerDataPointer;
	};

	typedef ImageWindow<float> Win;
	typedef Win Patch;

	typedef Win::iterator WinIt;
	typedef WinIt PatchIt;


	template <typename scalar = float>
	class ConstImageWindow
	{

	public:
		class iterator
		{
		public:
			iterator();
			iterator(
					const DeepImage< scalar >& i_rImage,
					PixelPosition i_centralPixel,
					int i_radius,
					int i_border = 0);
			iterator(
					int i_width,
					int i_height,
					int i_depth,
					PixelPosition i_minCorner,
					PixelPosition i_maxCorner,
					PixelPosition i_currentPixel,
					const scalar* i_pCurrentDataPointer);
			iterator(const iterator& i_rIt) = default;

		public:
			void reset(
					const DeepImage< scalar >& i_rImage,
					PixelPosition i_centralPixel,
					int i_radius,
					int i_border = 0);

			ImageWindowSize getSize() const;

			const scalar* operator*() const;

			iterator& operator++();

			bool hasEnded() const;

			const scalar& operator[](int i_dimensionIndex) const;

			bool operator!=(const iterator& i_rIt) const;

		private:
			int m_width;
			int m_height;
			int m_depth;
			PixelPosition m_minCorner;
			PixelPosition m_maxCorner;
			PixelPosition m_currentPixel;
			const scalar* m_pCurrentDataPointer;

		};



	public:
		ConstImageWindow();
		ConstImageWindow(
				const DeepImage< scalar >& i_rImage,
				PixelPosition i_centralPixel,
				int i_radius,
				int i_border = 0);

	public:
		void reset(
				const DeepImage< scalar >& i_rImage,
				PixelPosition i_centralPixel,
				int i_radius,
				int i_border = 0);

		ImageWindowSize getSize() const;

		iterator begin() const;
		iterator end() const;

	private:
		int m_width;
		int m_height;
		int m_depth;
		PixelPosition m_minCorner;
		PixelPosition m_maxCorner;
		const scalar* m_pMinCornerDataPointer;
	};

	typedef ConstImageWindow<float> ConstWin;
	typedef ConstWin ConstPatch;

	typedef ConstWin::iterator ConstWinIt;
	typedef ConstWinIt ConstPatchIt;

} // namespace bcd

#include "DeepImage.hpp"

#endif // DEEP_IMAGE_H
