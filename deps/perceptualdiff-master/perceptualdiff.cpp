/*
PerceptualDiff - a program that compares two images using a perceptual metric
based on the paper :
A perceptual metric for production testing. Journal of graphics tools,
9(4):33-40, 2004, Hector Yee
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
#include "lpyramid.h"
#include "metric.h"
#include "rgba_image.h"

#include <cstdlib>
#include <ciso646>
#include <iostream>
#include <string>


int main(const int argc, char **const argv)
{
    try
    {
        const pdiff::CompareArgs args(argc, argv);

        if (args.verbose_)
        {
            args.print_args();
        }

        std::string reason;
        float error_sum = 0;
        const auto passed = pdiff::yee_compare(
            *args.image_a_,
            *args.image_b_,
            args.parameters_,
            nullptr,
            &error_sum,
            &reason,
            args.image_difference_.get(),
            args.verbose_ ? &std::cout : nullptr);

        if (passed)
        {
            if (args.verbose_)
            {
                std::cout << "PASS: " + reason;
            }
        }
        else
        {
                std::cout << "FAIL: " + reason;
        }

        if (args.sum_errors_)
        {
            const auto normalized =
                error_sum /
                (args.image_a_->get_width() *
                 args.image_a_->get_height() * 255.);

            std::cout << error_sum << " error sum\n";
            std::cout << normalized << " normalzied error sum\n";
        }

        if (args.image_difference_.get())
        {
            args.image_difference_->write_to_file(args.image_difference_->get_name());

            std::cerr << "Wrote difference image to "
                      << args.image_difference_->get_name()
                      << "\n";
        }

        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    catch (const pdiff::ParseException &exception)
    {
        std::cerr << exception.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (const pdiff::RGBImageException &exception)
    {
        std::cerr << exception.what() << "\n";
        return EXIT_FAILURE;
    }
}
