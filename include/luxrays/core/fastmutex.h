/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#ifndef LUX_FASTMUTEX_H
#define LUX_FASTMUTEX_H

#if defined(__WIN32__)
#include <windows.h>
#else
#ifdef WIN32
#undef min
#undef max
#endif
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#endif

namespace luxrays {

#if defined(WIN32)

class fastmutex {
private:
	CRITICAL_SECTION cs;

	fastmutex(fastmutex const &);
	fastmutex &operator=(fastmutex const &);

public:
	fastmutex() {
		InitializeCriticalSection(&cs);
	}

	~fastmutex() {
		DeleteCriticalSection(&cs);
	}

	bool try_lock() {
		return TryEnterCriticalSection(&cs) == TRUE;
	}

	void lock() {
		EnterCriticalSection(&cs);
	}

	void unlock() {
		LeaveCriticalSection(&cs);
	}

	class scoped_lock;
	friend class scoped_lock;
	friend class fastcondvar;

	class scoped_lock {
	private:
		fastmutex &m_;
		bool has_lock;

		scoped_lock(scoped_lock const &);
		scoped_lock &operator=(scoped_lock const &);

	public:
		scoped_lock(fastmutex &m) : m_(m) {
			m_.lock();
			has_lock = true;
		}

		~scoped_lock() {
			if (has_lock)
				m_.unlock();
		}

		void unlock() {
			if (has_lock)
				m_.unlock();
			has_lock = false;
		}

		friend class fastcondvar;
	};
};

class fastcondvar {
private:
	CONDITION_VARIABLE cvar;

public:
	fastcondvar() { InitializeConditionVariable(&cvar); };
	~fastcondvar() { };

	void wait(fastmutex::scoped_lock &l) {
		SleepConditionVariableCS(&cvar, &l.m_.cs, 250); //INFINITE);

#if defined(WIN32)
		if (boost::this_thread::interruption_requested())
			throw boost::thread_interrupted();
#endif
	}

	void notify_one() {
		WakeConditionVariable(&cvar);
	}
};

#else

typedef boost::mutex fastmutex;
typedef boost::condition_variable fastcondvar;

#endif

}

#endif 	/* _LUXRAYS_FASTMUTEX_H */
