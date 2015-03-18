/*
Utilities.h, part of bv-tapi a SIP-TAPI bridge.
Copyright (C) 2011  Nick Knight

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/

//Public functions from this file

#include <windows.h>
#include <string>
#include <list>

#include <comutil.h>

#ifndef UTILITIES
#define UTILITIES

typedef std::list<std::string> stringList;
stringList splitString(std::string str, char delim);

std::string getuserfromsipuri(std::string uri);
std::string getdomainfromsipuri(std::string uri);
std::string getnormalizedsipuri(std::string uri);

//some mutex simplification 
//courtsey of a doc@ http://world.std.com/~jimf/papers/c++sync/c++sync.html


class Mutex {
private:
	CRITICAL_SECTION lock;
	//HANDLE lock;
	std::string name;

public:
	Mutex(std::string name);
	~Mutex();
	void Lock(std::string ref);
	void Unlock(std::string ref);
};

// This class is designed to releas the mutex
// when a function is return - perhaps unexpentantly
// through an exception. The destructor will clear us up.
class SafeMutex {
private:
	Mutex *ourMutex;
	bool Locked;
	std::string ourRef;

public:

	SafeMutex(Mutex *mut, std::string ref, bool initLocked = true)
	{
		this->ourMutex = mut;
		this->ourRef = ref;

		if ( true == initLocked )
		{
			this->Locked = true;

			if ( NULL != this->ourMutex ) 
			{
				this->ourMutex->Lock(this->ourRef);
			}
		}
		else
		{
			this->Locked = false;
		}
	}

	~SafeMutex()
	{
		if ( NULL != this->ourMutex )
		{
			if ( true == this->Locked )
			{
				this->ourMutex->Unlock(this->ourRef);
			}
		}
	}

	void Unlock(void)
	{
		if ( NULL != this->ourMutex ) 
		{

			if ( true == this->Locked )
			{
				this->ourMutex->Unlock(this->ourRef);
				this->Locked = false;
			}
		}
	}

	void Lock(void)
	{
		if ( NULL != this->ourMutex ) 
		{

			if ( true != this->Locked )
			{
				this->ourMutex->Lock(this->ourRef);
				this->Locked = true;
			}
		}
	}
};



std::string frombstr(_bstr_t orig);


#endif


