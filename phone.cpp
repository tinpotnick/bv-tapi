/*
    phone.cpp, part of bv-tapi a SIP-TAPI bridge.
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

#pragma warning(disable:4996)



#include <winsock2.h>
#include <tspi.h>
#include <tapi.h>

#include "PhoneCall.h"
#include "utilities.h"
#include "sofia-bridge.h"

#include "phone.h"

Phone::Phone()
{
	this->referedWaiting = false;
}


bool Phone::UpdateState(std::string identity, 
						   std::string dialog_id, 
						   std::string direction, 
						   std::string state, 
						   std::string remote_display, 
						   std::string remote_identity)
{
	// In case we don't know about it already...
	this->url = identity;

	if ( true == this->referedWaiting )
	{
		phoneCalls[dialog_id] = phoneCalls["refered"];
		phoneCalls[dialog_id].setCallID(dialog_id);
		phoneCalls.erase("refered");
		this->referedWaiting = false;

		bv_logger("Phone::UpdateState: found refered call\n");
	}
	
	
	phoneCalls[dialog_id].UpdateState(identity, 
										dialog_id, 
										direction, 
										state, 
										remote_display, 
										remote_identity);
	
	
	
	if ( "terminated" == state || "hangup" == state )
	{
		//clean up
		PhoneCallContainer::iterator it;
		it = phoneCalls.find(dialog_id);
		if ( it != phoneCalls.end() )
		{
			phoneCalls.erase(it);
		}
	}

	// it's for us.
	return true;
}


size_t Phone::GetCallCount(void)
{
	return this->phoneCalls.size();
}

int Phone::MarkAsRefered(std::string identity, std::string to, HTAPICALL hcall)
{
	phoneCalls["refered"].markAsRefered(identity, to, hcall);
	this->referedWaiting = true;
	return phoneCalls["refered"].getUID();
}



