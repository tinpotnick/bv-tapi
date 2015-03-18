// plhelp.cpp: Provider List Helpers

//THanks to Windows Telephony Programming - A developers guide to TAPI by Chris Sells for this file.

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Asttapi.
 *
 * The Initial Developer of the Original Code is
 * Nick Knight.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): none
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL")
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include "plhelp.h"

// Caller responsible for freeing returned memory
LONG GetProviderList(
    LINEPROVIDERLIST**  ppd)
{
    // Load tapi32.dll explictly. This won't matter
    // for clients that already have tapi32.dll loaded,
    // but makes all the difference if they don't.
    HINSTANCE   hinstDll = 0;
    LONG        (WINAPI* pfnGetProviderList)(DWORD, LINEPROVIDERLIST*) = 0;

    if( hinstDll = LoadLibrary("tapi32.dll") )
    {
        *(FARPROC*)&pfnGetProviderList = GetProcAddress(hinstDll, "lineGetProviderList");

        if( !pfnGetProviderList )
        {
            FreeLibrary(hinstDll);
            return LINEERR_OPERATIONFAILED;
        }
    }
    else
    {
        return LINEERR_OPERATIONFAILED;
    }

    // Fill LINEPROVIDERLIST
    DWORD dwNeededSize = sizeof(LINEPROVIDERLIST);
    LONG  tr = 0;

    do
    {
        // Get some more memory if we don't have enough
        if( !*ppd || (*ppd)->dwTotalSize < dwNeededSize )
        {
            *ppd = (LINEPROVIDERLIST*)::realloc(*ppd, dwNeededSize);
            if( *ppd )
            {
                (*ppd)->dwTotalSize = dwNeededSize;
            }
            else
            {
                return LINEERR_NOMEM;
            }
        }

        // Fill in the buffer
        tr = pfnGetProviderList(TAPI_CURRENT_VERSION, *ppd);

        // Check how much memory we need
        // (some TSPs succeed even if the
        //  data size is too small)
        if( tr == LINEERR_STRUCTURETOOSMALL ||
            (tr == 0 &&
             (*ppd)->dwTotalSize < (*ppd)->dwNeededSize) )
        {
            dwNeededSize = (*ppd)->dwNeededSize;
            tr = LINEERR_STRUCTURETOOSMALL;
        }
    }
    while( tr == LINEERR_STRUCTURETOOSMALL );

    // Unload tapi32.dll (or just release our reference to it)
    FreeLibrary(hinstDll);

    return tr;
}

bool GetPermanentProviderID(LPCSTR pszProvider, DWORD* pdwID)
{
    bool    bSucceeded = false;

    LINEPROVIDERLIST*   ppl = 0;
    if( GetProviderList(&ppl) == 0 )
    {
        LPLINEPROVIDERENTRY rglpe = (LPLINEPROVIDERENTRY)((BYTE*)ppl + ppl->dwProviderListOffset);
        for( DWORD i = 0; i < ppl->dwNumProviders; i++ )
        {
            LPCSTR pszProviderFilename = (LPCSTR)((BYTE*)ppl + rglpe[i].dwProviderFilenameOffset);

			//VC2K5
            //if( strcmpi(pszProviderFilename, pszProvider) == 0 )
			if( _strcmpi(pszProviderFilename, pszProvider) == 0 )
			//VC2K5
            {
                *pdwID = rglpe[i].dwPermanentProviderID;
                bSucceeded = true;
                break;
            }
        }

        free(ppl);
    }

    return bSucceeded;
}
