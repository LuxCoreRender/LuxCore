/*
Metric
Copyright (C) 2006-2011 Yangli Hector Yee
Copyright (C) 2011-2016 Steven Myint, Jeff Terrace

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "metric.h"

#include "lpyramid.h"
#include "rgba_image.h"

#include <ciso646>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>
#include <algorithm>


namespace pdiff
{
#if _MSC_VER <= 1800
    static const auto pi = 3.14159265f;


    static float to_radians(const float degrees)  // LCOV_EXCL_LINE
    {
      return degrees * pi / 180.f;  // LCOV_EXCL_LINE
    }


    static float to_degrees(const float radians)  // LCOV_EXCL_LINE
    {
      return radians * 180.f / pi;  // LCOV_EXCL_LINE
    }
#else
    constexpr auto pi = 3.14159265f;


    constexpr float to_radians(const float degrees)  // LCOV_EXCL_LINE
    {
        return degrees * pi / 180.f;  // LCOV_EXCL_LINE
    }


    constexpr float to_degrees(const float radians)  // LCOV_EXCL_LINE
    {
        return radians * 180.f / pi;  // LCOV_EXCL_LINE
    }
#endif

    // Given the adaptation luminance, this function returns the
    // threshold of visibility in cd per m^2.
    //
    // TVI means Threshold vs Intensity function.
    // This version comes from Ward Larson Siggraph 1997.
    //
    // Returns the threshold luminance given the adaptation luminance.
    // Units are candelas per meter squared.
    static float tvi(const float adaptation_luminance)
    {
        const auto log_a = log10f(adaptation_luminance);

        float r;
        if (log_a < -3.94f)
        {
            r = -2.86f;
        }
        else if (log_a < -1.44f)
        {
            r = powf(0.405f * log_a + 1.6f, 2.18f) - 2.86f;
        }
        else if (log_a < -0.0184f)
        {
            r = log_a - 0.395f;
        }
        else if (log_a < 1.9f)
        {
            r = powf(0.249f * log_a + 0.65f, 2.7f) - 0.72f;
        }
        else
        {
            r = log_a - 1.255f;
        }

        return powf(10.0f, r);
    }


    // computes the contrast sensitivity function (Barten SPIE 1989)
    // given the cycles per degree (cpd) and luminance (lum)
    static float csf(const float cpd, const float lum)
    {
        const auto a = 440.f * powf((1.f + 0.7f / lum), -0.2f);
        const auto b = 0.3f * powf((1.0f + 100.0f / lum), 0.15f);

        return a * cpd * expf(-b * cpd) * sqrtf(1.0f + 0.06f * expf(b * cpd));
    }


    /*
    * Visual Masking Function
    * from Daly 1993
    */
    static float mask(const float contrast)
    {
        const auto a = powf(392.498f * contrast, 0.7f);
        const auto b = powf(0.0153f * a, 4.f);
        return powf(1.0f + b, 0.25f);
    }


    // convert Adobe RGB (1998) with reference white D65 to XYZ
    static void adobe_rgb_to_xyz(const float r, const float g, const float b,
                                 float &x, float &y, float &z)
    {
        // matrix is from http://www.brucelindbloom.com/
        x = r * 0.576700f  + g * 0.185556f  + b * 0.188212f;
        y = r * 0.297361f  + g * 0.627355f  + b * 0.0752847f;
        z = r * 0.0270328f + g * 0.0706879f + b * 0.991248f;
    }


    struct White
    {
        White()
        {
            adobe_rgb_to_xyz(1.f, 1.f, 1.f, x, y, z);
        }

        float x;
        float y;
        float z;
    };


    static const White global_white;


    static void xyz_to_lab(const float x, const float y, const float z,
                           float &l, float &a, float &b)
    {
        const float epsilon = 216.0f / 24389.0f;
        const float kappa = 24389.0f / 27.0f;
        const float r[] = {
            x / global_white.x,
            y / global_white.y,
            z / global_white.z
        };
        float f[3];
        for (auto i = 0u; i < 3; i++)
        {
            if (r[i] > epsilon)
            {
                f[i] = powf(r[i], 1.0f / 3.0f);
            }
            else
            {
                f[i] = (kappa * r[i] + 16.0f) / 116.0f;
            }
        }
        l = 116.0f * f[1] - 16.0f;
        a = 500.0f * (f[0] - f[1]);
        b = 200.0f * (f[1] - f[2]);
    }


    static unsigned int adaptation(const float num_one_degree_pixels)
    {
        auto num_pixels = 1.f;
        auto adaptation_level = 0u;
        for (auto i = 0u; i < MAX_PYR_LEVELS; i++)
        {
            adaptation_level = i;
            if (num_pixels > num_one_degree_pixels)
            {
                break;
            }
            num_pixels *= 2;
        }
        return adaptation_level;  // LCOV_EXCL_LINE
    }


    PerceptualDiffParameters::PerceptualDiffParameters()
        : luminance_only(false),
          field_of_view(45.0f),
          gamma(2.2f),
          luminance(100.0f),
          threshold_pixels(100),
          color_factor(1.0f)
    {
    }


    bool yee_compare(const RGBAImage &image_a,
                     const RGBAImage &image_b,
                     const PerceptualDiffParameters &args,
                     size_t *const output_num_pixels_failed,
                     float *const output_error_sum,
                     std::string *const output_reason,
                     RGBAImage *const output_image_difference,
                     std::ostream *const output_verbose)
    {
        if ((image_a.get_width()  != image_b.get_width()) or
            (image_a.get_height() != image_b.get_height()))
        {
            if (output_reason)
            {
                *output_reason = "Image dimensions do not match\n";
            }
            return false;
        }

        const auto w = image_a.get_width();
        const auto h = image_a.get_height();
        const auto dim = w * h;

        auto identical = true;
        for (auto i = 0u; i < dim; i++)
        {
            if (image_a.get(i) != image_b.get(i))
            {
                identical = false;
                break;
            }
        }
        if (identical)
        {
            if (output_reason)
            {
                *output_reason = "Images are binary identical\n";
            }
            return true;
        }

        // Assuming colorspaces are in Adobe RGB (1998) convert to XYZ.
        std::vector<float> a_lum(dim);
        std::vector<float> b_lum(dim);

        std::vector<float> a_a(dim);
        std::vector<float> b_a(dim);
        std::vector<float> a_b(dim);
        std::vector<float> b_b(dim);

        if (output_verbose)
        {
            *output_verbose << "Converting RGB to XYZ\n";
        }

        const auto gamma = args.gamma;
        const auto luminance = args.luminance;

        #pragma omp parallel for shared(args, a_lum, b_lum, a_a, a_b, b_a, b_b)
        for (auto y = 0; y < static_cast<ptrdiff_t>(h); y++)
        {
            for (auto x = 0u; x < w; x++)
            {
                const auto i = x + y * w;

                // perceptualdiff used to use premultiplied alphas when loading
                // the image. This is no longer the case since the switch to
                // FreeImage. We need to do the multiplication here now. As was
                // the case with premultiplied alphas, differences in alphas
                // won't be detected where the color is black.

                const auto a_alpha = image_a.get_alpha(i) / 255.f;

                const auto a_color_r = powf(
                    image_a.get_red(i) / 255.f * a_alpha,
                    gamma);
                const auto a_color_g = powf(
                    image_a.get_green(i) / 255.f * a_alpha,
                    gamma);
                const auto a_color_b = powf(
                    image_a.get_blue(i) / 255.f * a_alpha,
                    gamma);

                float a_x;
                float a_y;
                float a_z;
                adobe_rgb_to_xyz(a_color_r, a_color_g, a_color_b,
                                 a_x, a_y, a_z);
                float l;
                xyz_to_lab(a_x, a_y, a_z, l, a_a[i], a_b[i]);

                const auto b_alpha = image_b.get_alpha(i) / 255.f;

                const auto b_color_r = powf(
                    image_b.get_red(i) / 255.f * b_alpha,
                    gamma);
                const auto b_color_g = powf(
                    image_b.get_green(i) / 255.f * b_alpha,
                    gamma);
                const auto b_color_b = powf(
                    image_b.get_blue(i) / 255.f * b_alpha,
                    gamma);

                float b_x;
                float b_y;
                float b_z;
                adobe_rgb_to_xyz(b_color_r, b_color_g, b_color_b,
                                 b_x, b_y, b_z);
                xyz_to_lab(b_x, b_y, b_z, l, b_a[i], b_b[i]);

                a_lum[i] = a_y * luminance;
                b_lum[i] = b_y * luminance;
            }
        }

        if (output_verbose)
        {
            *output_verbose << "Constructing Laplacian Pyramids\n";
        }

        const LPyramid la(a_lum, w, h);
        const LPyramid lb(b_lum, w, h);

        const auto num_one_degree_pixels =
            to_degrees(2 *
                       std::tan(args.field_of_view * to_radians(.5f)));
        const auto pixels_per_degree = w / num_one_degree_pixels;

        if (output_verbose)
        {
            *output_verbose << "Performing test\n";
        }

        const auto adaptation_level = adaptation(num_one_degree_pixels);

        float cpd[MAX_PYR_LEVELS];
        cpd[0] = 0.5f * pixels_per_degree;
        for (auto i = 1u; i < MAX_PYR_LEVELS; i++)
        {
            cpd[i] = 0.5f * cpd[i - 1];
        }
        const auto csf_max = csf(3.248f, 100.0f);

        static_assert(MAX_PYR_LEVELS > 2,
                      "MAX_PYR_LEVELS must be greater than 2");

        float f_freq[MAX_PYR_LEVELS - 2];
        for (auto i = 0u; i < MAX_PYR_LEVELS - 2; i++)
        {
            f_freq[i] = csf_max / csf(cpd[i], 100.0f);
        }

        auto pixels_failed = 0u;
        auto error_sum = 0.;

        #pragma omp parallel for reduction(+ : pixels_failed, error_sum) \
            shared(args, a_a, a_b, b_a, b_b, cpd, f_freq)
        for (auto y = 0; y < static_cast<ptrdiff_t>(h); y++)
        {
            for (auto x = 0u; x < w; x++)
            {
                const auto index = y * w + x;

                const auto adapt = std::max(
                    (la.get_value(x, y, adaptation_level) +
                     lb.get_value(x, y, adaptation_level)) * 0.5f,
                    1e-5f);

                auto sum_contrast = 0.f;
                auto factor = 0.f;

                for (auto i = 0u; i < MAX_PYR_LEVELS - 2; i++)
                {
                    const auto n1 =
                        std::abs(la.get_value(x, y, i) -
                                 la.get_value(x, y, i + 1));

                    const auto n2 =
                        std::abs(lb.get_value(x, y, i) -
                                 lb.get_value(x, y, i + 1));

                    const auto numerator = std::max(n1, n2);
                    const auto d1 = std::abs(la.get_value(x, y, i + 2));
                    const auto d2 = std::abs(lb.get_value(x, y, i + 2));
                    const auto denominator = std::max(std::max(d1, d2), 1e-5f);
                    const auto contrast = numerator / denominator;
                    const auto f_mask = mask(contrast * csf(cpd[i], adapt));
                    factor += contrast * f_freq[i] * f_mask;
                    sum_contrast += contrast;
                }
                sum_contrast = std::max(sum_contrast, 1e-5f);
                factor /= sum_contrast;
                factor = std::min(std::max(factor, 1.f), 10.f);
                const auto delta =
                    std::abs(la.get_value(x, y, 0) - lb.get_value(x, y, 0));
                error_sum += delta;
                auto pass = true;

                // Pure luminance test.
                if (delta > factor * tvi(adapt))
                {
                    pass = false;
                }

                if (not args.luminance_only)
                {
                    // CIE delta E test with modifications.
                    auto color_scale = args.color_factor;

                    // Ramp down the color test in scotopic regions.
                    if (adapt < 10.0f)
                    {
                        // Don't do color test at all.
                        color_scale = 0.0;
                    }

                    const auto da = a_a[index] - b_a[index];
                    const auto db = a_b[index] - b_b[index];
                    const auto delta_e = (da * da + db * db) * color_scale;
                    error_sum += delta_e;
                    if (delta_e > factor)
                    {
                        pass = false;
                    }
                }

                if (pass)
                {
                    if (output_image_difference)
                    {
                        output_image_difference->set(0, 0, 0, 255, index);
                    }
                }
                else
                {
                    pixels_failed++;
                    if (output_image_difference)
                    {
                        output_image_difference->set(255, 0, 0, 255, index);
                    }
                }
            }
        }

        const auto different =
            std::to_string(pixels_failed) + " pixels are different\n";

        const auto passed = pixels_failed < args.threshold_pixels;

        if (output_reason)
        {
            if (passed)
            {
                *output_reason =
                    "Images are perceptually indistinguishable\n" + different;
            }
            else
            {
                *output_reason = "Images are visibly different\n" + different;
            }
        }

        if (output_num_pixels_failed)
        {
            *output_num_pixels_failed = pixels_failed;
        }

        if (output_error_sum)
        {
            *output_error_sum = error_sum;
        }

        return passed;
    }
}
