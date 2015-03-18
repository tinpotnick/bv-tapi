/*
    astloader.cpp, part of bv-tapi a SIP-TAPI bridge.
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


#include "stdafx.h"
#include "astloader.h"
#include <tapi.h>
#include "plhelp.h"
#define MAX_LOADSTRING 100



int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	LPCSTR pszProvider = "bv-tapi.tsp";
	LPCSTR pszTitle = "TSP Installation";
	LPCSTR pszMessage = 0;

	//VC2K5
    //if( strcmpi(lpCmdLine, "/remove") == 0 )
    if( _strcmpi(lpCmdLine, "/remove") == 0 )
	//VC2K5
    {
        DWORD dwID;
        if( GetPermanentProviderID(pszProvider, &dwID) )
        {
            if( lineRemoveProvider(dwID, 0) == 0 )
            {
                pszMessage = "TSP removed.";
            }
            else
            {
                pszMessage = "TSP cannot be removed.";
            }
        }
        else
        {
            pszMessage = "TSP not installed.";
        }
    }
    // Add the TSP
    else
    {
	    DWORD   dwID;
		LONG retVal = lineAddProvider(pszProvider, 0, &dwID);
        switch( retVal )
        {
        case 0:
            if( MessageBox(0, "TSP successfully installed. Windows must be restarted.", pszTitle, MB_SETFOREGROUND) == IDOK )
            {
                ExitWindowsEx(EWX_REBOOT, 0);
            }
        break;

        case LINEERR_NOMULTIPLEINSTANCE:
            pszMessage = "TSP already installed.";
        break;

        default:
            pszMessage = "Unable to install TSP (1).";
        break;
        }
	}

    if( pszMessage )
    {
        MessageBox(0, pszMessage, pszTitle, MB_SETFOREGROUND);
    }

	return (int) 0;
} 



