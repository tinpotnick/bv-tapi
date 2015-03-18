/*
    Utilities.cpp, part of bv-tapi a SIP-TAPI bridge.
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


#define STRICT
#include <winsock2.h>
#include <tapi.h>
#include <tspi.h>
#include <string>

#include "utilities.h"
#include "sofia-bridge.h"

std::string frombstr(_bstr_t orig)
{

	if ( SysStringLen(orig.GetBSTR()) == 0 )
	{
		return "";
	}

	char *p = _com_util::ConvertBSTRToString(orig.GetBSTR());
	std::string retval(p);
	delete [] p;

	return retval;

}

// removes things like the port (sip:user@dom:port)
// should just return something like sip:user@dom
std::string getnormalizedsipuri(std::string uri)
{
	std::string user = getuserfromsipuri(uri);
	std::string dom = getdomainfromsipuri(uri);

	return std::string("sip:") + user + std::string("@") + dom;
}


std::string getuserfromsipuri(std::string uri)
{
	if ( "" == uri ) return "";

	stringList remdomain = splitString(uri, '@');

	// we may (or may not) have sip: at the front
	stringList remproto = splitString(remdomain.front(), ':');

	if ( remproto.size() == 1 )
	{
		return remproto.front();
	}

	remproto.pop_front();
	return remproto.front();
}

std::string getdomainfromsipuri(std::string uri)
{
	if ( "" == uri ) return "";

	stringList remdomain = splitString(uri, '@');


	if ( remdomain.size() == 1 )
	{
		return "";
	}

	remdomain.pop_front();
	std::string afterpart = remdomain.front();

	// now check for anything after the dom
	stringList postdomain = splitString(afterpart, ':');
	return postdomain.front();

}


//STL utilities
stringList splitString(std::string str, char delim)
{
	stringList ourList;

	std::string::iterator it;
	std::string buf;

	for ( it = str.begin(); it != str.end() ; it ++ )
	{
		if ( (*it) == delim )
		{
			ourList.push_back(buf);
			buf = "";
		}
		else
		{
			buf += (*it);
		}
	}
	if ( buf != "" )
	{
		ourList.push_back(buf);
	}

	return ourList;
}

Mutex::Mutex(std::string name) 
{
	this->name = name;

	bv_logger("Mutex::Mutex, %i, %s\n", this, this->name.c_str());
	try
	{
		InitializeCriticalSection(&this->lock);
		//this->lock = CreateMutex(NULL, FALSE, NULL);
	}
	catch(...)
	{
		bv_logger("Mutex::Mutex exception\n");
	}
}

Mutex::~Mutex() 
{
	bv_logger("Mutex::~Mutex, %i, %s\n", this, this->name.c_str());

	try
	{
		//make sure no oneis waiting or using...
		//this->Lock();
		DeleteCriticalSection(&this->lock);
		//CloseHandle(this->lock);
	}
	catch(...)
	{
		bv_logger("Mutex::~Mutex exception\n");
	}
}

void Mutex::Lock(std::string ref) 
{
	bv_logger("Mutex::Lock, %i, 0x%x, %s, %s\n", this, GetCurrentThread(), this->name.c_str(), ref.c_str());

	try
	{
		EnterCriticalSection(&this->lock);
		//WaitForSingleObject(this->lock, 0);
		bv_logger("Mutex::Locked, %i, 0x%x, %s, %s\n", this, GetCurrentThread(), this->name.c_str(), ref.c_str());
	}
	catch(...)
	{
		bv_logger("Mutex::Lock exception\n");
	}
}

void Mutex::Unlock(std::string ref) 
{
	bv_logger("Mutex::Unlock, %i, 0x%x, %s, %s\n", this, GetCurrentThread(), this->name.c_str(), ref.c_str());

	try
	{

		LeaveCriticalSection(&this->lock);
		bv_logger("Mutex::Unlocked, %i, 0x%x, %s, %s\n", this, GetCurrentThread(), this->name.c_str(), ref.c_str());
		//ReleaseMutex(this->lock);
	}
	catch(...)
	{
		bv_logger("Mutex::Unlock exception\n");
	}
}

