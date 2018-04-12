/*
Compare Args
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

#include "compare_args.h"

#include "rgba_image.h"

#include <cassert>
#include <ciso646>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sstream>


namespace pdiff
{
    static const auto VERSION = "2.1";


    static const auto USAGE =
"Usage: perceptualdiff image1 image2\n"
"\n"
"Compares image1 and image2 using a perceptually based image metric.\n"
"\n"
"Options:\n"
"  --verbose         Turn on verbose mode\n"
"  --fov deg         Field of view in degrees [0.1, 89.9] (default: 45.0)\n"
"  --threshold p     Number of pixels p below which differences are ignored\n"
"  --gamma g         Value to convert rgb into linear space (default: 2.2)\n"
"  --luminance l     White luminance (default: 100.0 cdm^-2)\n"
"  --luminance-only  Only consider luminance; ignore chroma (color) in the\n"
"                    comparison\n"
"  --color-factor    How much of color to use [0.0, 1.0] (default: 1.0)\n"
"  --down-sample     How many powers of two to down sample the image\n"
"                    (default: 0)\n"
"  --scale           Scale images to match each other's dimensions\n"
"  --sum-errors      Print a sum of the luminance and color differences\n"
"  --output o        Write difference to the file o\n"
"  --version         Print version\n"
"\n";


    static bool option_matches(const char *arg, const std::string &option_name)
    {
        const auto string_arg = std::string(arg);

        return (string_arg == "--" + option_name) or
               (string_arg == "-" + option_name);
    }


    CompareArgs::CompareArgs(int argc, char **argv)
        : verbose_(false),
          sum_errors_(false),
          down_sample_(0)
    {
        parse_args(argc, argv);
    }


    static void print_help()
    {
        std::cout << USAGE;
        std::cout << "\n"
           << "OpenMP status: ";
#ifdef _OPENMP
        std::cout << "enabled\n";
#else
        std::cout << "disabled\n";
#endif
    }

    void CompareArgs::parse_args(const int argc, char **argv)
    {
        if (argc <= 1)
        {
            print_help();
            exit(EXIT_FAILURE);
        }

        auto image_count = 0u;
        const char *output_file_name = nullptr;
        auto scale = false;
        for (auto i = 1; i < argc; i++)
        {
            try
            {
                if (option_matches(argv[i], "help") or
                    option_matches(argv[i], "h"))
                {
                    print_help();
                    exit(EXIT_SUCCESS);
                }
                else if (option_matches(argv[i], "fov"))
                {
                    if (++i < argc)
                    {
                        parameters_.field_of_view = std::stof(argv[i]);
                    }
                }
                else if (option_matches(argv[i], "verbose"))
                {
                    verbose_ = true;
                }
                else if (option_matches(argv[i], "threshold"))
                {
                    if (++i < argc)
                    {
                        auto temporary = std::stoi(argv[i]);
                        if (temporary < 0)
                        {
                            throw PerceptualDiffException(
                                "-threshold must be positive");
                        }
                        parameters_.threshold_pixels =
                            static_cast<unsigned int>(temporary);
                    }
                }
                else if (option_matches(argv[i], "gamma"))
                {
                    if (++i < argc)
                    {
                        parameters_.gamma = std::stof(argv[i]);
                    }
                }
                else if (option_matches(argv[i], "luminance"))
                {
                    if (++i < argc)
                    {
                        parameters_.luminance = std::stof(argv[i]);
                    }
                }
                else if (option_matches(argv[i], "luminance-only") or
                         option_matches(argv[i], "luminanceonly"))
                {
                    parameters_.luminance_only = true;
                }
                else if (option_matches(argv[i], "sum-errors"))
                {
                    sum_errors_ = true;
                }
                else if (option_matches(argv[i], "color-factor") or
                         option_matches(argv[i], "colorfactor"))
                {
                    if (++i < argc)
                    {
                        parameters_.color_factor = std::stof(argv[i]);
                    }
                }
                else if (option_matches(argv[i], "down-sample") or
                         option_matches(argv[i], "downsample"))
                {
                    if (++i < argc)
                    {
                        auto temporary = std::stoi(argv[i]);
                        if (temporary < 0)
                        {
                            throw PerceptualDiffException(
                                "--downsample must be positive");
                        }
                        down_sample_ = static_cast<unsigned int>(temporary);
                        assert(down_sample_ <= INT_MAX);
                    }
                }
                else if (option_matches(argv[i], "scale"))
                {
                    scale = true;
                }
                else if (option_matches(argv[i], "output"))
                {
                    if (++i < argc)
                    {
                        output_file_name = argv[i];
                    }
                }
                else if (option_matches(argv[i], "version"))
                {
                    std::cout << "perceptualdiff " << VERSION << "\n";
                    exit(EXIT_SUCCESS);
                }
                else if (image_count < 2)
                {
                    auto image = read_from_file(argv[i]);

                    ++image_count;
                    if (image_count == 1)
                    {
                        image_a_ = image;
                    }
                    else
                    {
                        image_b_ = image;
                    }
                }
                else
                {
                    std::cerr << "Warning: option/file \"" << argv[i]
                              << "\" ignored\n";
                }
            }
            catch (const PerceptualDiffException &exception)
            {
                std::string reason = "";
                if (not std::string(exception.what()).empty())
                {
                    reason = std::string("; ") + exception.what();
                }
                throw ParseException(
                    "Invalid argument (" + std::string(argv[i]) + ") for " +
                    argv[i - 1] + reason);
            }
            catch (const std::invalid_argument &exception)
            {
                throw ParseException(
                    "Invalid argument (" + std::string(argv[i]) + ") for " +
                    argv[i - 1]);
            }
        }

        if (not image_a_ or not image_b_)
        {
            std::cerr << "Not enough image files specified\n";
            exit(EXIT_FAILURE);
        }

        for (auto i = 0u; i < down_sample_; i++)
        {
            const auto tmp_a = image_a_->down_sample();
            const auto tmp_b = image_b_->down_sample();

            if (tmp_a and tmp_b)
            {
                image_a_ = tmp_a;
                image_b_ = tmp_b;
            }
            else
            {
                break;
            }

            if (verbose_)
            {
                std::cout << "Downsampling by " << (1 << (i + 1)) << "\n";
            }
        }

        if (scale and
            (image_a_->get_width() != image_b_->get_width() or
             image_a_->get_height() != image_b_->get_height()))
        {
            auto min_width = image_a_->get_width();
            if (image_b_->get_width() < min_width)
            {
                min_width = image_b_->get_width();
            }

            auto min_height = image_a_->get_height();
            if (image_b_->get_height() < min_height)
            {
                min_height = image_b_->get_height();
            }

            if (verbose_)
            {
                std::cout << "Scaling to " << min_width << " x " << min_height
                          << "\n";
            }
            auto tmp = image_a_->down_sample(min_width, min_height);
            if (tmp)
            {
                image_a_ = tmp;
            }
            tmp = image_b_->down_sample(min_width, min_height);
            if (tmp)
            {
                image_b_ = tmp;
            }
        }
        if (output_file_name)
        {
            image_difference_ = std::make_shared<RGBAImage>(image_a_->get_width(),
                                                            image_a_->get_height(),
                                                            output_file_name);
        }
    }

    void CompareArgs::print_args() const
    {
        std::cout
            << "Field of view is " << parameters_.field_of_view << " degrees\n"
            << "Threshold pixels is " << parameters_.threshold_pixels
            << " pixels\n"
            << "The gamma is " << parameters_.gamma << "\n"
            << "The display's luminance is " << parameters_.luminance
            << " candela per meter squared\n";
    }
}
