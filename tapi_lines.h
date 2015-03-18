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

#ifndef TAPI_LINES
#define TAPI_LINES

#include <list>
#include <string>

#include <winsock2.h>
#include <tspi.h>


class tapi_line 
{

public:
	tapi_line(DWORD devid, HTAPILINE tline, LINEEVENT ev, std::string uri) 
	{ 
		this->deviceid = devid; 
		this->line = tline; 
		this->eventproc = ev;
		this->subscribe_uri = uri;
	}

	std::string get_subscribed_uri(void) { return this->subscribe_uri; }
	LINEEVENT get_line_event(void) { return this->eventproc; }
	HTAPILINE get_tapi_line(void) { return this->line; }
	DWORD get_deviceid (void) { return this->deviceid; }

private:

	DWORD deviceid;
	HTAPILINE line;
	LINEEVENT eventproc;
	std::string subscribe_uri;

	// our HDRVLINE is simply th epointer to this object once instantiated.

};


typedef std::list<tapi_line *> tapi_lines_list;


HDRVLINE add_line_entry(DWORD dwDeviceID,
					HTAPILINE htLine,
					LINEEVENT pfnEventProc,
					std::string uri);
void remove_line_entry (HDRVLINE hdLine);
tapi_line * get_line_entry (HDRVLINE hdLine);
bool is_already_subscribed(std::string uri);
tapi_lines_list get_line_list(void);


#endif


