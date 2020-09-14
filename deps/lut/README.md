# Color Lookup Table
lut.hpp is a C++ 11 single required source file, that can be used to achieve color correction and color grading, you can see ue4 [docs](https://docs.unrealengine.com/en-us/Engine/Rendering/PostProcessEffects/UsingLUTs) for more information

# Color Neutral LUT
[![link text](ColorNeutralLUT.png)]()
[![link text](1.jpg)]()

# Candle Light LUT
[![link text](CandlelightLUT.png)]()
[![link text](2.jpg)]()

# Usage
```c++
#include "lut.hpp"

// Usage 1
auto lut = octoon::image::lut::parse("xxx.cube"); // load the lut from .cube file
for (std::size_t i = 0; i < image.size(); i++)
{
	auto data = lut.lookup(rgb.r, rgb.g, rgb.b); // The (r,g,b) can be extended to support these types of std::uint8_t, std::uint16_t, std::uint32_t, float, double
	rgb.r = data[0];
	rgb.g = data[1];
	rgb.b = data[2];
}

// Usage 2
auto lut = octoon::image::lut::parse("xxx.cube"); // load the lut from .cube file
lut.lookup(image.data(), image.data(), image.size(), 3); // The (r,g,b) can be extended to support these types of std::uint8_t, std::uint16_t, std::uint32_t, float, double

// Serializable to .cube stream
method 1 : std::cout << lut.dump();
method 2 : std::cout << lut;

// Serializable to image
auto image = octoon::image::Image(octoon::image::Format::R8G8B8UNorm, lut.width, lut.height);
std::memcpy((std::uint8_t*)image.data(), lut.data.get(), lut.width * lut.height * lut.channel);
image.save("C:\\Users\\Administrator\\Desktop\\1.png", "png");
```

[License (MIT)](https://raw.githubusercontent.com/ray-cast/lut/master/LICENSE.txt)
-------------------------------------------------------------------------------
    MIT License

    Copyright (c) 2018 Rui

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
	BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
	AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
	CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.