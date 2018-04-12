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

#include "lpyramid.h"

#include <algorithm>
#include <cassert>
#include <ciso646>
#include <cstddef>


namespace pdiff
{
    LPyramid::LPyramid(const std::vector<float> &image,
                       const unsigned int width, const unsigned int height)
        : width_(width), height_(height)
    {
        // Make the Laplacian pyramid by successively
        // copying the earlier levels and blurring them
        for (auto i = 0u; i < MAX_PYR_LEVELS; i++)
        {
            if (i == 0 or width * height <= 1)
            {
                levels_[i] = image;
            }
            else
            {
                levels_[i].resize(width_ * height_);
                convolve(levels_[i], levels_[i - 1]);
            }
        }
    }

    // Convolves image b with the filter kernel and stores it in a.
    void LPyramid::convolve(std::vector<float> &a,
                            const std::vector<float> &b) const
    {
        assert(a.size() > 1);
        assert(b.size() > 1);

        #pragma omp parallel for shared(a, b)
        for (auto y = 0; y < static_cast<ptrdiff_t>(height_); y++)
        {
            for (auto x = 0u; x < width_; x++)
            {
                const auto index = y * width_ + x;
                auto result = 0.0f;
                for (auto i = -2; i <= 2; i++)
                {
                    for (auto j = -2; j <= 2; j++)
                    {
                        int nx = x + i;
                        int ny = y + j;
                        nx = std::max(nx, -nx);
                        ny = std::max(ny, -ny);
                        if (nx >= static_cast<long>(width_))
                        {
                            nx = 2 * width_ - nx - 1;
                        }
                        if (ny >= static_cast<long>(height_))
                        {
                            ny = 2 * height_ - ny - 1;
                        }

                        const float kernel[] = {0.05f, 0.25f, 0.4f, 0.25f, 0.05f};

                        result +=
                            kernel[i + 2] * kernel[j + 2] * b[ny * width_ + nx];
                    }
                }
                a[index] = result;
            }
        }
    }

    float LPyramid::get_value(const unsigned int x, const unsigned int y,
                              const unsigned int level) const
    {
        const auto index = x + y * width_;
        assert(level < MAX_PYR_LEVELS);
        return levels_[level][index];
    }
}
