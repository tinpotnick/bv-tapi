/*
    phone.h, part of bv-tapi a SIP-TAPI bridge.
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

#ifndef BV_PHONE
#define BV_PHONE


#include <map>
#include "phonecall.h"


typedef std::map<std::string, PhoneCall> PhoneCallContainer;

class Phone
{
public:

	Phone(void);

	// returs true if we have processed it, false otherwise.
	bool UpdateState(std::string identity, 
						   std::string dialog_id, 
						   std::string direction, 
						   std::string state, 
						   std::string remote_display, 
						   std::string remote_identity);

	size_t GetCallCount(void);
	inline std::string GetUrl(void) { return this->url; }

	PhoneCallContainer &GetPhoneCalls(void) {return this->phoneCalls;}

	int MarkAsRefered(std::string identity, std::string to, HTAPICALL hcall);


private:

	std::string url;
	PhoneCallContainer phoneCalls;

	bool referedWaiting;
};





#endif