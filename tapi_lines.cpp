/*
    tapi_lines.cpp, part of bv-tapi a SIP-TAPI bridge.
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

#include "tapi_lines.h"


static tapi_lines_list ourlinelist;


////////////////////////////////////////////////////////////////////////////////
// Function add_line_entry
//
// When a line is opened we call this to get a reference and store it.
//
////////////////////////////////////////////////////////////////////////////////
HDRVLINE add_line_entry(DWORD dwDeviceID,
					HTAPILINE htLine,
					LINEEVENT pfnEventProc,
					std::string uri)
{
	tapi_line *tmp = new tapi_line(dwDeviceID, htLine, pfnEventProc, uri);
	ourlinelist.push_front(tmp);
	return (HDRVLINE)tmp;
}


////////////////////////////////////////////////////////////////////////////////
// Function remove_line_entry
//
// When a line is closed remove our references to it.
//
////////////////////////////////////////////////////////////////////////////////
void remove_line_entry (HDRVLINE hdLine)
{
	tapi_lines_list::iterator it;

	for (it = ourlinelist.begin(); it != ourlinelist.end(); it++ )
	{
		if ((HDRVLINE)(*it) == hdLine )
		{
			ourlinelist.erase(it);
			delete hdLine;
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function get_line_entry
//
// Search for a line to ensure it exsists
//
////////////////////////////////////////////////////////////////////////////////
tapi_line* get_line_entry (HDRVLINE hdLine)
{
	tapi_lines_list::iterator it;

	for (it = ourlinelist.begin(); it != ourlinelist.end(); it++ )
	{
		if ((HDRVLINE)(*it) == hdLine )
		{
			return *it;
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Function is_already_subscribed
//
// If a line is already subscribed to a resource - we don't need to subscribe 
// again - we just share the information.
//
////////////////////////////////////////////////////////////////////////////////
bool is_already_subscribed(std::string uri)
{
	tapi_lines_list::iterator it;

	for (it = ourlinelist.begin(); it != ourlinelist.end(); it++ )
	{
		if ( (*it)->get_subscribed_uri() == uri )
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
// Function get_line_list
//
// Allow other modules access to our line list.
//
////////////////////////////////////////////////////////////////////////////////
tapi_lines_list get_line_list(void)
{
	return ourlinelist;
}