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

#ifndef PERCEPTUALDIFF_COMPARE_ARGS_H
#define PERCEPTUALDIFF_COMPARE_ARGS_H

#include "exceptions.h"
#include "metric.h"

#include <memory>
#include <stdexcept>
#include <string>


namespace pdiff
{
    // Arguments to pass into the comparison function.
    class CompareArgs
    {
    public:

        CompareArgs(int argc, char **argv);

        void print_args() const;

        std::shared_ptr<RGBAImage> image_a_;
        std::shared_ptr<RGBAImage> image_b_;
        std::shared_ptr<RGBAImage> image_difference_;
        bool verbose_;

        // Print a sum of the luminance and color differences of each pixel.
        bool sum_errors_;

        // How much to down sample image before comparing, in powers of 2.
        unsigned int down_sample_;

        PerceptualDiffParameters parameters_;

    private:

        void parse_args(int argc, char **argv);
    };


    class ParseException : public virtual PerceptualDiffException
    {
    public:

        explicit ParseException(const std::string &message)
            : std::invalid_argument(message),
              PerceptualDiffException(message)
        {
        }
    };
}

#endif
