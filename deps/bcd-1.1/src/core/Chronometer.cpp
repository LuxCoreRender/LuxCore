// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#include "Chronometer.h"

#include <sstream>
#include <cmath>

using namespace std;

namespace bcd
{

	Chronometer::Chronometer() : m_isRunning(false), m_elapsedTime(0)
	{
	}

	void Chronometer::reset()
	{
		m_isRunning = false;
		m_elapsedTime = 0;
	}

	void Chronometer::start()
	{
		if(m_isRunning)
			return;
		m_isRunning = true;
		m_startTime = std::chrono::high_resolution_clock::now();
	}

	void Chronometer::stop()
	{
		if(!m_isRunning)
			return;
		m_elapsedTime = getElapsedTime();
		m_isRunning = false;
	}

	float Chronometer::getElapsedTime() const
	{
		if(m_isRunning)
		{
			std::chrono::duration<float> fs = std::chrono::high_resolution_clock::now() - m_startTime;
			return m_elapsedTime + fs.count();
		}
		return m_elapsedTime;
	}

	string Chronometer::getStringFromTime(float i_timeInSeconds)
	{
		ostringstream oss;

		float hms; // for hour minute second
		oss << i_timeInSeconds*1000.f << " ms";
		if(i_timeInSeconds >= 1.f)
		{
			oss << " ( ";
			if(i_timeInSeconds >= 3600.f)
			{
				hms = floor(i_timeInSeconds/3600.f);
				oss << hms << "h ";
				i_timeInSeconds -= hms * 3600.f;
			}
			if(i_timeInSeconds >= 60.f)
			{
				hms = floor(i_timeInSeconds/60.f);
				oss << hms << "min ";
				i_timeInSeconds -= hms * 60.f;
			}
			hms = floor(i_timeInSeconds);
			oss << hms << "s ";
			i_timeInSeconds -= hms;
			hms = floor(i_timeInSeconds*1000.f);
			oss << hms << "ms )";
		}

		return oss.str();
	}

	void Chronometer::printElapsedTime(std::ostream& o_stream)
	{
		o_stream << getStringFromTime(getElapsedTime());
	}

	ostream& operator<<(ostream& i_rOs, const Chronometer& i_rChrono)
	{
		i_rOs << Chronometer::getStringFromTime(i_rChrono.getElapsedTime());
		return i_rOs;
	}

} // namespace bcd

