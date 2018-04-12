/*
RGBAImage.cpp
Copyright (C) 2006-2011 Yangli Hector Yee
Copyright (C) 2011-2016 Steven Myint, Jeff Terrace

(This entire file was rewritten by Jim Tilander)

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

#include "rgba_image.h"

#include <FreeImage.h>

#include <cassert>
#include <ciso646>
#include <cstring>
#include <string>


namespace pdiff
{
    struct FreeImageDeleter
    {
        inline void operator()(FIBITMAP *image)
        {
            if (image)
            {
                FreeImage_Unload(image);
            }
        }
    };


    static std::shared_ptr<FIBITMAP> to_free_image(const RGBAImage &image)
    {
        const auto *data = image.get_data();

        std::shared_ptr<FIBITMAP> bitmap(
            FreeImage_Allocate(image.get_width(), image.get_height(), 32,
                               0x000000ff, 0x0000ff00, 0x00ff0000),
            FreeImageDeleter());
        assert(bitmap.get());

        for (auto y = 0u; y < image.get_height();
             y++, data += image.get_width())
        {
            auto scanline = reinterpret_cast<unsigned int *>(
                FreeImage_GetScanLine(bitmap.get(), image.get_height() - y - 1));
            memcpy(scanline, data, sizeof(data[0]) * image.get_width());
        }

        return bitmap;
    }


    static std::shared_ptr<RGBAImage> to_rgba_image(FIBITMAP *image,
                                                    const std::string &filename="")
    {
        const auto w = FreeImage_GetWidth(image);
        const auto h = FreeImage_GetHeight(image);

        auto result = std::make_shared<RGBAImage>(w, h, filename);
        // Copy the image over to our internal format, FreeImage has the scanlines
        // bottom to top though.
        auto dest = result->get_data();
        for (unsigned int y = 0; y < h; y++, dest += w)
        {
            const auto scanline = reinterpret_cast<const unsigned int *>(
                FreeImage_GetScanLine(image, h - y - 1));
            memcpy(dest, scanline, sizeof(dest[0]) * w);
        }

        return result;

    }

    std::shared_ptr<RGBAImage> RGBAImage::down_sample(unsigned int w,
                                                      unsigned int h) const
    {
        if (w == 0)
        {
            w = width_ / 2;
        }

        if (h == 0)
        {
            h = height_ / 2;
        }

        if (width_ <= 1 or height_ <= 1)
        {
            return nullptr;
        }
        if (width_ == w and height_ == h)
        {
            return nullptr;
        }
        assert(w <= width_);
        assert(h <= height_);

        auto bitmap = to_free_image(*this);
        std::unique_ptr<FIBITMAP, FreeImageDeleter> converted(
            FreeImage_Rescale(bitmap.get(), w, h, FILTER_BICUBIC));

        auto img = to_rgba_image(converted.get(), name_);

        return img;
    }

    void RGBAImage::write_to_file(const std::string &filename) const
    {
        const auto file_type = FreeImage_GetFIFFromFilename(filename.c_str());
        if (FIF_UNKNOWN == file_type)
        {
            throw RGBImageException("Can't save to unknown filetype '" +
                                    filename + "'");
        }

        auto bitmap = to_free_image(*this);

        const bool result =
            !!FreeImage_Save(file_type, bitmap.get(), filename.c_str());
        if (not result)
        {
            throw RGBImageException("Failed to save to '" + filename + "'");
        }
    }

    std::shared_ptr<RGBAImage> read_from_file(const std::string &filename)
    {
        const auto file_type = FreeImage_GetFileType(filename.c_str());
        if (FIF_UNKNOWN == file_type)
        {
            throw RGBImageException("Unknown filetype '" + filename + "'");
        }

        FIBITMAP *free_image = nullptr;
        if (auto temporary = FreeImage_Load(file_type, filename.c_str(), 0))
        {
            free_image = FreeImage_ConvertTo32Bits(temporary);
            FreeImage_Unload(temporary);
        }
        if (not free_image)
        {
            throw RGBImageException("Failed to load the image " + filename);
        }

        auto result = to_rgba_image(free_image);

        FreeImage_Unload(free_image);

        return result;
    }
}
