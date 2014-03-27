/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include <sstream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebuf.h>

#include "slg/sdl/sdl.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Texture
//------------------------------------------------------------------------------

// The generic implementation
UV Texture::GetDuv(const HitPoint &hitPoint,
        const Vector &dpdu, const Vector &dpdv,
        const Normal &dndu, const Normal &dndv,
        const float sampleDistance) const {
    // Calculate bump map value at intersection point
    const float base = GetFloatValue(hitPoint);

    // Compute offset positions and evaluate displacement texture
    const Point origP = hitPoint.p;
    const Normal origShadeN = hitPoint.shadeN;
    const float origU = hitPoint.uv.u;

    UV duv;
    HitPoint hitPointTmp = hitPoint;

    // Shift hitPointTmp.du in the u direction and calculate value
    const float uu = sampleDistance / dpdu.Length();
    hitPointTmp.p += uu * dpdu;
    hitPointTmp.uv.u += uu;
    hitPointTmp.shadeN = Normalize(origShadeN + uu * dndu);
    duv.u = (GetFloatValue(hitPointTmp) - base) / uu;

    // Shift hitPointTmp.dv in the v direction and calculate value
    const float vv = sampleDistance / dpdv.Length();
    hitPointTmp.p = origP + vv * dpdv;
    hitPointTmp.uv.u = origU;
    hitPointTmp.uv.v += vv;
    hitPointTmp.shadeN = Normalize(origShadeN + vv * dndv);
    duv.v = (GetFloatValue(hitPointTmp) - base) / vv;

    return duv;
}

//------------------------------------------------------------------------------
// Texture utility functions
//------------------------------------------------------------------------------

// Perlin Noise Data
#define NOISE_PERM_SIZE 256
static const int NoisePerm[2 * NOISE_PERM_SIZE] = {
	151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96,
	53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
	// Rest of noise permutation table
	8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
	151, 160, 137, 91, 90, 15,
	131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

/* needed for voronoi */
#define HASHPNT(x,y,z) hashpntf+3*voronoi_hash[ (voronoi_hash[ (voronoi_hash[(z) & 255]+(y)) & 255]+(x)) & 255]

static float hashpntf[768] = {0.536902, 0.020915, 0.501445, 0.216316, 0.517036, 0.822466, 0.965315,
0.377313, 0.678764, 0.744545, 0.097731, 0.396357, 0.247202, 0.520897,
0.613396, 0.542124, 0.146813, 0.255489, 0.810868, 0.638641, 0.980742,
0.292316, 0.357948, 0.114382, 0.861377, 0.629634, 0.722530, 0.714103,
0.048549, 0.075668, 0.564920, 0.162026, 0.054466, 0.411738, 0.156897,
0.887657, 0.599368, 0.074249, 0.170277, 0.225799, 0.393154, 0.301348,
0.057434, 0.293849, 0.442745, 0.150002, 0.398732, 0.184582, 0.915200,
0.630984, 0.974040, 0.117228, 0.795520, 0.763238, 0.158982, 0.616211,
0.250825, 0.906539, 0.316874, 0.676205, 0.234720, 0.667673, 0.792225,
0.273671, 0.119363, 0.199131, 0.856716, 0.828554, 0.900718, 0.705960,
0.635923, 0.989433, 0.027261, 0.283507, 0.113426, 0.388115, 0.900176,
0.637741, 0.438802, 0.715490, 0.043692, 0.202640, 0.378325, 0.450325,
0.471832, 0.147803, 0.906899, 0.524178, 0.784981, 0.051483, 0.893369,
0.596895, 0.275635, 0.391483, 0.844673, 0.103061, 0.257322, 0.708390,
0.504091, 0.199517, 0.660339, 0.376071, 0.038880, 0.531293, 0.216116,
0.138672, 0.907737, 0.807994, 0.659582, 0.915264, 0.449075, 0.627128,
0.480173, 0.380942, 0.018843, 0.211808, 0.569701, 0.082294, 0.689488, 
0.573060, 0.593859, 0.216080, 0.373159, 0.108117, 0.595539, 0.021768, 
0.380297, 0.948125, 0.377833, 0.319699, 0.315249, 0.972805, 0.792270, 
0.445396, 0.845323, 0.372186, 0.096147, 0.689405, 0.423958, 0.055675, 
0.117940, 0.328456, 0.605808, 0.631768, 0.372170, 0.213723, 0.032700, 
0.447257, 0.440661, 0.728488, 0.299853, 0.148599, 0.649212, 0.498381,
0.049921, 0.496112, 0.607142, 0.562595, 0.990246, 0.739659, 0.108633, 
0.978156, 0.209814, 0.258436, 0.876021, 0.309260, 0.600673, 0.713597, 
0.576967, 0.641402, 0.853930, 0.029173, 0.418111, 0.581593, 0.008394, 
0.589904, 0.661574, 0.979326, 0.275724, 0.111109, 0.440472, 0.120839, 
0.521602, 0.648308, 0.284575, 0.204501, 0.153286, 0.822444, 0.300786, 
0.303906, 0.364717, 0.209038, 0.916831, 0.900245, 0.600685, 0.890002, 
0.581660, 0.431154, 0.705569, 0.551250, 0.417075, 0.403749, 0.696652, 
0.292652, 0.911372, 0.690922, 0.323718, 0.036773, 0.258976, 0.274265, 
0.225076, 0.628965, 0.351644, 0.065158, 0.080340, 0.467271, 0.130643,
0.385914, 0.919315, 0.253821, 0.966163, 0.017439, 0.392610, 0.478792, 
0.978185, 0.072691, 0.982009, 0.097987, 0.731533, 0.401233, 0.107570, 
0.349587, 0.479122, 0.700598, 0.481751, 0.788429, 0.706864, 0.120086, 
0.562691, 0.981797, 0.001223, 0.192120, 0.451543, 0.173092, 0.108960,
0.549594, 0.587892, 0.657534, 0.396365, 0.125153, 0.666420, 0.385823, 
0.890916, 0.436729, 0.128114, 0.369598, 0.759096, 0.044677, 0.904752, 
0.088052, 0.621148, 0.005047, 0.452331, 0.162032, 0.494238, 0.523349, 
0.741829, 0.698450, 0.452316, 0.563487, 0.819776, 0.492160, 0.004210, 
0.647158, 0.551475, 0.362995, 0.177937, 0.814722, 0.727729, 0.867126, 
0.997157, 0.108149, 0.085726, 0.796024, 0.665075, 0.362462, 0.323124,
0.043718, 0.042357, 0.315030, 0.328954, 0.870845, 0.683186, 0.467922, 
0.514894, 0.809971, 0.631979, 0.176571, 0.366320, 0.850621, 0.505555, 
0.749551, 0.750830, 0.401714, 0.481216, 0.438393, 0.508832, 0.867971, 
0.654581, 0.058204, 0.566454, 0.084124, 0.548539, 0.902690, 0.779571, 
0.562058, 0.048082, 0.863109, 0.079290, 0.713559, 0.783496, 0.265266, 
0.672089, 0.786939, 0.143048, 0.086196, 0.876129, 0.408708, 0.229312, 
0.629995, 0.206665, 0.207308, 0.710079, 0.341704, 0.264921, 0.028748, 
0.629222, 0.470173, 0.726228, 0.125243, 0.328249, 0.794187, 0.741340, 
0.489895, 0.189396, 0.724654, 0.092841, 0.039809, 0.860126, 0.247701, 
0.655331, 0.964121, 0.672536, 0.044522, 0.690567, 0.837238, 0.631520, 
0.953734, 0.352484, 0.289026, 0.034152, 0.852575, 0.098454, 0.795529, 
0.452181, 0.826159, 0.186993, 0.820725, 0.440328, 0.922137, 0.704592,
0.915437, 0.738183, 0.733461, 0.193798, 0.929213, 0.161390, 0.318547,
0.888751, 0.430968, 0.740837, 0.193544, 0.872253, 0.563074, 0.274598, 
0.347805, 0.666176, 0.449831, 0.800991, 0.588727, 0.052296, 0.714761, 
0.420620, 0.570325, 0.057550, 0.210888, 0.407312, 0.662848, 0.924382, 
0.895958, 0.775198, 0.688605, 0.025721, 0.301913, 0.791408, 0.500602, 
0.831984, 0.828509, 0.642093, 0.494174, 0.525880, 0.446365, 0.440063, 
0.763114, 0.630358, 0.223943, 0.333806, 0.906033, 0.498306, 0.241278,
0.427640, 0.772683, 0.198082, 0.225379, 0.503894, 0.436599, 0.016503, 
0.803725, 0.189878, 0.291095, 0.499114, 0.151573, 0.079031, 0.904618, 
0.708535, 0.273900, 0.067419, 0.317124, 0.936499, 0.716511, 0.543845, 
0.939909, 0.826574, 0.715090, 0.154864, 0.750150, 0.845808, 0.648108, 
0.556564, 0.644757, 0.140873, 0.799167, 0.632989, 0.444245, 0.471978, 
0.435910, 0.359793, 0.216241, 0.007633, 0.337236, 0.857863, 0.380247, 
0.092517, 0.799973, 0.919000, 0.296798, 0.096989, 0.854831, 0.165369, 
0.568475, 0.216855, 0.020457, 0.835511, 0.538039, 0.999742, 0.620226, 
0.244053, 0.060399, 0.323007, 0.294874, 0.988899, 0.384919, 0.735655, 
0.773428, 0.549776, 0.292882, 0.660611, 0.593507, 0.621118, 0.175269, 
0.682119, 0.794493, 0.868197, 0.632150, 0.807823, 0.509656, 0.482035, 
0.001780, 0.259126, 0.358002, 0.280263, 0.192985, 0.290367, 0.208111, 
0.917633, 0.114422, 0.925491, 0.981110, 0.255570, 0.974862, 0.016629,
0.552599, 0.575741, 0.612978, 0.615965, 0.803615, 0.772334, 0.089745, 
0.838812, 0.634542, 0.113709, 0.755832, 0.577589, 0.667489, 0.529834,
0.325660, 0.817597, 0.316557, 0.335093, 0.737363, 0.260951, 0.737073, 
0.049540, 0.735541, 0.988891, 0.299116, 0.147695, 0.417271, 0.940811, 
0.524160, 0.857968, 0.176403, 0.244835, 0.485759, 0.033353, 0.280319, 
0.750688, 0.755809, 0.924208, 0.095956, 0.962504, 0.275584, 0.173715,
0.942716, 0.706721, 0.078464, 0.576716, 0.804667, 0.559249, 0.900611, 
0.646904, 0.432111, 0.927885, 0.383277, 0.269973, 0.114244, 0.574867, 
0.150703, 0.241855, 0.272871, 0.199950, 0.079719, 0.868566, 0.962833, 
0.789122, 0.320025, 0.905554, 0.234876, 0.991356, 0.061913, 0.732911, 
0.785960, 0.874074, 0.069035, 0.658632, 0.309901, 0.023676, 0.791603, 
0.764661, 0.661278, 0.319583, 0.829650, 0.117091, 0.903124, 0.982098, 
0.161631, 0.193576, 0.670428, 0.857390, 0.003760, 0.572578, 0.222162, 
0.114551, 0.420118, 0.530404, 0.470682, 0.525527, 0.764281, 0.040596, 
0.443275, 0.501124, 0.816161, 0.417467, 0.332172, 0.447565, 0.614591, 
0.559246, 0.805295, 0.226342, 0.155065, 0.714630, 0.160925, 0.760001, 
0.453456, 0.093869, 0.406092, 0.264801, 0.720370, 0.743388, 0.373269, 
0.403098, 0.911923, 0.897249, 0.147038, 0.753037, 0.516093, 0.739257, 
0.175018, 0.045768, 0.735857, 0.801330, 0.927708, 0.240977, 0.591870,
0.921831, 0.540733, 0.149100, 0.423152, 0.806876, 0.397081, 0.061100, 
0.811630, 0.044899, 0.460915, 0.961202, 0.822098, 0.971524, 0.867608, 
0.773604, 0.226616, 0.686286, 0.926972, 0.411613, 0.267873, 0.081937, 
0.226124, 0.295664, 0.374594, 0.533240, 0.237876, 0.669629, 0.599083, 
0.513081, 0.878719, 0.201577, 0.721296, 0.495038, 0.079760, 0.965959,
0.233090, 0.052496, 0.714748, 0.887844, 0.308724, 0.972885, 0.723337,
0.453089, 0.914474, 0.704063, 0.823198, 0.834769, 0.906561, 0.919600,
0.100601, 0.307564, 0.901977, 0.468879, 0.265376, 0.885188, 0.683875,
0.868623, 0.081032, 0.466835, 0.199087, 0.663437, 0.812241, 0.311337,
0.821361, 0.356628, 0.898054, 0.160781, 0.222539, 0.714889, 0.490287,
0.984915, 0.951755, 0.964097, 0.641795, 0.815472, 0.852732, 0.862074,
0.051108, 0.440139, 0.323207, 0.517171, 0.562984, 0.115295, 0.743103,
0.977914, 0.337596, 0.440694, 0.535879, 0.959427, 0.351427, 0.704361,
0.010826, 0.131162, 0.577080, 0.349572, 0.774892, 0.425796, 0.072697,
0.500001, 0.267322, 0.909654, 0.206176, 0.223987, 0.937698, 0.323423,
0.117501, 0.490308, 0.474372, 0.689943, 0.168671, 0.719417, 0.188928,
0.330464, 0.265273, 0.446271, 0.171933, 0.176133, 0.474616, 0.140182,
0.114246, 0.905043, 0.713870, 0.555261, 0.951333};

const unsigned char voronoi_hash[512]= {
0xA2,0xA0,0x19,0x3B,0xF8,0xEB,0xAA,0xEE,0xF3,0x1C,0x67,0x28,0x1D,0xED,0x0,0xDE,0x95,0x2E,0xDC,0x3F,0x3A,0x82,0x35,0x4D,0x6C,0xBA,0x36,0xD0,0xF6,0xC,0x79,0x32,0xD1,0x59,0xF4,0x8,0x8B,0x63,0x89,0x2F,0xB8,0xB4,0x97,0x83,0xF2,0x8F,0x18,0xC7,0x51,0x14,0x65,0x87,0x48,0x20,0x42,0xA8,0x80,0xB5,0x40,0x13,0xB2,0x22,0x7E,0x57,
0xBC,0x7F,0x6B,0x9D,0x86,0x4C,0xC8,0xDB,0x7C,0xD5,0x25,0x4E,0x5A,0x55,0x74,0x50,0xCD,0xB3,0x7A,0xBB,0xC3,0xCB,0xB6,0xE2,0xE4,0xEC,0xFD,0x98,0xB,0x96,0xD3,0x9E,0x5C,0xA1,0x64,0xF1,0x81,0x61,0xE1,0xC4,0x24,0x72,0x49,0x8C,0x90,0x4B,0x84,0x34,0x38,0xAB,0x78,0xCA,0x1F,0x1,0xD7,0x93,0x11,0xC1,0x58,0xA9,0x31,0xF9,0x44,0x6D,
0xBF,0x33,0x9C,0x5F,0x9,0x94,0xA3,0x85,0x6,0xC6,0x9A,0x1E,0x7B,0x46,0x15,0x30,0x27,0x2B,0x1B,0x71,0x3C,0x5B,0xD6,0x6F,0x62,0xAC,0x4F,0xC2,0xC0,0xE,0xB1,0x23,0xA7,0xDF,0x47,0xB0,0x77,0x69,0x5,0xE9,0xE6,0xE7,0x76,0x73,0xF,0xFE,0x6E,0x9B,0x56,0xEF,0x12,0xA5,0x37,0xFC,0xAE,0xD9,0x3,0x8E,0xDD,0x10,0xB9,0xCE,0xC9,0x8D,
0xDA,0x2A,0xBD,0x68,0x17,0x9F,0xBE,0xD4,0xA,0xCC,0xD2,0xE8,0x43,0x3D,0x70,0xB7,0x2,0x7D,0x99,0xD8,0xD,0x60,0x8A,0x4,0x2C,0x3E,0x92,0xE5,0xAF,0x53,0x7,0xE0,0x29,0xA6,0xC5,0xE3,0xF5,0xF7,0x4A,0x41,0x26,0x6A,0x16,0x5E,0x52,0x2D,0x21,0xAD,0xF0,0x91,0xFF,0xEA,0x54,0xFA,0x66,0x1A,0x45,0x39,0xCF,0x75,0xA4,0x88,0xFB,0x5D,
0xA2,0xA0,0x19,0x3B,0xF8,0xEB,0xAA,0xEE,0xF3,0x1C,0x67,0x28,0x1D,0xED,0x0,0xDE,0x95,0x2E,0xDC,0x3F,0x3A,0x82,0x35,0x4D,0x6C,0xBA,0x36,0xD0,0xF6,0xC,0x79,0x32,0xD1,0x59,0xF4,0x8,0x8B,0x63,0x89,0x2F,0xB8,0xB4,0x97,0x83,0xF2,0x8F,0x18,0xC7,0x51,0x14,0x65,0x87,0x48,0x20,0x42,0xA8,0x80,0xB5,0x40,0x13,0xB2,0x22,0x7E,0x57,
0xBC,0x7F,0x6B,0x9D,0x86,0x4C,0xC8,0xDB,0x7C,0xD5,0x25,0x4E,0x5A,0x55,0x74,0x50,0xCD,0xB3,0x7A,0xBB,0xC3,0xCB,0xB6,0xE2,0xE4,0xEC,0xFD,0x98,0xB,0x96,0xD3,0x9E,0x5C,0xA1,0x64,0xF1,0x81,0x61,0xE1,0xC4,0x24,0x72,0x49,0x8C,0x90,0x4B,0x84,0x34,0x38,0xAB,0x78,0xCA,0x1F,0x1,0xD7,0x93,0x11,0xC1,0x58,0xA9,0x31,0xF9,0x44,0x6D,
0xBF,0x33,0x9C,0x5F,0x9,0x94,0xA3,0x85,0x6,0xC6,0x9A,0x1E,0x7B,0x46,0x15,0x30,0x27,0x2B,0x1B,0x71,0x3C,0x5B,0xD6,0x6F,0x62,0xAC,0x4F,0xC2,0xC0,0xE,0xB1,0x23,0xA7,0xDF,0x47,0xB0,0x77,0x69,0x5,0xE9,0xE6,0xE7,0x76,0x73,0xF,0xFE,0x6E,0x9B,0x56,0xEF,0x12,0xA5,0x37,0xFC,0xAE,0xD9,0x3,0x8E,0xDD,0x10,0xB9,0xCE,0xC9,0x8D,
0xDA,0x2A,0xBD,0x68,0x17,0x9F,0xBE,0xD4,0xA,0xCC,0xD2,0xE8,0x43,0x3D,0x70,0xB7,0x2,0x7D,0x99,0xD8,0xD,0x60,0x8A,0x4,0x2C,0x3E,0x92,0xE5,0xAF,0x53,0x7,0xE0,0x29,0xA6,0xC5,0xE3,0xF5,0xF7,0x4A,0x41,0x26,0x6A,0x16,0x5E,0x52,0x2D,0x21,0xAD,0xF0,0x91,0xFF,0xEA,0x54,0xFA,0x66,0x1A,0x45,0x39,0xCF,0x75,0xA4,0x88,0xFB,0x5D,
};

const float hashvectf[768]= {
0.33783,0.715698,-0.611206,-0.944031,-0.326599,-0.045624,-0.101074,-0.416443,-0.903503,0.799286,0.49411,-0.341949,-0.854645,0.518036,0.033936,0.42514,-0.437866,-0.792114,-0.358948,0.597046,0.717377,-0.985413,0.144714,0.089294,-0.601776,-0.33728,-0.723907,-0.449921,0.594513,0.666382,0.208313,-0.10791,
0.972076,0.575317,0.060425,0.815643,0.293365,-0.875702,-0.383453,0.293762,0.465759,0.834686,-0.846008,-0.233398,-0.47934,-0.115814,0.143036,-0.98291,0.204681,-0.949036,-0.239532,0.946716,-0.263947,0.184326,-0.235596,0.573822,0.784332,0.203705,-0.372253,-0.905487,0.756989,-0.651031,0.055298,0.497803,
0.814697,-0.297363,-0.16214,0.063995,-0.98468,-0.329254,0.834381,0.441925,0.703827,-0.527039,-0.476227,0.956421,0.266113,0.119781,0.480133,0.482849,0.7323,-0.18631,0.961212,-0.203125,-0.748474,-0.656921,-0.090393,-0.085052,-0.165253,0.982544,-0.76947,0.628174,-0.115234,0.383148,0.537659,0.751068,
0.616486,-0.668488,-0.415924,-0.259979,-0.630005,0.73175,0.570953,-0.087952,0.816223,-0.458008,0.023254,0.888611,-0.196167,0.976563,-0.088287,-0.263885,-0.69812,-0.665527,0.437134,-0.892273,-0.112793,-0.621674,-0.230438,0.748566,0.232422,0.900574,-0.367249,0.22229,-0.796143,0.562744,-0.665497,-0.73764,
0.11377,0.670135,0.704803,0.232605,0.895599,0.429749,-0.114655,-0.11557,-0.474243,0.872742,0.621826,0.604004,-0.498444,-0.832214,0.012756,0.55426,-0.702484,0.705994,-0.089661,-0.692017,0.649292,0.315399,-0.175995,-0.977997,0.111877,0.096954,-0.04953,0.994019,0.635284,-0.606689,-0.477783,-0.261261,
-0.607422,-0.750153,0.983276,0.165436,0.075958,-0.29837,0.404083,-0.864655,-0.638672,0.507721,0.578156,0.388214,0.412079,0.824249,0.556183,-0.208832,0.804352,0.778442,0.562012,0.27951,-0.616577,0.781921,-0.091522,0.196289,0.051056,0.979187,-0.121216,0.207153,-0.970734,-0.173401,-0.384735,0.906555,
0.161499,-0.723236,-0.671387,0.178497,-0.006226,-0.983887,-0.126038,0.15799,0.97934,0.830475,-0.024811,0.556458,-0.510132,-0.76944,0.384247,0.81424,0.200104,-0.544891,-0.112549,-0.393311,-0.912445,0.56189,0.152222,-0.813049,0.198914,-0.254517,-0.946381,-0.41217,0.690979,-0.593811,-0.407257,0.324524,
0.853668,-0.690186,0.366119,-0.624115,-0.428345,0.844147,-0.322296,-0.21228,-0.297546,-0.930756,-0.273071,0.516113,0.811798,0.928314,0.371643,0.007233,0.785828,-0.479218,-0.390778,-0.704895,0.058929,0.706818,0.173248,0.203583,0.963562,0.422211,-0.904297,-0.062469,-0.363312,-0.182465,0.913605,0.254028,
-0.552307,-0.793945,-0.28891,-0.765747,-0.574554,0.058319,0.291382,0.954803,0.946136,-0.303925,0.111267,-0.078156,0.443695,-0.892731,0.182098,0.89389,0.409515,-0.680298,-0.213318,0.701141,0.062469,0.848389,-0.525635,-0.72879,-0.641846,0.238342,-0.88089,0.427673,0.202637,-0.532501,-0.21405,0.818878,
0.948975,-0.305084,0.07962,0.925446,0.374664,0.055817,0.820923,0.565491,0.079102,0.25882,0.099792,-0.960724,-0.294617,0.910522,0.289978,0.137115,0.320038,-0.937408,-0.908386,0.345276,-0.235718,-0.936218,0.138763,0.322754,0.366577,0.925934,-0.090637,0.309296,-0.686829,-0.657684,0.66983,0.024445,
0.742065,-0.917999,-0.059113,-0.392059,0.365509,0.462158,-0.807922,0.083374,0.996399,-0.014801,0.593842,0.253143,-0.763672,0.974976,-0.165466,0.148285,0.918976,0.137299,0.369537,0.294952,0.694977,0.655731,0.943085,0.152618,-0.295319,0.58783,-0.598236,0.544495,0.203796,0.678223,0.705994,-0.478821,
-0.661011,0.577667,0.719055,-0.1698,-0.673828,-0.132172,-0.965332,0.225006,-0.981873,-0.14502,0.121979,0.763458,0.579742,0.284546,-0.893188,0.079681,0.442474,-0.795776,-0.523804,0.303802,0.734955,0.67804,-0.007446,0.15506,0.986267,-0.056183,0.258026,0.571503,-0.778931,-0.681549,-0.702087,-0.206116,
-0.96286,-0.177185,0.203613,-0.470978,-0.515106,0.716095,-0.740326,0.57135,0.354095,-0.56012,-0.824982,-0.074982,-0.507874,0.753204,0.417969,-0.503113,0.038147,0.863342,0.594025,0.673553,-0.439758,-0.119873,-0.005524,-0.992737,0.098267,-0.213776,0.971893,-0.615631,0.643951,0.454163,0.896851,-0.441071,
0.032166,-0.555023,0.750763,-0.358093,0.398773,0.304688,0.864929,-0.722961,0.303589,0.620544,-0.63559,-0.621948,-0.457306,-0.293243,0.072327,0.953278,-0.491638,0.661041,-0.566772,-0.304199,-0.572083,-0.761688,0.908081,-0.398956,0.127014,-0.523621,-0.549683,-0.650848,-0.932922,-0.19986,0.299408,0.099426,
0.140869,0.984985,-0.020325,-0.999756,-0.002319,0.952667,0.280853,-0.11615,-0.971893,0.082581,0.220337,0.65921,0.705292,-0.260651,0.733063,-0.175537,0.657043,-0.555206,0.429504,-0.712189,0.400421,-0.89859,0.179352,0.750885,-0.19696,0.630341,0.785675,-0.569336,0.241821,-0.058899,-0.464111,0.883789,
0.129608,-0.94519,0.299622,-0.357819,0.907654,0.219238,-0.842133,-0.439117,-0.312927,-0.313477,0.84433,0.434479,-0.241211,0.053253,0.968994,0.063873,0.823273,0.563965,0.476288,0.862152,-0.172516,0.620941,-0.298126,0.724915,0.25238,-0.749359,-0.612122,-0.577545,0.386566,0.718994,-0.406342,-0.737976,
0.538696,0.04718,0.556305,0.82959,-0.802856,0.587463,0.101166,-0.707733,-0.705963,0.026428,0.374908,0.68457,0.625092,0.472137,0.208405,-0.856506,-0.703064,-0.581085,-0.409821,-0.417206,-0.736328,0.532623,-0.447876,-0.20285,-0.870728,0.086945,-0.990417,0.107086,0.183685,0.018341,-0.982788,0.560638,
-0.428864,0.708282,0.296722,-0.952576,-0.0672,0.135773,0.990265,0.030243,-0.068787,0.654724,0.752686,0.762604,-0.551758,0.337585,-0.819611,-0.407684,0.402466,-0.727844,-0.55072,-0.408539,-0.855774,-0.480011,0.19281,0.693176,-0.079285,0.716339,0.226013,0.650116,-0.725433,0.246704,0.953369,-0.173553,
-0.970398,-0.239227,-0.03244,0.136383,-0.394318,0.908752,0.813232,0.558167,0.164368,0.40451,0.549042,-0.731323,-0.380249,-0.566711,0.730865,0.022156,0.932739,0.359741,0.00824,0.996552,-0.082306,0.956635,-0.065338,-0.283722,-0.743561,0.008209,0.668579,-0.859589,-0.509674,0.035767,-0.852234,0.363678,
-0.375977,-0.201965,-0.970795,-0.12915,0.313477,0.947327,0.06546,-0.254028,-0.528259,0.81015,0.628052,0.601105,0.49411,-0.494385,0.868378,0.037933,0.275635,-0.086426,0.957336,-0.197937,0.468903,-0.860748,0.895599,0.399384,0.195801,0.560791,0.825012,-0.069214,0.304199,-0.849487,0.43103,0.096375,
0.93576,0.339111,-0.051422,0.408966,-0.911072,0.330444,0.942841,-0.042389,-0.452362,-0.786407,0.420563,0.134308,-0.933472,-0.332489,0.80191,-0.566711,-0.188934,-0.987946,-0.105988,0.112518,-0.24408,0.892242,-0.379791,-0.920502,0.229095,-0.316376,0.7789,0.325958,0.535706,-0.912872,0.185211,-0.36377,
-0.184784,0.565369,-0.803833,-0.018463,0.119537,0.992615,-0.259247,-0.935608,0.239532,-0.82373,-0.449127,-0.345947,-0.433105,0.659515,0.614349,-0.822754,0.378845,-0.423676,0.687195,-0.674835,-0.26889,-0.246582,-0.800842,0.545715,-0.729187,-0.207794,0.651978,0.653534,-0.610443,-0.447388,0.492584,-0.023346,
0.869934,0.609039,0.009094,-0.79306,0.962494,-0.271088,-0.00885,0.2659,-0.004913,0.963959,0.651245,0.553619,-0.518951,0.280548,-0.84314,0.458618,-0.175293,-0.983215,0.049805,0.035339,-0.979919,0.196045,-0.982941,0.164307,-0.082245,0.233734,-0.97226,-0.005005,-0.747253,-0.611328,0.260437,0.645599,
0.592773,0.481384,0.117706,-0.949524,-0.29068,-0.535004,-0.791901,-0.294312,-0.627167,-0.214447,0.748718,-0.047974,-0.813477,-0.57959,-0.175537,0.477264,-0.860992,0.738556,-0.414246,-0.53183,0.562561,-0.704071,0.433289,-0.754944,0.64801,-0.100586,0.114716,0.044525,-0.992371,0.966003,0.244873,-0.082764,
};

static float Grad(int x, int y, int z, float dx, float dy, float dz) {
	const int h = NoisePerm[NoisePerm[NoisePerm[x] + y] + z] & 15;
	const float u = h < 8 || h == 12 || h == 13 ? dx : dy;
	const float v = h < 4 || h == 12 || h == 13 ? dy : dz;
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static float NoiseWeight(float t) {
	const float t3 = t * t * t;
	const float t4 = t3 * t;
	return 6.f * t4 * t - 15.f * t4 + 10.f * t3;
}

float slg::Noise(float x, float y, float z) {
	// Compute noise cell coordinates and offsets
	int ix = Floor2Int(x);
	int iy = Floor2Int(y);
	int iz = Floor2Int(z);
	const float dx = x - ix, dy = y - iy, dz = z - iz;
	// Compute gradient weights
	ix &= (NOISE_PERM_SIZE - 1);
	iy &= (NOISE_PERM_SIZE - 1);
	iz &= (NOISE_PERM_SIZE - 1);
	const float w000 = Grad(ix, iy, iz, dx, dy, dz);
	const float w100 = Grad(ix + 1, iy, iz, dx - 1, dy, dz);
	const float w010 = Grad(ix, iy + 1, iz, dx, dy - 1, dz);
	const float w110 = Grad(ix + 1, iy + 1, iz, dx - 1, dy - 1, dz);
	const float w001 = Grad(ix, iy, iz + 1, dx, dy, dz - 1);
	const float w101 = Grad(ix + 1, iy, iz + 1, dx - 1, dy, dz - 1);
	const float w011 = Grad(ix, iy + 1, iz + 1, dx, dy - 1, dz - 1);
	const float w111 = Grad(ix + 1, iy + 1, iz + 1, dx - 1, dy - 1, dz - 1);
	// Compute trilinear interpolation of weights
	const float wx = NoiseWeight(dx);
	const float wy = NoiseWeight(dy);
	const float wz = NoiseWeight(dz);
	const float x00 = Lerp(wx, w000, w100);
	const float x10 = Lerp(wx, w010, w110);
	const float x01 = Lerp(wx, w001, w101);
	const float x11 = Lerp(wx, w011, w111);
	const float y0 = Lerp(wy, x00, x10);
	const float y1 = Lerp(wy, x01, x11);
	return Lerp(wz, y0, y1);
}

static float FBm(const Point &P, const float omega, const int maxOctaves) {
	// Compute number of octaves for anti-aliased FBm
	const float foctaves = static_cast<float>(maxOctaves);
	const int octaves = Floor2Int(foctaves);
	// Compute sum of octaves of noise for FBm
	float sum = 0.f, lambda = 1.f, o = 1.f;
	for (int i = 0; i < octaves; ++i) {
		sum += o * Noise(lambda * P);
		lambda *= 1.99f;
		o *= omega;
	}
	const float partialOctave = foctaves - static_cast<float>(octaves);
	sum += o * SmoothStep(.3f, .7f, partialOctave) *
			Noise(lambda * P);
	return sum;
}

float slg::Turbulence(const Point &P, const float omega, const int maxOctaves) {
	// Compute number of octaves for anti-aliased FBm
	const float foctaves = static_cast<float>(maxOctaves);
	const int octaves = Floor2Int(foctaves);
	// Compute sum of octaves of noise for turbulence
	float sum = 0.f, lambda = 1.f, o = 1.f;
	for (int i = 0; i < octaves; ++i) {
		sum += o * fabsf(Noise(lambda * P));
		lambda *= 1.99f;
		o *= omega;
	}
	const float partialOctave = foctaves - static_cast<float>(octaves);
	sum += o * SmoothStep(.3f, .7f, partialOctave) *
	       fabsf(Noise(lambda * P));

	// finally, add in value to account for average value of fabsf(Noise())
	// (~0.2) for the remaining octaves...
	sum += (maxOctaves - foctaves) * 0.2f;

	return sum;
}

/* creates a sine wave */
float slg::tex_sin(float a) {
    a = 0.5f + 0.5f * sinf(a);

    return a;
}

/* creates a saw wave */
float slg::tex_saw(float a) {
    const float b = 2.0f * M_PI;

    int n = (int) (a / b);
    a -= n*b;
    if (a < 0) a += b;
    return a / b;
}

/* creates a triangle wave */
float slg::tex_tri(float a) {
    const float b = 2.0f * M_PI;
    const float rmax = 1.0f;

    a = rmax - 2.0f * fabs(floor((a * (1.0f / b)) + 0.5f) - (a * (1.0f / b)));

    return a;
}

/******************/
/* VORONOI/WORLEY */
/******************/

/* distance metrics for voronoi, e parameter only used in Minkovsky */
/* Camberra omitted, didn't seem useful */

/* distance squared */
static float dist_Squared(float x, float y, float z, float e) { return (x*x + y*y + z*z); }
/* real distance */
static float dist_Real(float x, float y, float z, float e) { return sqrt(x*x + y*y + z*z); }
/* manhattan/taxicab/cityblock distance */
static float dist_Manhattan(float x, float y, float z, float e) { return (fabs(x) + fabs(y) + fabs(z)); }
/* Chebychev */
static float dist_Chebychev(float x, float y, float z, float e) {
	float t;
	x = fabs(x);
	y = fabs(y);
	z = fabs(z);
	t = (x>y)?x:y;
	return ((z>t)?z:t);
}

/* minkovsky preset exponent 0.5 */
static float dist_MinkovskyH(float x, float y, float z, float e) {
	float d = sqrt(fabs(x)) + sqrt(fabs(y)) + sqrt(fabs(z));
	return (d*d);
}

/* minkovsky preset exponent 4 */
static float dist_Minkovsky4(float x, float y, float z, float e) {
	x *= x;
	y *= y;
	z *= z;
	return sqrt(sqrt(x*x + y*y + z*z));
}

/* Minkovsky, general case, slow, maybe too slow to be useful */
static float dist_Minkovsky(float x, float y, float z, float e) {
 return powf(powf(fabs(x), e) + powf(fabs(y), e) + powf(fabs(z), e), 1.0/e);
}


/* Not 'pure' Worley, but the results are virtually the same.
	 Returns distances in da and point coords in pa */
void slg::voronoi(float x, float y, float z, float* da, float* pa, float me, DistanceMetric dtype) {
	int xx, yy, zz, xi, yi, zi;
	float xd, yd, zd, d, *p;

	float (*distfunc)(float, float, float, float);
	switch (dtype) {
		case DISTANCE_SQUARED:
			distfunc = dist_Squared;
			break;
		case MANHATTAN:
			distfunc = dist_Manhattan;
			break;
		case CHEBYCHEV:
			distfunc = dist_Chebychev;
			break;
		case MINKOWSKI_HALF:
			distfunc = dist_MinkovskyH;
			break;
		case MINKOWSKI_FOUR:
			distfunc = dist_Minkovsky4;
			break;
		case MINKOWSKI:
			distfunc = dist_Minkovsky;
			break;
		case ACTUAL_DISTANCE:
		default:
			distfunc = dist_Real;
	}

	xi = (int)(floor(x));
	yi = (int)(floor(y));
	zi = (int)(floor(z));
	da[0] = da[1] = da[2] = da[3] = 1e10f;
	for (xx=xi-1;xx<=xi+1;xx++) {
		for (yy=yi-1;yy<=yi+1;yy++) {
			for (zz=zi-1;zz<=zi+1;zz++) {
				p = HASHPNT(xx, yy, zz);
				xd = x - (p[0] + xx);
				yd = y - (p[1] + yy);
				zd = z - (p[2] + zz);
				d = distfunc(xd, yd, zd, me);
				if (d<da[0]) {
					da[3]=da[2];  da[2]=da[1];  da[1]=da[0];  da[0]=d;
					pa[9]=pa[6];  pa[10]=pa[7];  pa[11]=pa[8];
					pa[6]=pa[3];  pa[7]=pa[4];  pa[8]=pa[5];
					pa[3]=pa[0];  pa[4]=pa[1];  pa[5]=pa[2];
					pa[0]=p[0]+xx;  pa[1]=p[1]+yy;  pa[2]=p[2]+zz;
				}
				else if (d<da[1]) {
					da[3]=da[2];  da[2]=da[1];  da[1]=d;
					pa[9]=pa[6];  pa[10]=pa[7];  pa[11]=pa[8];
					pa[6]=pa[3];  pa[7]=pa[4];  pa[8]=pa[5];
					pa[3]=p[0]+xx;  pa[4]=p[1]+yy;  pa[5]=p[2]+zz;
				}
				else if (d<da[2]) {
					da[3]=da[2];  da[2]=d;
					pa[9]=pa[6];  pa[10]=pa[7];  pa[11]=pa[8];
					pa[6]=p[0]+xx;  pa[7]=p[1]+yy;  pa[8]=p[2]+zz;
				}
				else if (d<da[3]) {
					da[3]=d;
					pa[9]=p[0]+xx;  pa[10]=p[1]+yy;  pa[11]=p[2]+zz;
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

TextureDefinitions::TextureDefinitions() { }

TextureDefinitions::~TextureDefinitions() {
	BOOST_FOREACH(Texture *t, texs)
		delete t;
}

void TextureDefinitions::DefineTexture(const string &name, Texture *newTex) {
	if (IsTextureDefined(name)) {
		const Texture *oldTex = GetTexture(name);

		// Update name/texture definition
		const u_int index = GetTextureIndex(name);
		texs[index] = newTex;
		texsByName.erase(name);
		texsByName.insert(std::make_pair(name, newTex));

		// Update all references
		BOOST_FOREACH(Texture *tex, texs)
			tex->UpdateTextureReferences(oldTex, newTex);

		// Delete the old texture definition
		delete oldTex;
	} else {
		// Add the new texture
		texs.push_back(newTex);
		texsByName.insert(make_pair(name, newTex));
	}
}

Texture *TextureDefinitions::GetTexture(const string &name) {
	// Check if the texture has been already defined
	boost::unordered_map<string, Texture *>::const_iterator it = texsByName.find(name);

	if (it == texsByName.end())
		throw runtime_error("Reference to an undefined texture: " + name);
	else
		return it->second;
}

vector<string> TextureDefinitions::GetTextureNames() const {
	vector<string> names;
	names.reserve(texs.size());

	for (boost::unordered_map<string, Texture *>::const_iterator it = texsByName.begin(); it != texsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void TextureDefinitions::DeleteTexture(const string &name) {
	const u_int index = GetTextureIndex(name);
	texs.erase(texs.begin() + index);
	texsByName.erase(name);
}

u_int TextureDefinitions::GetTextureIndex(const Texture *t) const {
	for (u_int i = 0; i < texs.size(); ++i) {
		if (t == texs[i])
			return i;
	}

	throw runtime_error("Reference to an undefined texture: " + boost::lexical_cast<string>(t));
}

u_int TextureDefinitions::GetTextureIndex(const string &name) {
	return GetTextureIndex(GetTexture(name));
}

//------------------------------------------------------------------------------
// ImageMap
//------------------------------------------------------------------------------

ImageMap::ImageMap(const string &fileName, const float g) {
	gamma = g;

	SDL_LOG("Reading texture map: " << fileName);

	ImageInput *in = ImageInput::open(fileName);
	
	if (in) {
	  const ImageSpec &spec = in->spec();

	  width = spec.width;
	  height = spec.height;
	  channelCount = spec.nchannels;
	  
	  pixels = new float[width * height * channelCount];
	  
	  in->read_image(TypeDesc::FLOAT, &pixels[0]);
	  in->close();
	  delete in;
	}
	else
	  throw runtime_error("Unknown image file format: " + fileName);
	
	ReverseGammaCorrection();
}

ImageMap::ImageMap(float *p, const float g, const u_int count,
		const u_int w, const u_int h) {
	gamma = g;

	pixels = p;

	channelCount = count;
	width = w;
	height = h;
}

ImageMap::~ImageMap() {
	delete[] pixels;
}

void ImageMap::ReverseGammaCorrection() {
	if (gamma != 1.0) {
		if (channelCount < 4) {
			for (u_longlong i=0; i<(width * height * channelCount); i++)
				pixels[i] = powf(pixels[i], gamma);
		}
		else {
			for (u_longlong i=0; i<(width * height * channelCount); i+=4) {
				pixels[i] = powf(pixels[i], gamma);
				pixels[i+1] = powf(pixels[i+1], gamma);
				pixels[i+2] = powf(pixels[i+2], gamma);
			}
		}
	}
}

void ImageMap::Resize(const u_int newWidth, const u_int newHeight) {
	if ((width == newHeight) && (height == newHeight))
		return;

	ImageSpec spec(width, height, channelCount, TypeDesc::FLOAT);
	
	ImageBuf source(spec, (void *)pixels);
	ImageBuf dest;
	
	ROI roi(0,newWidth, 0,newHeight, 0, 1, 0, source.nchannels());
	ImageBufAlgo::resize(dest,source,"",0,roi);
	
	// I can delete the current image
	delete[] pixels;

	width = newWidth;
	height = newHeight;
	
	pixels = new float[width * height * channelCount];
	
	dest.get_pixels(0,width, 0,height, 0,1, TypeDesc::FLOAT, pixels);
}

void ImageMap::WriteImage(const string &fileName) const {
	ImageOutput *out = ImageOutput::create(fileName);
	if (out) {
		ImageSpec spec(width, height, channelCount, TypeDesc::HALF);
		out->open(fileName, spec);
		out->write_image(TypeDesc::FLOAT, pixels);
		out->close();
		delete out;
	}
	else
		throw runtime_error("Failed image save");
}

float ImageMap::GetSpectrumMean() const {
	float mean = 0.f;	
	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int index = x + y * width;
			
			if (channelCount == 1) 
				mean += pixels[index];
			else {
				// channelCount = (3 or 4)
				const float *pixel = &pixels[index * channelCount];
				mean += (pixel[0] + pixel[1] + pixel[2]) * (1.f / 3.f);
			}
		}
	}

	return mean / (width * height);
}

float ImageMap::GetSpectrumMeanY() const {
	float mean = 0.f;	
	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int index = x + y * width;
			
			if (channelCount == 1) 
				mean += pixels[index];
			else {
				// channelCount = (3 or 4)
				const float *pixel = &pixels[index * channelCount];
				mean += Spectrum(pixel[0], pixel[1], pixel[2]).Y();
			}
		}
	}

	return mean / (width * height);
}

ImageMap *ImageMap::Merge(const ImageMap *map0, const ImageMap *map1, const u_int channels,
		const u_int width, const u_int height) {
	if (channels == 1) {
		float *mergedImg = new float[width * height];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				mergedImg[x + y * width] = map0->GetFloat(uv) * map1->GetFloat(uv);
			}
		}

		// I assume the images have the same gamma
		return new ImageMap(mergedImg, map0->GetGamma(), 1, width, height);
	} else if (channels == 3) {
		float *mergedImg = new float[width * height * 3];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				const Spectrum c = map0->GetSpectrum(uv) * map1->GetSpectrum(uv);

				const u_int index = (x + y * width) * 3;
				mergedImg[index] = c.c[0];
				mergedImg[index + 1] = c.c[1];
				mergedImg[index + 2] = c.c[2];
			}
		}

		// I assume the images have the same gamma
		return new ImageMap(mergedImg, map0->GetGamma(), 3, width, height);
	} else
		throw runtime_error("Unsupported number of channels in ImageMap::Merge(): " + ToString(channels));
}

ImageMap *ImageMap::Merge(const ImageMap *map0, const ImageMap *map1, const u_int channels) {
	const u_int width = Max(map0->GetWidth(), map1->GetWidth());
	const u_int height = Max(map0->GetHeight(), map1->GetHeight());

	return ImageMap::Merge(map0, map1, channels, width, height);
}

ImageMap *ImageMap::Resample(const ImageMap *map, const u_int channels,
		const u_int width, const u_int height) {
	if (channels == 1) {
		float *newImg = new float[width * height];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				newImg[x + y * width] = map->GetFloat(uv);
			}
		}

		return new ImageMap(newImg, map->GetGamma(), 1, width, height);
	} else if (channels == 3) {
		float *newImg = new float[width * height * 3];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				const Spectrum c = map->GetSpectrum(uv);

				const u_int index = (x + y * width) * 3;
				newImg[index] = c.c[0];
				newImg[index + 1] = c.c[1];
				newImg[index + 2] = c.c[2];
			}
		}

		return new ImageMap(newImg, map->GetGamma(), 3, width, height);
	} else
		throw runtime_error("Unsupported number of channels in ImageMap::Merge(): " + ToString(channels));
}

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

ImageMapCache::ImageMapCache() {
	allImageScale = 1.f;
}

ImageMapCache::~ImageMapCache() {
	BOOST_FOREACH(ImageMap *m, maps)
		delete m;
}

ImageMap *ImageMapCache::GetImageMap(const string &fileName, const float gamma) {
	// Check if the texture map has been already loaded
	boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.find(fileName);

	if (it == mapByName.end()) {
		// I have yet to load the file

		ImageMap *im = new ImageMap(fileName, gamma);
		const u_int width = im->GetWidth();
		const u_int height = im->GetHeight();

		// Scale the image if required
		if (allImageScale > 1.f) {
			// Enlarge all images
			const u_int newWidth = width * allImageScale;
			const u_int newHeight = height * allImageScale;
			im->Resize(newWidth, newHeight);
		} else if ((allImageScale < 1.f) && (width > 128) && (height > 128)) {
			const u_int newWidth = Max<u_int>(128, width * allImageScale);
			const u_int newHeight = Max<u_int>(128, height * allImageScale);
			im->Resize(newWidth, newHeight);
		}

		mapByName.insert(make_pair(fileName, im));
		maps.push_back(im);

		return im;
	} else {
		//SDL_LOG("Cached image map: " << fileName);
		ImageMap *im = (it->second);
		if (im->GetGamma() != gamma)
			throw runtime_error("Image map: " + fileName + " can not be used with 2 different gamma");
		return im;
	}
}

void ImageMapCache::DefineImageMap(const string &name, ImageMap *im) {
	SDL_LOG("Define ImageMap: " << name);

	boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.find(name);
	if (it == mapByName.end()) {
		// Add the new image definition
		mapByName.insert(make_pair(name, im));
		maps.push_back(im);
	} else {
		// Overwrite the existing image definition
		mapByName.erase(name);
		mapByName.insert(make_pair(name, im));

		const u_int index = GetImageMapIndex(it->second);
		delete maps[index];
		maps[index] = im;
	}
}

u_int ImageMapCache::GetImageMapIndex(const ImageMap *im) const {
	for (u_int i = 0; i < maps.size(); ++i) {
		if (maps[i] == im)
			return i;
	}

	throw runtime_error("Unknown image map: " + boost::lexical_cast<string>(im));
}

void ImageMapCache::GetImageMaps(vector<const ImageMap *> &ims) {
	ims.reserve(maps.size());

	BOOST_FOREACH(ImageMap *im, maps)
		ims.push_back(im);
}

//------------------------------------------------------------------------------
// ConstFloat texture
//------------------------------------------------------------------------------

Properties ConstFloatTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("constfloat1"));
	props.Set(Property("scene.textures." + name + ".value")(value));

	return props;
}

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

Properties ConstFloat3Texture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("constfloat3"));
	props.Set(Property("scene.textures." + name + ".value")(color));

	return props;
}

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTexture::ImageMapTexture(const ImageMap * im, const TextureMapping2D *mp, const float g) :
	imgMap(im), mapping(mp), gain(g) {
	imageY = gain * imgMap->GetSpectrumMeanY();
	imageFilter = gain * imgMap->GetSpectrumMean();
}

float ImageMapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetFloat(mapping->Map(hitPoint));
}

Spectrum ImageMapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetSpectrum(mapping->Map(hitPoint));
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("imagemap"));
	props.Set(Property("scene.textures." + name + ".file")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imgMap)).str() + ".exr"));
	props.Set(Property("scene.textures." + name + ".gamma")("1.0"));
	props.Set(Property("scene.textures." + name + ".gain")(gain));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

float ScaleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return tex1->GetFloatValue(hitPoint) * tex2->GetFloatValue(hitPoint);
}

Spectrum ScaleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return tex1->GetSpectrumValue(hitPoint) * tex2->GetSpectrumValue(hitPoint);
}

Properties ScaleTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("scale"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetName()));

	return props;
}

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

float FresnelApproxN(const float Fr) {
	const float sqrtReflectance = sqrtf(Clamp(Fr, 0.f, .999f));

	return (1.f + sqrtReflectance) /
		(1.f - sqrtReflectance);
}

Spectrum FresnelApproxN(const Spectrum &Fr) {
	const Spectrum sqrtReflectance = Fr.Clamp(0.f, .999f).Sqrt();

	return (Spectrum(1.f) + sqrtReflectance) /
		(Spectrum(1.f) - sqrtReflectance);
}

float FresnelApproxK(const float Fr) {
	const float reflectance = Clamp(Fr, 0.f, .999f);

	return 2.f * sqrtf(reflectance /
		(1.f - reflectance));
}

Spectrum FresnelApproxK(const Spectrum &Fr) {
	const Spectrum reflectance = Fr.Clamp(0.f, .999f);

	return 2.f * Sqrt(reflectance /
		(Spectrum(1.f) - reflectance));
}

float FresnelApproxNTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetFloatValue(hitPoint));
}

Spectrum FresnelApproxNTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetSpectrumValue(hitPoint));
}

float FresnelApproxNTexture::Y() const {
	return FresnelApproxN(tex->Y());
}

float FresnelApproxNTexture::Filter() const {
	return FresnelApproxN(tex->Filter());
}

float FresnelApproxKTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetFloatValue(hitPoint));
}

Spectrum FresnelApproxKTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetSpectrumValue(hitPoint));
}

float FresnelApproxKTexture::Y() const {
	return FresnelApproxK(tex->Y());
}

float FresnelApproxKTexture::Filter() const {
	return FresnelApproxK(tex->Filter());
}

Properties FresnelApproxNTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("fresnelapproxn"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetName()));

	return props;
}

Properties FresnelApproxKTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("fresnelapproxk"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetName()));

	return props;
}

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

float CheckerBoard2DTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetFloatValue(hitPoint);
	else
		return tex2->GetFloatValue(hitPoint);
}

Spectrum CheckerBoard2DTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetSpectrumValue(hitPoint);
	else
		return tex2->GetSpectrumValue(hitPoint);
}

Properties CheckerBoard2DTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("checkerboard2d"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetName()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

float CheckerBoard3DTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p = mapping->Map(hitPoint);
	if ((Floor2Int(p.x) + Floor2Int(p.y) + Floor2Int(p.z)) % 2 == 0)
		return tex1->GetFloatValue(hitPoint);
	else
		return tex2->GetFloatValue(hitPoint);
}

Spectrum CheckerBoard3DTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Point p = mapping->Map(hitPoint);
	if ((Floor2Int(p.x) + Floor2Int(p.y) + Floor2Int(p.z)) % 2 == 0)
		return tex1->GetSpectrumValue(hitPoint);
	else
		return tex2->GetSpectrumValue(hitPoint);
}

Properties CheckerBoard3DTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("checkerboard3d"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetName()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

float MixTexture::Y() const {
	return luxrays::Lerp(amount->Y(), tex1->Y(), tex2->Y());
}

float MixTexture::Filter() const {
	return luxrays::Lerp(amount->Filter(), tex1->Filter(), tex2->Filter());
}

float MixTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const float value1 = tex1->GetFloatValue(hitPoint);
	const float value2 = tex2->GetFloatValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Spectrum MixTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const Spectrum value1 = tex1->GetSpectrumValue(hitPoint);
	const Spectrum value2 = tex2->GetSpectrumValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Properties MixTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("mix"));
	props.Set(Property("scene.textures." + name + ".amount")(amount->GetName()));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetName()));

	return props;
}

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

float FBMTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p(mapping->Map(hitPoint));
	const float value = FBm(p, omega, octaves);
	
	return value;
}

Spectrum FBMTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties FBMTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("fbm"));
	props.Set(Property("scene.textures." + name + ".octaves")(octaves));
	props.Set(Property("scene.textures." + name + ".roughness")(omega));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

Spectrum MarbleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	P *= scale;

	float marble = P.y + variation * FBm(P, omega, octaves);
	float t = .5f + .5f * sinf(marble);
	// Evaluate marble spline at _t_
	static const float c[9][3] = {
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{ .5f, .5f, .5f},
		{ .6f, .59f, .58f},
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{.2f, .2f, .33f},
		{ .58f, .58f, .6f}
	};
#define NC  sizeof(c) / sizeof(c[0])
#define NSEG (NC-3)
	int first = Floor2Int(t * NSEG);
	t = (t * NSEG - first);
#undef NC
#undef NSEG
	Spectrum c0(c[first]), c1(c[first + 1]), c2(c[first + 2]), c3(c[first + 3]);
	// Bezier spline evaluated with de Castilejau's algorithm
	Spectrum s0(Lerp(t, c0, c1));
	Spectrum s1(Lerp(t, c1, c2));
	Spectrum s2(Lerp(t, c2, c3));
	s0 = Lerp(t, s0, s1);
	s1 = Lerp(t, s1, s2);
	// Extra scale of 1.5 to increase variation among colors
	return 1.5f * Lerp(t, s0, s1);
}

float MarbleTexture::Y() const {
	static float c[][3] = { { .58f, .58f, .6f }, { .58f, .58f, .6f }, { .58f, .58f, .6f },
		{ .5f, .5f, .5f }, { .6f, .59f, .58f }, { .58f, .58f, .6f },
		{ .58f, .58f, .6f }, {.2f, .2f, .33f }, { .58f, .58f, .6f }, };
	luxrays::Spectrum cs;
#define NC  sizeof(c) / sizeof(c[0])
	for (u_int i = 0; i < NC; ++i)
		cs += luxrays::Spectrum(c[i]);
	return cs.Y() / NC;
#undef NC
}

float MarbleTexture::Filter() const {
	static float c[][3] = { { .58f, .58f, .6f }, { .58f, .58f, .6f }, { .58f, .58f, .6f },
		{ .5f, .5f, .5f }, { .6f, .59f, .58f }, { .58f, .58f, .6f },
		{ .58f, .58f, .6f }, {.2f, .2f, .33f }, { .58f, .58f, .6f }, };
	luxrays::Spectrum cs;
#define NC  sizeof(c) / sizeof(c[0])
	for (u_int i = 0; i < NC; ++i)
		cs += luxrays::Spectrum(c[i]);
	return cs.Filter() / NC;
#undef NC
}

float MarbleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Properties MarbleTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("marble"));
	props.Set(Property("scene.textures." + name + ".octaves")(octaves));
	props.Set(Property("scene.textures." + name + ".roughness")(omega));
	props.Set(Property("scene.textures." + name + ".scale")(scale));
	props.Set(Property("scene.textures." + name + ".variation")(variation));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

float DotsTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);

	const int sCell = Floor2Int(uv.u + .5f);
	const int tCell = Floor2Int(uv.v + .5f);
	// Return _insideDot_ result if point is inside dot
	if (Noise(sCell + .5f, tCell + .5f) > 0.f) {
		const float radius = .35f;
		const float maxShift = 0.5f - radius;
		const float sCenter = sCell + maxShift *
			Noise(sCell + 1.5f, tCell + 2.8f);
		const float tCenter = tCell + maxShift *
			Noise(sCell + 4.5f, tCell + 9.8f);
		const float ds = uv.u - sCenter, dt = uv.v - tCenter;
		if (ds * ds + dt * dt < radius * radius)
			return insideTex->GetFloatValue(hitPoint);
	}

	return outsideTex->GetFloatValue(hitPoint);
}

Spectrum DotsTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);

	const int sCell = Floor2Int(uv.u + .5f);
	const int tCell = Floor2Int(uv.v + .5f);
	// Return _insideDot_ result if point is inside dot
	if (Noise(sCell + .5f, tCell + .5f) > 0.f) {
		const float radius = .35f;
		const float maxShift = 0.5f - radius;
		const float sCenter = sCell + maxShift *
			Noise(sCell + 1.5f, tCell + 2.8f);
		const float tCenter = tCell + maxShift *
			Noise(sCell + 4.5f, tCell + 9.8f);
		const float ds = uv.u - sCenter, dt = uv.v - tCenter;
		if (ds * ds + dt * dt < radius * radius)
			return insideTex->GetSpectrumValue(hitPoint);
	}

	return outsideTex->GetSpectrumValue(hitPoint);
}

Properties DotsTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("dots"));
	props.Set(Property("scene.textures." + name + ".inside")(insideTex->GetName()));
	props.Set(Property("scene.textures." + name + ".outside")(outsideTex->GetName()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

BrickTexture::BrickTexture(const TextureMapping3D *mp, const Texture *t1,
		const Texture *t2, const Texture *t3,
		float brickw, float brickh, float brickd, float mortar,
		float r, float bev, const string &b) :
		mapping(mp), tex1(t1), tex2(t2), tex3(t3),
		brickwidth(brickw), brickheight(brickh), brickdepth(brickd), mortarsize(mortar),
		run(r), initialbrickwidth(brickw), initialbrickheight(brickh), initialbrickdepth(brickd) {
	if (b == "stacked") {
		bond = RUNNING;
		run = 0.f;
	} else if (b == "flemish")
		bond = FLEMISH;
	else if (b == "english") {
		bond = ENGLISH;
		run = 0.25f;
	} else if (b == "herringbone")
		bond = HERRINGBONE;
	else if (b == "basket")
		bond = BASKET;
	else if (b == "chain link") {
		bond = KETTING;
		run = 1.25f;
		offset = Point(.25f, -1.f, 0.f);
	} else {
		bond = RUNNING;
		offset = Point(0.f, -.5f , 0.f);
	}
	if (bond == HERRINGBONE || bond == BASKET) {
		proportion = floorf(brickwidth / brickheight);
		brickdepth = brickheight = brickwidth;
		invproportion = 1.f / proportion;
	}
	mortarwidth = mortarsize / brickwidth;
	mortarheight = mortarsize / brickheight;
	mortardepth = mortarsize / brickdepth;
	bevelwidth = bev / brickwidth;
	bevelheight = bev / brickheight;
	beveldepth = bev / brickdepth;
	usebevel = bev > 0.f;
}

float BrickTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

bool BrickTexture::RunningAlternate(const Point &p, Point &i, Point &b,
		int nWhole) const {
	const float sub = nWhole + 0.5f;
	const float rsub = ceilf(sub);
	i.z = floorf(p.z);
	b.x = (p.x + i.z * run) / sub;
	b.y = (p.y + i.z * run) / sub;
	i.x = floorf(b.x);
	i.y = floorf(b.y);
	b.x = (b.x - i.x) * sub;
	b.y = (b.y - i.y) * sub;
	b.z = (p.z - i.z) * sub;
	i.x += floor(b.x) / rsub;
	i.y += floor(b.y) / rsub;
	b.x -= floor(b.x);
	b.y -= floor(b.y);
	return b.z > mortarheight && b.y > mortardepth &&
		b.x > mortarwidth;
}

bool BrickTexture::Basket(const Point &p, Point &i) const {
	i.x = floorf(p.x);
	i.y = floorf(p.y);
	float bx = p.x - i.x;
	float by = p.y - i.y;
	i.x += i.y - 2.f * floorf(0.5f * i.y);
	const bool split = (i.x - 2.f * floor(0.5f * i.x)) < 1.f;
	if (split) {
		bx = fmodf(bx, invproportion);
		i.x = floorf(proportion * p.x) * invproportion;
	} else {
		by = fmodf(by, invproportion);
		i.y = floorf(proportion * p.y) * invproportion;
	}
	return by > mortardepth && bx > mortarwidth;
}

bool BrickTexture::Herringbone(const Point &p, Point &i) const {
	i.y = floorf(proportion * p.y);
	const float px = p.x + i.y * invproportion;
	i.x = floorf(px);
	float bx = 0.5f * px - floorf(px * 0.5f);
	bx *= 2.f;
	float by = proportion * p.y - floorf(proportion * p.y);
	by *= invproportion;
	if (bx > 1.f + invproportion) {
		bx = proportion * (bx - 1.f);
		i.y -= floorf(bx - 1.f);
		bx -= floorf(bx);
		bx *= invproportion;
		by = 1.f;
	} else if (bx > 1.f) {
		bx = proportion * (bx - 1.f);
		i.y -= floorf(bx - 1.f);
		bx -= floorf(bx);
		bx *= invproportion;
	}
	return by > mortarheight && bx > mortarwidth;
}

bool BrickTexture::Running(const Point &p, Point &i, Point &b) const {
	i.z = floorf(p.z);
	b.x = p.x + i.z * run;
	b.y = p.y - i.z * run;
	i.x = floorf(b.x);
	i.y = floorf(b.y);
	b.z = p.z - i.z;
	b.x -= i.x;
	b.y -= i.y;
	return b.z > mortarheight && b.y > mortardepth &&
		b.x > mortarwidth;
}

bool BrickTexture::English(const Point &p, Point &i, Point &b) const {
	i.z = floorf(p.z);
	b.x = p.x + i.z * run;
	b.y = p.y - i.z * run;
	i.x = floorf(b.x);
	i.y = floorf(b.y);
	b.z = p.z - i.z;
	const float divider = floorf(fmodf(fabsf(i.z), 2.f)) + 1.f;
	b.x = (divider * b.x - floorf(divider * b.x)) / divider;
	b.y = (divider * b.y - floorf(divider * b.y)) / divider;
	return b.z > mortarheight && b.y > mortardepth &&
		b.x > mortarwidth;
}

Spectrum BrickTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
#define BRICK_EPSILON 1e-3f
	const Point P(mapping->Map(hitPoint));

	const float offs = BRICK_EPSILON + mortarsize;
	Point bP(P + Point(offs, offs, offs));

	// Normalize coordinates according to brick dimensions
	bP.x /= brickwidth;
	bP.y /= brickdepth;
	bP.z /= brickheight;

	bP += offset;

	Point brickIndex;
	Point bevel;
	bool b;
	switch (bond) {
		case FLEMISH:
			b = RunningAlternate(bP, brickIndex, bevel, 1);
			break;
		case RUNNING:
			b = Running(bP, brickIndex, bevel);
			break;
		case ENGLISH:
			b = English(bP, brickIndex, bevel);
			break;
		case HERRINGBONE:
			b = Herringbone(bP, brickIndex);
			break;
		case BASKET:
			b = Basket(bP, brickIndex);
			break;
		case KETTING:
			b = RunningAlternate(bP, brickIndex, bevel, 2);
			break; 
		default:
			b = true;
			break;
	}

	if (b) {
		// Brick texture * Modulation texture
		return tex1->GetSpectrumValue(hitPoint) * tex3->GetSpectrumValue(hitPoint);
	} else {
		// Mortar texture
		return tex2->GetSpectrumValue(hitPoint);
	}
#undef BRICK_EPSILON
}

Properties BrickTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("brick"));
	props.Set(Property("scene.textures." + name + ".bricktex")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".mortartex")(tex2->GetName()));
	props.Set(Property("scene.textures." + name + ".brickmodtex")(tex3->GetName()));
	props.Set(Property("scene.textures." + name + ".brickwidth")(brickwidth));
	props.Set(Property("scene.textures." + name + ".brickheight")(brickheight));
	props.Set(Property("scene.textures." + name + ".brickdepth")(brickdepth));
	props.Set(Property("scene.textures." + name + ".mortarsize")(mortarsize));
	props.Set(Property("scene.textures." + name + ".brickrun")(run));
	props.Set(Property("scene.textures." + name + ".brickbevel")(bevelwidth * brickwidth));

	string brickBondValue;
	switch (bond) {
		case FLEMISH:
			brickBondValue = "flemish";
			break;
		case ENGLISH:
			brickBondValue = "english";
			break;
		case HERRINGBONE:
			brickBondValue = "herringbone";
			break;
		case BASKET:
			brickBondValue = "basket";
			break;
		case KETTING:
			brickBondValue = "chain link";
			break;
		default:
		case RUNNING:
			brickBondValue = "stacked";
			break;
	}
	props.Set(Property("scene.textures." + name + ".brickbond")(brickBondValue));

	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

float AddTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return tex1->GetFloatValue(hitPoint) + tex2->GetFloatValue(hitPoint);
}

Spectrum AddTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return tex1->GetSpectrumValue(hitPoint) + tex2->GetSpectrumValue(hitPoint);
}

Properties AddTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("add"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetName()));

	return props;
}

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

float WindyTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point P(mapping->Map(hitPoint));
	const float windStrength = FBm(.1f * P, .5f, 3);
	const float waveHeight = FBm(P, .5f, 6);

	return fabsf(windStrength) * waveHeight;
}

Spectrum WindyTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties WindyTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("windy"));

	return props;
}

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

float WrinkledTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p(mapping->Map(hitPoint));
	return Turbulence(p, omega, octaves);
}

Spectrum WrinkledTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties WrinkledTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("wrinkled"));
	props.Set(Property("scene.textures." + name + ".octaves")(octaves));
	props.Set(Property("scene.textures." + name + ".roughness")(omega));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

float UVTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum UVTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	
	return Spectrum(uv.u - Floor2Int(uv.u), uv.v - Floor2Int(uv.v), 0.f);
}

Properties UVTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("uv"));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

float BandTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum BandTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float a = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);

	if (a < offsets.front())
		return values.front();
	if (a >= offsets.back())
		return values.back();
	// upper_bound is not available on OpenCL
	//const u_int p = upper_bound(offsets.begin(), offsets.end(), a) - offsets.begin();
	u_int p = 0;
	for (; p < offsets.size(); ++p) {
		if (a < offsets[p])
			break;
	}

	return Lerp((a - offsets[p - 1]) / (offsets[p] - offsets[p - 1]),
			values[p - 1], values[p]);
}

Properties BandTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("band"));
	props.Set(Property("scene.textures." + name + ".amount")(amount->GetName()));

	for (u_int i = 0; i < offsets.size(); ++i) {
		props.Set(Property("scene.textures." + name + ".offset" + ToString(i))(offsets[i]));
		props.Set(Property("scene.textures." + name + ".value" + ToString(i))(values[i]));
	}

	return props;
}

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

float HitPointColorTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.color.Y();
}

Spectrum HitPointColorTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return hitPoint.color;
}

Properties HitPointColorTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointcolor"));

	return props;
}

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

float HitPointAlphaTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.alpha;
}

Spectrum HitPointAlphaTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(hitPoint.alpha);
}

Properties HitPointAlphaTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointalpha"));

	return props;
}

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

float HitPointGreyTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return (channel > 2) ? hitPoint.color.Y() : hitPoint.color.c[channel];
}

Spectrum HitPointGreyTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float v = (channel > 2) ? hitPoint.color.Y() : hitPoint.color.c[channel];
	return Spectrum(v);
}

Properties HitPointGreyTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointgrey"));
	props.Set(Property("scene.textures." + name + ".channel")(
		((channel != 0) && (channel != 1) && (channel != 2)) ? -1 : ((int)channel)));

	return props;
}

//------------------------------------------------------------------------------
// NormalMap textures
//------------------------------------------------------------------------------

luxrays::UV NormalMapTexture::GetDuv(const HitPoint &hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float sampleDistance) const {
    Spectrum rgb = tex->GetSpectrumValue(hitPoint);
    rgb.Clamp(-1.f, 1.f);

	// Normal from normal map
	Vector n(rgb.c[0], rgb.c[1], rgb.c[2]);
	n = 2.f * n - Vector(1.f, 1.f, 1.f);

	const Vector k = Vector(hitPoint.shadeN);

	// Transform n from tangent to object space
	const Vector &t1 = dpdu;
	const Vector &t2 = dpdv;
    const float btsign = (Dot(dpdv, hitPoint.shadeN) > 0.f) ? 1.f : -1.f;

	// Magnitude of btsign is the magnitude of the interpolated normal
	const Vector kk = k * fabsf(btsign);

	// tangent -> object
	n = Normalize(n.x * t1 + n.y * btsign * t2 + n.z * kk);	

	// Since n is stored normalized in the normal map
	// we need to recover the original length (lambda).
	// We do this by solving 
	//   lambda*n = dp/du x dp/dv
	// where 
	//   p(u,v) = base(u,v) + h(u,v) * k
	// and
	//   k = dbase/du x dbase/dv
	//
	// We recover lambda by dotting the above with k
	//   k . lambda*n = k . (dp/du x dp/dv)
	//   lambda = (k . k) / (k . n)
	// 
	// We then recover dh/du by dotting the first eq by dp/du
	//   dp/du . lambda*n = dp/du . (dp/du x dp/dv)
	//   dp/du . lambda*n = dh/du * [dbase/du . (k x dbase/dv)]
	//
	// The term "dbase/du . (k x dbase/dv)" reduces to "-(k . k)", so we get
	//   dp/du . lambda*n = dh/du * -(k . k)
	//   dp/du . [(k . k) / (k . n)*n] = dh/du * -(k . k)
	//   dp/du . [-n / (k . n)] = dh/du
	// and similar for dh/dv
	// 
	// Since the recovered dh/du will be in units of ||k||, we must divide
	// by ||k|| to get normalized results. Using dg.nn as k in the last eq 
	// yields the same result.
	const Vector nn = (-1.f / Dot(k, n)) * n;

	return UV(Dot(dpdu, nn), Dot(dpdv, nn));
}

Properties NormalMapTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("normalmap"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetName()));

	return props;
}
