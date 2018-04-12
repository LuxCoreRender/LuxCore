/*
Laplacian Pyramid
Copyright (C) 2006-2011 Yangli Hector Yee
Copyright (C) 2011-2016 Steven Myint, Jeff Terrace

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef PERCEPTUALDIFF_LPYRAMID_H
#define PERCEPTUALDIFF_LPYRAMID_H

#include <vector>


namespace pdiff
{
#if _MSC_VER >= 1900
    constexpr auto MAX_PYR_LEVELS = 8u;
#else
#define MAX_PYR_LEVELS 8u
#endif

    class LPyramid
    {
    public:

        LPyramid(const std::vector<float> &image,
                 unsigned int width,
                 unsigned int height);

        float get_value(unsigned int x, unsigned int y, unsigned int level) const;

    private:

        void convolve(std::vector<float> &a, const std::vector<float> &b) const;

        // Successively blurred versions of the original image.
        std::vector<float> levels_[MAX_PYR_LEVELS];

        unsigned int width_;
        unsigned int height_;
    };
}

#endif
