/*
    bv-tapi.cpp, part of bv-tapi a SIP-TAPI bridge.
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

// The debugger can't handle symbols more than 255 characters long.
// STL often creates symbols longer than that.
// When symbols are longer than 255 characters, the warning is issued.
#pragma warning(disable:4786 4244 4996)

//#define TAPI_CURRENT_VERSION 0x00020002
#include <winsock2.h>
#include <tspi.h>

#include "dll.h"
#include <windows.h>

#include <stdio.h>

#include <wchar.h>
#include <time.h>

//our resources
#include "resource.h"

//misc functions
#include "utilities.h"
#include "config.h"
#include "presence-tapi-bridge.h"

#include "sofia-bridge.h"
#include "tapi_lines.h"

////////////////////////////////////////////////////////////////////////////////
//
// Globals
//
// Keep track of all the globals we require
//
////////////////////////////////////////////////////////////////////////////////

//Function call for asyncrounous functions
ASYNC_COMPLETION g_pfnCompletionProc = 0;


DWORD g_dwPermanentProviderID = 0;
DWORD g_dwLineDeviceIDBase = 0;
DWORD isInit = 0;

HPROVIDER g_hProvider = 0;

//For our windows...
HINSTANCE g_hinst = 0;


Mutex *g_CallMut;


// { dwDialPause, dwDialSpeed, dwDigitDuration, dwWaitForDialtone }
LINEDIALPARAMS      g_dpMin = { 100,  50, 100,  100 };
LINEDIALPARAMS      g_dpDef = { 250,  50, 250,  500 };
LINEDIALPARAMS      g_dpMax = { 1000, 50, 1000, 1000 };


////////////////////////////////////////////////////////////////////////////////
// Function DllMain
//
// Dll entry
//
////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HINSTANCE   hinst,
					DWORD       dwReason,
					void*      /*pReserved*/)
{
	if( dwReason == DLL_PROCESS_ATTACH )
	{
		g_hinst = hinst;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerInit
//
// Startup.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerInit(
							   DWORD               dwTSPIVersion,
							   DWORD               dwPermanentProviderID,
							   DWORD               dwLineDeviceIDBase,
							   DWORD               dwPhoneDeviceIDBase,
							   DWORD_PTR           dwNumLines,
							   DWORD_PTR           dwNumPhones,
							   ASYNC_COMPLETION    lpfnCompletionProc,
							   LPDWORD             lpdwTSPIOptions                         // TSPI v2.0
							   )

{

	// We shouldn't be called twice.
	if ( isInit == 1 )
	{
		return 0;
	}
	isInit = 1;

	g_CallMut = new Mutex("TAPI mutex");

	loadConfig();

	//Record all of the globals we need
	g_pfnCompletionProc = lpfnCompletionProc;

	//other params we need to track
	g_dwPermanentProviderID = dwPermanentProviderID;
	g_dwLineDeviceIDBase = dwLineDeviceIDBase;

	strstrmap ourlines = getLines();
	dwNumLines = ourlines.size();
	dwNumPhones = 0;

	bv_logger("TSPI_providerInit with %u number of lines and 0 phones\n", ourlines.size());


	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerShutdown
//
// Shutdown and clean up.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerShutdown(
								   DWORD               dwTSPIVersion,
								   DWORD               dwPermanentProviderID                   // TSPI v2.0
								   )
{

	bv_logger("TSPI_providerShutdown: Shutting down\n");

	sofia_fini();
	isInit = 0;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Capabilities
//
// TAPI will ask us what our capabilities are
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineNegotiateTSPIVersion
//
// 
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineNegotiateTSPIVersion(
	DWORD dwDeviceID,
	DWORD dwLowVersion,
	DWORD dwHighVersion,
	LPDWORD lpdwTSPIVersion)
{


	LONG tr = 0;

	if ( dwLowVersion <= TAPI_CURRENT_VERSION )
	{
#define MIN(a, b) (a < b ? a : b)
		*lpdwTSPIVersion = MIN(TAPI_CURRENT_VERSION,dwHighVersion);
	}
	else
	{
		//bv_logger("TSPI_lineNegotiateTSPIVersion: low version too low\n");
		tr = LINEERR_INCOMPATIBLEAPIVERSION;
	}

	//bv_logger("TSPI_lineNegotiateTSPIVersion: %x:%x? %x\n", TAPI_CURRENT_VERSION, dwHighVersion, *lpdwTSPIVersion);

	return tr;

}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerEnumDevices
//
// 
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerEnumDevices(
									  DWORD dwPermanentProviderID,
									  LPDWORD lpdwNumLines,
									  LPDWORD lpdwNumPhones,
									  HPROVIDER hProvider,
									  LINEEVENT lpfnLineCreateProc,
									  PHONEEVENT lpfnPhoneCreateProc)
{
	
	g_hProvider = hProvider;

	strstrmap ourlines = getLines();
	
	*lpdwNumLines = ourlines.size();
	*lpdwNumPhones = 0;

	bv_logger("TSPI_providerEnumDevices: %u lines, 0 phones\n", *lpdwNumLines);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetDevCaps
//
// Allows TAPI to check our line capabilities before placing a call
//
// dwDeviceID is 0 => dwDeviceID-1
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetDevCaps(
								 DWORD           dwDeviceID,
								 DWORD           dwTSPIVersion,
								 DWORD           dwExtVersion,
								 LPLINEDEVCAPS   pldc
								 )
{

	bv_logger("TSPI_lineGetDevCaps: device id %u\n", dwDeviceID - g_dwLineDeviceIDBase);

	LONG tr = 0;
	const wchar_t szProviderInfo[] = L"babblevoice Service Provider";
	const wchar_t szLineName[] = L"babblevoice: ";

	strstrpair actualline = getLineByIndex(dwDeviceID - g_dwLineDeviceIDBase);

	bv_logger("TSPI_lineGetDevCaps: trying to open %s\n", actualline.first.c_str());

	wchar_t linename[200];
	size_t numconverted;
	mbstowcs_s(&numconverted, linename,200, actualline.first.c_str(),200);

	pldc->dwNeededSize = sizeof(LINEDEVCAPS) +
							sizeof(szProviderInfo) +
							sizeof(szLineName) +
							(numconverted * sizeof(wchar_t)) + 
							sizeof(wchar_t);

	wchar_t *pszLineName;
	bv_logger("TSPI_lineGetDevCaps: need %u got %u\n", pldc->dwNeededSize, pldc->dwTotalSize);
	if ( pldc->dwNeededSize <= pldc->dwTotalSize )
	{
		pldc->dwUsedSize = pldc->dwNeededSize;

		pldc->dwProviderInfoSize = sizeof(szProviderInfo);
		pldc->dwProviderInfoOffset = sizeof(LINEDEVCAPS);

		wchar_t *pszProviderInfo = (wchar_t*)((BYTE*)pldc +
										pldc->dwProviderInfoOffset);

		std::size_t len = wcslen(szProviderInfo) + 1;
		wcscpy_s(pszProviderInfo, len, szProviderInfo);

		pldc->dwLineNameOffset = sizeof(LINEDEVCAPS) +
									sizeof(szProviderInfo);

		pldc->dwLineNameSize = sizeof(szLineName)
								+ ( actualline.first.size() * sizeof(wchar_t) )
								+ sizeof(wchar_t);

		pszLineName = (wchar_t*)((BYTE*)pldc + 
									pldc->dwLineNameOffset);

bv_logger("total size for buffer is %u, %u\n", pldc->dwLineNameSize, numconverted);
		memset((void*)pszLineName, 0, pldc->dwLineNameSize);

		len = wcslen(szLineName) + 1;
		wcscpy_s(pszLineName, len, szLineName);
		wcscpy_s(pszLineName + len - 1, numconverted, linename);

	}
	else
	{
		pldc->dwUsedSize = sizeof(LINEDEVCAPS);
	}

	pldc->dwStringFormat      = STRINGFORMAT_ASCII;//STRINGFORMAT_UNICODE;

	// Microsoft recommended algorithm for
	// calculating the permanent line ID
#define MAKEPERMLINEID(dwPermProviderID, dwDeviceID) \
	((LOWORD(dwPermProviderID) << 16) | dwDeviceID)

	pldc->dwPermanentLineID   = MAKEPERMLINEID(g_dwPermanentProviderID, dwDeviceID - g_dwLineDeviceIDBase);
	//pldc->dwAddressModes      = LINEADDRESSMODE_DIALABLEADDR;
	pldc->dwAddressModes      = LINEADDRESSMODE_ADDRESSID;
	pldc->dwNumAddresses      = 1;
	pldc->dwBearerModes       = LINEBEARERMODE_VOICE;
	pldc->dwMediaModes        = LINEMEDIAMODE_INTERACTIVEVOICE;
	pldc->dwGenerateDigitModes= LINEDIGITMODE_DTMF;
	pldc->dwDevCapFlags       = LINEDEVCAPFLAGS_CLOSEDROP;
	pldc->dwMaxNumActiveCalls = 1;
	pldc->dwLineFeatures      = LINEFEATURE_MAKECALL;
	//pldc->dwMonitorDigitModes = LINEDIGITMODE_DTMF;
	pldc->dwMedCtlMediaMaxListSize = 1;
	pldc->dwMedCtlToneMaxListSize = 1;
	pldc->dwMedCtlCallStateMaxListSize = 1;
	

	// DialParams
	pldc->MinDialParams = g_dpMin;
	pldc->MaxDialParams = g_dpMax;
	pldc->DefaultDialParams = g_dpDef;

	return tr;
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetAddressCaps
//
// Allows TAPI to check our line capabilities before placing a call
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetAddressCaps(
									 DWORD   dwDeviceID,
									 DWORD   dwAddressID,
									 DWORD   dwTSPIVersion,
									 DWORD   dwExtVersion,
									 LPLINEADDRESSCAPS  pac)
{

	/* TODO (Nick#1#): Most of this function has been taken from an example
	and will need to be modified in more detail */

	//pac->dwNeededSize = sizeof(LPLINEADDRESSCAPS);
	//pac->dwUsedSize = sizeof(LPLINEADDRESSCAPS);
	pac->dwNeededSize = sizeof(LINEADDRESSCAPS);
	pac->dwUsedSize = sizeof(LINEADDRESSCAPS);


	pac->dwLineDeviceID = dwDeviceID;
	pac->dwAddressSharing = LINEADDRESSSHARING_PRIVATE;
	pac->dwCallInfoStates = LINECALLINFOSTATE_MEDIAMODE | LINECALLINFOSTATE_APPSPECIFIC;
	pac->dwCallerIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN;
	pac->dwCalledIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN;
	pac->dwRedirectionIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN ;
	pac->dwCallStates = LINECALLSTATE_IDLE | LINECALLSTATE_OFFERING | LINECALLSTATE_ACCEPTED | LINECALLSTATE_DIALING | LINECALLSTATE_CONNECTED;

	pac->dwDialToneModes = LINEDIALTONEMODE_UNAVAIL;
	pac->dwBusyModes = LINEDIALTONEMODE_UNAVAIL;
	pac->dwSpecialInfo = LINESPECIALINFO_UNAVAIL;

	pac->dwDisconnectModes = LINEDISCONNECTMODE_UNAVAIL;

	/* TODO (Nick#1#): This needs to be taken from the UI */
	pac->dwMaxNumActiveCalls = 1;

	pac->dwAddrCapFlags = LINEADDRCAPFLAGS_DIALED | LINEADDRCAPFLAGS_ACCEPTTOALERT;

	pac->dwCallFeatures = LINECALLFEATURE_DIAL | LINECALLFEATURE_DROP | LINECALLFEATURE_GENERATEDIGITS;
	pac->dwAddressFeatures = LINEADDRFEATURE_MAKECALL | LINEADDRFEATURE_PICKUP;

	bv_logger("TSPI_lineGetDevCaps: \n");

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Lines
//
// After a suitable line has been found it will be opened with lineOpen
// which TAPI will forward onto TSPI_lineOpen
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineOpen
//
// This function is typically called where the software needs to reserve some 
// hardware, you can assign any 32bit value to the *phdLine, and it will be sent
// back to any future calls to functions about that line.
//
// Becuase this is sockets not hardware we can set-up the sockets required in this 
// functions, and perhaps get the thread going to read the output of the manager.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineOpen(
						   DWORD               dwDeviceID,
						   HTAPILINE           htLine,
						   LPHDRVLINE          phdLine,
						   DWORD               dwTSPIVersion,
						   LINEEVENT           pfnEventProc
						   )
{

	bv_logger("TSPI_lineOpen: device id %u\n", dwDeviceID);

	if ( get_line_list().size() == 0 )
	{
		srand(time(0));
		int port = rand() % 30000 + 1024 ;

		char buffer[30];
		_itoa (port,buffer,10);

		std::string user = getuserfromsipuri(getConfigValue("reg_url"));

		std::string url = std::string("sip:") + user + std::string("@0.0.0.0:") + buffer;
		sofia_init(url);


		run_registrations(getConfigValue("reg_url"), getConfigValue("reg_secret"));
	}

	strstrpair actualline = getLineByIndex(dwDeviceID - g_dwLineDeviceIDBase);


	if ( !is_already_subscribed(actualline.second) )
	{
		subscribe(actualline.second);
	}

	// record our line info.
	*phdLine = add_line_entry(dwDeviceID - g_dwLineDeviceIDBase, htLine, pfnEventProc, actualline.second );
	bv_logger("TSPI_lineOpen: HDRVLINE 0x%x\n", *phdLine);

	//return 0 - signal success
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineClose
//
// Called by TAPI when a line is no longer required.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineClose(HDRVLINE hdLine)
{
	tapi_line *line = get_line_entry(hdLine);

	if ( NULL != line)
	{
		unsubscribe(line->get_subscribed_uri());
		remove_line_entry(hdLine);

		if ( get_line_list().size() == 0 )
		{
			sofia_fini();
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineMakeCall
//
// This function is called by TAPI to initialize a new outgoing call. This will
// initiate the call and return, when the call is made (but not necasarily connected)
// we should then signal TAPI via the asyncrounous completion function.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineMakeCall(
							   // The request ID
							   DRV_REQUESTID       dwRequestID,
							   // the handle to the line which the call should be placed
							   HDRVLINE            hdLine,
							   // TAPI's handle to the call - we must save it to send up with any future requests					
							   HTAPICALL           htCall,
							   // Pointer to our handle of the call which we must populate
							   LPHDRVCALL          phdCall,
							   // The number we want to call
							   LPCWSTR             pszDestAddress,
							   DWORD               dwCountryCode,
							   LPLINECALLPARAMS    const pCallParams
							   )
{

	bv_logger("TSPI_lineMakeCall: request id %u\n", dwRequestID);

	///////////////////////////////////////////////////////////////////
	// Convert the numberinto a usable format
	///////////////////////////////////////////////////////////////////
	char charString[250];
	if( *pszDestAddress == L'T' || *pszDestAddress == L'P' )
	{
		pszDestAddress++;
	}

	size_t pReturnValue;
	mbstate_t mbstate;

	// Reset to initial shift state
	::memset((void*)&mbstate, 0, sizeof(mbstate));

	wcsrtombs_s(&pReturnValue, &charString[0], 100, &pszDestAddress, 100, &mbstate);

	tapi_line *ourline;
	bv_logger("TSPI_lineMakeCall: HDRVLINE 0x%x\n", hdLine);
	ourline = get_line_entry(hdLine);
	if ( NULL == ourline )
	{
		bv_logger("TSPI_lineMakeCall: no line with reference 0x%x\n", hdLine);
		return LINEERR_OPERATIONFAILED;
	}

	bv_logger("TSPI_lineMakeCall: line opened, looking for info on %u\n", ourline->get_deviceid());
	strstrpair lineinfo = getLineByIndex(ourline->get_deviceid());
	

	std::string url = lineinfo.second;
	std::string dom = getdomainfromsipuri(url);
	std::string domwithat;
	if ( dom != "" )
	{
		domwithat = std::string("@") + dom;
	}

	std::string tourl(std::string("sip:") + std::string(charString) + domwithat);
	bv_logger("TSPI_lineMakeCall: making call - %s %s\n", tourl.c_str(), url.c_str());
	HDRVCALL callid;
	if ( 0 == ( callid = call( tourl, url, dwRequestID, htCall )))
	{
		bv_logger("TSPI_lineMakeCall: failed to make call\n");
		return LINEERR_OPERATIONFAILED;
	}

	bv_logger("TSPI_lineMakeCall: returning (HDRVCALL) 0x%x\n", callid);
	*phdCall = callid;
	///////////////////////////////////////////////////////////////////
	// We now return - our server should send back us our new states.
	///////////////////////////////////////////////////////////////////

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineDrop
//
// This function is called by TAPI to signal the end of a call. The status 
// information for the call should be retained until the function TSPI_lineCloseCall
// is called.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineDrop(
						   DRV_REQUESTID       dwRequestID,
						   HDRVCALL            hdCall,
						   LPCSTR              lpsUserInfo,
						   DWORD               dwSize
						   )
{

	//g_CallMut->Lock();
	if ( 0 != g_pfnCompletionProc ) g_pfnCompletionProc(dwRequestID, 0L);
	//g_CallMut->Unlock();

	//DropCall(ServerUrl, UserName, Password, hdCall);

	return dwRequestID;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineCloseCall
//
// This function should deallocate all of the calls resources, TSPI_lineDrop 
// may not be called before this one - so we also have to check the call 
// is dropped as well.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineCloseCall(
								HDRVCALL            hdCall
								)
{
	//DropCall(ServerUrl, UserName, Password, hdCall);

	return 0;
}



////////////////////////////////////////////////////////////////////////////////
//
// Status
//
// if TAPI requires to find out the status of our lines then it can call
// the following functions.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetLineDevStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetLineDevStatus(
									   HDRVLINE            hdLine,
									   LPLINEDEVSTATUS     plds
									   )
{
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetAdressStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPI_lineGetAdressStatus(
							  HDRVLINE hdLine,
							  DWORD dwAddressID,
							  LPLINEADDRESSSTATUS pas)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallStatus(
									HDRVCALL            hdCall,
									LPLINECALLSTATUS    pls
									)
{

	// TODO
	PhoneCall ourCall = find_phonecall_by_tapi_call_id(hdCall);

	pls->dwNeededSize = sizeof(LINECALLSTATUS);
	pls->dwUsedSize = sizeof(LINECALLSTATUS);

	SafeMutex locker(g_CallMut, "TSPI_lineGetCallStatus");
		
	pls->dwCallState = ourCall.getCurrentTapiState();
	pls->tStateEntryTime = ourCall.GetCurrentStateTime();
	locker.Unlock();
	
	pls->dwCallStateMode = 0;

	/*LINECALLFEATURE_ACCEPT | LINECALLFEATURE_ANSWER | LINECALLFEATURE_COMPLETECALL | */
	pls->dwCallFeatures = LINECALLFEATURE_DIAL | LINECALLFEATURE_DROP | LINECALLFEATURE_HOLD | LINECALLFEATURE_PARK | LINECALLFEATURE_REDIRECT | LINECALLFEATURE_UNHOLD;
	//and more...

	pls->dwDevSpecificSize  = 0;
	pls->dwDevSpecificOffset  = 0;
	pls->dwCallFeatures2  = LINECALLFEATURE2_PARKNONDIRECT;


	return 0;
}

std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}


//Required (maybe) by lineGetCallInfo
//Thanks to the poster!
//http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&threadm=114501c32a84%24a337f930%24a101280a%40phx.gbl&rnum=3&prev=/groups%3Fq%3DTSPI_lineGetCallInfo%26ie%3DUTF-8%26oe%3DUTF-8%26hl%3Den%26btnG%3DGoogle%2BSearch
void TackOnData(void* pData, const char* pStr, DWORD* pSize)
{

	// Convert the string to wide str
	LPCWSTR pWStr;
	std::wstring stemp;

	try
	{
		stemp = s2ws(pStr);
		pWStr = stemp.c_str();
	}
	catch(...)
	{
	}

	DWORD cbStr = (DWORD)(strlen(pStr) + 1) * 2;
	LPLINECALLINFO pDH = (LPLINECALLINFO)pData;

	// If this isn't an empty string then tack it on
	if (cbStr > 2)
	{
		// Increase the needed size to reflect this string whether we are
		// successful or not.
		pDH->dwNeededSize += cbStr;

		// Do we have space to tack on the string?
		if (pDH->dwTotalSize >= pDH->dwUsedSize + cbStr)
		{
			bv_logger("Room to insert...%s\r\n", pStr);
			// YES, tack it on
			memcpy((char *)pDH + pDH->dwUsedSize, pWStr, cbStr);

			// Now adjust size and offset in message and used 
			// size in the header
			DWORD* pOffset = pSize + 1;

			*pSize   = cbStr;
			*pOffset = pDH->dwUsedSize;
			pDH->dwUsedSize += cbStr;
		}
		else
		{
			bv_logger("No room at the inn...%s\r\n", pStr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallInfo
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallInfo(
								  HDRVCALL            hdCall,
								  LPLINECALLINFO      lpCallInfo
								  )
{

	//Some useful information taken from a newsgroup cutting I found - it's ok for
	//the SP to fill in as much info as it can, set dwNeededSize apprpriately, &
	//return success.  (The app might not care about the var-length fields;
	//otherwise it'll see dwNeededSized, realloc a bigger buf, and retry)

	//These are the items that a TSP is required to fill out - other items we must preserve
	// - that is they are used by TAPI.
	
	bv_logger("TSPI_lineGetCallInfo: got (HDRVCALL) 0x%x\n", hdCall);
	//calculate size needed
	
	//DWORD extraspace = 0;

	std::string callerID;
	std::string callerName;
	std::string calledID;
	std::string calledIDName;
	std::string connectedID;
	std::string connectedIDName;

	std::string callID;
	//std::size_t totalSizeRequired = 0;
	DWORD Origin;

	lpCallInfo->dwUsedSize = sizeof(LINECALLINFO);
	lpCallInfo->dwNeededSize = sizeof(LINECALLINFO);

	SafeMutex locker(g_CallMut, "TSPI_lineGetCallInfo");
	PhoneCall ourCall = find_phonecall_by_tapi_call_id(hdCall);
	// 
	callID = ourCall.getCallID();
	bv_logger("TSPI_lineGetCallInfo: Found call with call ID %s\n", callID.c_str());

	bv_logger("0\n");
	callerID = ourCall.getCallerID();
	bv_logger("1\n");
	callerName = ourCall.getCallerIDName();
	bv_logger("2\n");
	calledID = ourCall.getCalledID();
	bv_logger("3\n");
	calledIDName = ourCall.getCalledIDName();
	bv_logger("4\n");
	connectedID = ourCall.getConnectedID();
	bv_logger("5\n");
	connectedIDName = ourCall.getConnectedIDName();
	bv_logger("6\n");
	Origin = ourCall.getOrigin();
	bv_logger("7\n");
	locker.Unlock();
	bv_logger("8\n");

	bv_logger("TSPI_lineGetCallInfo: Caller ID is %s\r\n", callerID.c_str());
	

	lpCallInfo->dwLineDeviceID = 0;  
	lpCallInfo->dwAddressID = 0;
	lpCallInfo->dwBearerMode = LINEBEARERMODE_SPEECH;
	//lpCallInfo->dwRate;
	//lpCallInfo->dwMediaMode;
	//lpCallInfo->dwAppSpecific;
	//lpCallInfo->dwCallID;
	//lpCallInfo->dwRelatedCallID;
	//lpCallInfo->dwCallParamFlags;
	//lpCallInfo->DialParams;
	lpCallInfo->dwOrigin = Origin;
	lpCallInfo->dwReason = LINECALLREASON_DIRECT;
	lpCallInfo->dwCompletionID = 0;
	lpCallInfo->dwCountryCode = 0;   // 0 = unknown
	lpCallInfo->dwTrunk = 0xFFFFFFFF; // 0xFFFFFFFF = unknown

	// For the next section we need to find our call relating to this.
	lpCallInfo->dwCallerIDOffset = 0;
	lpCallInfo->dwCallerIDSize = 0;
	lpCallInfo->dwCallerIDFlags = 0;

	lpCallInfo->dwCallerIDNameOffset = 0;
	lpCallInfo->dwCallerIDNameSize = 0;

	lpCallInfo->dwCalledIDOffset = 0;
	lpCallInfo->dwCalledIDSize = 0;
	lpCallInfo->dwCalledIDFlags = 0;

	lpCallInfo->dwCalledIDNameOffset = 0;
	lpCallInfo->dwCalledIDNameSize = 0;

	lpCallInfo->dwConnectedIDSize = 0;
	lpCallInfo->dwConnectedIDOffset = 0;
	lpCallInfo->dwConnectedIDFlags = 0;

	lpCallInfo->dwConnectedIDNameSize = 0;
	lpCallInfo->dwConnectedIDNameOffset = 0;

	lpCallInfo->dwRedirectionIDFlags = 0;
	lpCallInfo->dwRedirectionIDSize = 0;
	lpCallInfo->dwRedirectionIDOffset = 0;
	lpCallInfo->dwRedirectionIDNameSize = 0;
	lpCallInfo->dwRedirectionIDNameOffset = 0;
	lpCallInfo->dwRedirectingIDFlags = 0;
	lpCallInfo->dwRedirectingIDSize = 0;
	lpCallInfo->dwRedirectingIDOffset = 0;
	lpCallInfo->dwRedirectingIDNameSize = 0;
	lpCallInfo->dwRedirectingIDNameOffset = 0;
	lpCallInfo->dwDisplaySize = 0;
	lpCallInfo->dwDisplayOffset = 0;
	lpCallInfo->dwUserUserInfoSize = 0;
	lpCallInfo->dwUserUserInfoOffset = 0;
	lpCallInfo->dwHighLevelCompSize = 0;
	lpCallInfo->dwHighLevelCompOffset = 0;
	lpCallInfo->dwLowLevelCompSize = 0;
	lpCallInfo->dwLowLevelCompOffset = 0;
	lpCallInfo->dwChargingInfoSize = 0;
	lpCallInfo->dwChargingInfoOffset = 0;
	lpCallInfo->dwTerminalModesSize = 0;
	lpCallInfo->dwTerminalModesOffset = 0;
	lpCallInfo->dwDevSpecificSize = 0;
	lpCallInfo->dwDevSpecificOffset = 0;

	if ( callerID != "" )
	{
		lpCallInfo->dwCallerIDOffset = lpCallInfo->dwUsedSize; //This is where we place the caller ID info
		//we can also call the following function on caller ID name etc
		TackOnData(lpCallInfo, getuserfromsipuri(callerID).c_str() ,&lpCallInfo->dwCallerIDSize);
		lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_ADDRESS;
	}
	
	if ( callerName != "" )
	{
		bv_logger("Inserting callerName\n");
		lpCallInfo->dwCallerIDNameOffset = lpCallInfo->dwUsedSize;
		TackOnData(lpCallInfo, callerName.c_str() ,&lpCallInfo->dwCallerIDNameSize);
		lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_NAME; 
	}

	if ( calledID != "" )
	{
		bv_logger("Inserting calledID: %s\n", getuserfromsipuri(calledID).c_str());
		lpCallInfo->dwCalledIDOffset = lpCallInfo->dwUsedSize;
		TackOnData(lpCallInfo, getuserfromsipuri(calledID).c_str() ,&lpCallInfo->dwCalledIDSize);
		lpCallInfo->dwCalledIDFlags |= LINECALLPARTYID_ADDRESS;
	}


	if ( calledIDName != "" )
	{
		bv_logger("Inserting calledIDName: %s\n", calledIDName.c_str());
		lpCallInfo->dwCalledIDNameOffset = lpCallInfo->dwUsedSize;
		TackOnData(lpCallInfo, calledIDName.c_str() ,&lpCallInfo->dwCalledIDNameSize);
		lpCallInfo->dwCalledIDFlags |= LINECALLPARTYID_NAME;
	}

	if ( connectedID != "" )
	{
		bv_logger("Inserting connectedID: %s\n", getuserfromsipuri(calledIDName).c_str());
		lpCallInfo->dwCalledIDOffset = lpCallInfo->dwUsedSize;
		TackOnData(lpCallInfo, getuserfromsipuri(connectedID).c_str() ,&lpCallInfo->dwCalledIDSize);
		lpCallInfo->dwConnectedIDFlags |= LINECALLPARTYID_ADDRESS;
	}


	if ( connectedIDName != "" )
	{
		bv_logger("Inserting calledIDName: %s\n", calledIDName.c_str());
		lpCallInfo->dwConnectedIDNameOffset = lpCallInfo->dwUsedSize;
		TackOnData(lpCallInfo, connectedIDName.c_str() ,&lpCallInfo->dwConnectedIDNameSize);
		lpCallInfo->dwConnectedIDFlags |= LINECALLPARTYID_NAME;
	}

	if ( callID != "" )
	{
		bv_logger("Inserting callID: %s\n", callID.c_str());
		lpCallInfo->dwDevSpecificOffset = lpCallInfo->dwUsedSize;
		TackOnData(lpCallInfo, callID.c_str() ,&lpCallInfo->dwDevSpecificSize);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallAddressID
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallAddressID(
									   HDRVCALL            hdCall,
									   LPDWORD             pdwAddressID
									   )
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Installation, removal and configuration
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerInstall
//
// Called by TAPI on installation, ideal oppertunity to gather information
// via. some form of user interface.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerInstall(
									TUISPIDLLCALLBACK pfnUIDLLCallback,
									HWND hwndOwner,
									DWORD dwPermanentProviderID)
{

	std::string strData;

	MessageBox(hwndOwner, "babblevoice Service Provider installed",
		"Startup", MB_OK);

	return 0;
}

LONG
TSPIAPI
TSPI_providerInstall(
					 HWND                hwndOwner,
					 DWORD               dwPermanentProviderID
					 )
{
	// Although this func is never called by TAPI v2.0, we export
	// it so that the Telephony Control Panel Applet knows that it
	// can add this provider via lineAddProvider(), otherwise
	// Telephon.cpl will not consider it installable

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerRemove
//
// When TAPI is requested to remove the TSP via the call lineRemoveProvider.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerRemove(
								   TUISPIDLLCALLBACK pfnUIDLLCallback,
								   HWND hwndOwner,
								   DWORD dwPermanentProviderID)
{
	return 0;
}
LONG TSPIAPI TSPI_providerRemove(
								 HWND    hwndOwner,
								 DWORD   dwPermanentProviderID)
{
	// Although this func is never called by TAPI v2.0, we export
	// it so that the Telephony Control Panel Applet knows that it
	// can remove this provider via lineRemoveProvider(), otherwise
	// Telephon.cpl will not consider it removable


	return 0;
}
////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerRemove
//
// When TAPI is requested to remove the TSP via the call lineRemoveProvider.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerUIIdentify(
									   TUISPIDLLCALLBACK pfnUIDLLCallback,
									   LPWSTR pszUIDLLName)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Dialog config area
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Function ConfigDlgProc
//
// The callback function for our line dialog box.
//
////////////////////////////////////////////////////////////////////////////////
static char szItemName[80];
static HWND parentConfigDlg;
BOOL CALLBACK ConfigLineDlgProc(
							HWND    hwnd,
							UINT    nMsg,
							WPARAM  wparam,
							LPARAM  lparam)
{

	BOOL    b = FALSE;
	std::string temp, tempb;
	strstrmap::iterator it;
	strstrmap linelist;


	switch( nMsg )
	{
	case WM_INITDIALOG:

		temp = "Some Description";
		SetDlgItemText(hwnd, IDC_LINE_NAME, temp.c_str());
		temp = "sip:user@your.babblevoice.com";
		SetDlgItemText(hwnd, IDC_LINE_URL, temp.c_str());

		b = TRUE;

	case WM_COMMAND:
		switch( wparam )
		{
		case ID_LINEADD_OK:

			GetDlgItemText(hwnd, IDC_LINE_NAME, szItemName, sizeof(szItemName));
			temp = szItemName;
			GetDlgItemText(hwnd, IDC_LINE_URL, szItemName, sizeof(szItemName));
			tempb = szItemName;
			addLine(temp, tempb);
			EndDialog(hwnd, IDCANCEL);

			
			SendMessage(GetDlgItem(parentConfigDlg, IDC_LINELIST), (UINT)LB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
			linelist = getLines();
			for ( it = linelist.begin(); it != linelist.end(); it++ )
			{
				SendMessage(GetDlgItem(parentConfigDlg, IDC_LINELIST), (UINT)LB_ADDSTRING,(WPARAM)0,(LPARAM)(*it).first.c_str());
			}

			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;

		}

	}

	return b;
}

////////////////////////////////////////////////////////////////////////////////
// Function ConfigDlgProc
//
// The callback function for our dialog box.
//
////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK ConfigDlgProc(
							HWND    hwnd,
							UINT    nMsg,
							WPARAM  wparam,
							LPARAM  lparam)
{
	BOOL    b = FALSE;
	std::string temp;
	strstrmap linelist;
	strstrmap::iterator it;

	UINT listviewindex;
	bool linesupdated;
	char buf[300];

	switch( nMsg )
	{
	case WM_INITDIALOG:

		loadConfig();
		temp = getConfigValue("reg_url");

		if ( "" == temp )
		{
			temp = "sip:user@your.babblevoice.com";
		}

		SetDlgItemText(hwnd, IDC_USER, temp.c_str());

		temp = getConfigValue("reg_secret");
		SetDlgItemText(hwnd, IDC_PASS, temp.c_str());

		temp = getConfigValue("debug_file");
		SetDlgItemText(hwnd, IDC_DEBUG_FILE, temp.c_str());
		

		linelist = getLines();
		for ( it = linelist.begin(); it != linelist.end(); it++ )
		{
			SendMessage(GetDlgItem(hwnd, IDC_LINELIST), (UINT)LB_ADDSTRING,(WPARAM)0,(LPARAM)(*it).first.c_str());
		}

		b = TRUE;
		break;

	case WM_COMMAND:
		switch( wparam )
		{

		case IDC_ADDLINE:
			parentConfigDlg = hwnd;
			DialogBox(g_hinst,
				MAKEINTRESOURCE(IDD_DIALOG2),
				hwnd,
				ConfigLineDlgProc);
			break;

		case IDC_REMOVELINE:

			linelist = getLines();
			linesupdated = false;
			listviewindex = SendMessage(GetDlgItem(hwnd, IDC_LINELIST), (UINT)LB_GETCURSEL,(WPARAM)0,(LPARAM)0);

			if ( listviewindex != LB_ERR )
			{
				SendMessage(GetDlgItem(hwnd, IDC_LINELIST), (UINT)LB_GETTEXT ,(WPARAM)listviewindex,(LPARAM)&buf[0]);
				removeLine(&buf[0]);
				linesupdated = true;
				
			}

			if ( true == linesupdated )
			{
				SendMessage(GetDlgItem(hwnd, IDC_LINELIST), (UINT)LB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
				linelist = getLines();
				for ( it = linelist.begin(); it != linelist.end(); it++ )
				{
					SendMessage(GetDlgItem(hwnd, IDC_LINELIST), (UINT)LB_ADDSTRING,(WPARAM)0,(LPARAM)(*it).first.c_str());
				}
			}

			break;

		case IDOK:
			GetDlgItemText(hwnd, IDC_USER, &szItemName[0], sizeof(szItemName));
			setConfigValue("reg_url", szItemName);

			GetDlgItemText(hwnd, IDC_PASS, &szItemName[0], sizeof(szItemName));
			setConfigValue("reg_secret", szItemName);

			GetDlgItemText(hwnd, IDC_DEBUG_FILE, &szItemName[0], sizeof(szItemName));
			setConfigValue("debug_file", szItemName);

			GetDlgItemText(hwnd, IDC_PROXY, &szItemName[0], sizeof(szItemName));
			setProxyServer("proxy_address", szItemName);

			saveConfig();
			EndDialog(hwnd, IDOK);

			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;

		}
		break;
	}

	return b;
}

////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_lineConfigDialog
//
// Called when a request for config is made upon us.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_lineConfigDialog(                                        // TSPI v2.0
									 TUISPIDLLCALLBACK   lpfnUIDLLCallback,
									 DWORD               dwDeviceID,
									 HWND                hwndOwner,
									 LPCWSTR             lpszDeviceClass
									 )
{


	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG1),
		hwndOwner,
		ConfigDlgProc);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerUIIdentify
//
// Becuase the UI part of a TSP can exsist in another DLL we need to tell TAPI
// that this is the DLL which provides the UI functionality as well.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerUIIdentify(                                        // TSPI v2.0
									 LPWSTR              lpszUIDLLName
									 )
{
	char szPath[MAX_PATH+1];
	GetModuleFileNameA(g_hinst,szPath,MAX_PATH);
	std::size_t numConverted;
	mbstowcs_s(&numConverted, lpszUIDLLName, strlen(szPath) * sizeof(wchar_t), szPath, MAX_PATH * sizeof(wchar_t) );

	return 0;
}

LONG
TSPIAPI
TSPI_providerGenericDialogData(                                 // TSPI v2.0
							   DWORD_PTR           dwObjectID,
							   DWORD               dwObjectType,
							   LPVOID              lpParams,
							   DWORD               dwSize
							   )
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerConfig
//
// Obsolete - only in TAPI version 1.4 and below
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerConfig(
								   TUISPIDLLCALLBACK pfnUIDLLCallback,
								   HWND hwndOwner,
								   DWORD dwPermanentProviderID)
{

	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG1),
		hwndOwner,
		ConfigDlgProc);

	return 0;
}

///////////////////Start////////////////////
LONG
TSPIAPI
TSPI_lineAccept(
				DRV_REQUESTID       dwRequestID,
				HDRVCALL            hdCall,
				LPCSTR              lpsUserUserInfo,
				DWORD               dwSize
				)
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineAddToConference(
						 DRV_REQUESTID       dwRequestID,
						 HDRVCALL            hdConfCall,
						 HDRVCALL            hdConsultCall
						 )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineAnswer(
				DRV_REQUESTID       dwRequestID,
				HDRVCALL            hdCall,
				LPCSTR              lpsUserUserInfo,
				DWORD               dwSize
				)
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineBlindTransfer(
					   DRV_REQUESTID       dwRequestID,
					   HDRVCALL            hdCall,
					   LPCWSTR             lpszDestAddress,
					   DWORD               dwCountryCode)
{
	return 0;
}





LONG
TSPIAPI
TSPI_lineCompleteCall(
					  DRV_REQUESTID       dwRequestID,
					  HDRVCALL            hdCall,
					  LPDWORD             lpdwCompletionID,
					  DWORD               dwCompletionMode,
					  DWORD               dwMessageID
					  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineCompleteTransfer(
						  DRV_REQUESTID       dwRequestID,
						  HDRVCALL            hdCall,
						  HDRVCALL            hdConsultCall,
						  HTAPICALL           htConfCall,
						  LPHDRVCALL          lphdConfCall,
						  DWORD               dwTransferMode
						  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineConditionalMediaDetection(
								   HDRVLINE            hdLine,
								   DWORD               dwMediaModes,
								   LPLINECALLPARAMS    const lpCallParams
								   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineDevSpecific(
					 DRV_REQUESTID       dwRequestID,
					 HDRVLINE            hdLine,
					 DWORD               dwAddressID,
					 HDRVCALL            hdCall,
					 LPVOID              lpParams,
					 DWORD               dwSize
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineDevSpecificFeature(
							DRV_REQUESTID       dwRequestID,
							HDRVLINE            hdLine,
							DWORD               dwFeature,
							LPVOID              lpParams,
							DWORD               dwSize
							)
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineDial(
			  DRV_REQUESTID       dwRequestID,
			  HDRVCALL            hdCall,
			  LPCWSTR             lpszDestAddress,
			  DWORD               dwCountryCode
			  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineDropOnClose(                                           // TSPI v1.4
					 HDRVCALL            hdCall
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineDropNoOwner(                                           // TSPI v1.4
					 HDRVCALL            hdCall
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineForward(
				 DRV_REQUESTID       dwRequestID,
				 HDRVLINE            hdLine,
				 DWORD               bAllAddresses,
				 DWORD               dwAddressID,
				 LPLINEFORWARDLIST   const lpForwardList,
				 DWORD               dwNumRingsNoAnswer,
				 HTAPICALL           htConsultCall,
				 LPHDRVCALL          lphdConsultCall,
				 LPLINECALLPARAMS    const lpCallParams
				 )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineGatherDigits(
					  HDRVCALL            hdCall,
					  DWORD               dwEndToEndID,
					  DWORD               dwDigitModes,
					  LPWSTR              lpsDigits,
					  DWORD               dwNumDigits,
					  LPCWSTR             lpszTerminationDigits,
					  DWORD               dwFirstDigitTimeout,
					  DWORD               dwInterDigitTimeout
					  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineGenerateDigits(
						HDRVCALL            hdCall,
						DWORD               dwEndToEndID,
						DWORD               dwDigitMode,
						LPCWSTR             lpszDigits,
						DWORD               dwDuration
						)
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineGenerateTone(
					  HDRVCALL            hdCall,
					  DWORD               dwEndToEndID,
					  DWORD               dwToneMode,
					  DWORD               dwDuration,
					  DWORD               dwNumTones,
					  LPLINEGENERATETONE  const lpTones
					  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineGetAddressID(
					  HDRVLINE            hdLine,
					  LPDWORD             lpdwAddressID,
					  DWORD               dwAddressMode,
					  LPCWSTR             lpsAddress,
					  DWORD               dwSize
					  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineGetAddressStatus(
						  HDRVLINE            hdLine,
						  DWORD               dwAddressID,
						  LPLINEADDRESSSTATUS lpAddressStatus
						  )
{
	//This function can be expanded when we impliment
	//parking, transfer etc

	lpAddressStatus->dwTotalSize = sizeof(LINEADDRESSSTATUS);  
	lpAddressStatus->dwNeededSize = sizeof(LINEADDRESSSTATUS); 


	return 0;
}

LONG
TSPIAPI
TSPI_lineGetDevConfig(
					  DWORD               dwDeviceID,
					  LPVARSTRING         lpDeviceConfig,
					  LPCWSTR             lpszDeviceClass
					  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineGetExtensionID(
						DWORD               dwDeviceID,
						DWORD               dwTSPIVersion,
						LPLINEEXTENSIONID   lpExtensionID
						)
{
	
	lpExtensionID->dwExtensionID0 = 0;
	lpExtensionID->dwExtensionID1 = 0;
	lpExtensionID->dwExtensionID2 = 0;
	lpExtensionID->dwExtensionID3 = 0;

	return 0;
}


LONG
TSPIAPI
TSPI_lineGetIcon(
				 DWORD               dwDeviceID,
				 LPCWSTR             lpszDeviceClass,
				 LPHICON             lphIcon
				 )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineGetID(
			   HDRVLINE            hdLine,
			   DWORD               dwAddressID,
			   HDRVCALL            hdCall,
			   DWORD               dwSelect,
			   LPVARSTRING         lpDeviceID,
			   LPCWSTR             lpszDeviceClass,
			   HANDLE              hTargetProcess                          // TSPI v2.0
			   )
{
	return LINEERR_NODEVICE;
}



LONG
TSPIAPI
TSPI_lineGetNumAddressIDs(
						  HDRVLINE            hdLine,
						  LPDWORD             lpdwNumAddressIDs
						  )
{
	*lpdwNumAddressIDs = 1;

	return 0;
}

LONG
TSPIAPI
TSPI_lineHold(
			  DRV_REQUESTID       dwRequestID,
			  HDRVCALL            hdCall
			  )
{
	return 0;
}



LONG TSPIAPI TSPI_lineMonitorDigits(
					   HDRVCALL            hdCall,
					   DWORD               dwDigitModes
					   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineMonitorMedia(
					  HDRVCALL            hdCall,
					  DWORD               dwMediaModes
					  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineMonitorTones(
					  HDRVCALL            hdCall,
					  DWORD               dwToneListID,
					  LPLINEMONITORTONE   const lpToneList,
					  DWORD               dwNumEntries
					  )
{
	return 0;
}

LONG TSPIAPI TSPI_lineNegotiateExtVersion(
							 DWORD               dwDeviceID,
							 DWORD               dwTSPIVersion,
							 DWORD               dwLowVersion,
							 DWORD               dwHighVersion,
							 LPDWORD             lpdwExtVersion
							 )
{

	LONG tr = 0;


	if ( dwLowVersion <= TAPI_CURRENT_VERSION )
	{
		*lpdwExtVersion = MIN(TAPI_CURRENT_VERSION, dwHighVersion);
	}
	else
	{
		tr = LINEERR_INCOMPATIBLEAPIVERSION;
	}

	return 0;
}



LONG
TSPIAPI
TSPI_linePark(
			  DRV_REQUESTID       dwRequestID,
			  HDRVCALL            hdCall,
			  DWORD               dwParkMode,
			  LPCWSTR             lpszDirAddress,
			  LPVARSTRING         lpNonDirAddress
			  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_linePickup(
				DRV_REQUESTID       dwRequestID,
				HDRVLINE            hdLine,
				DWORD               dwAddressID,
				HTAPICALL           htCall,
				LPHDRVCALL          lphdCall,
				LPCWSTR             lpszDestAddress,
				LPCWSTR             lpszGroupID
				)
{
	return 0;
}

LONG
TSPIAPI
TSPI_linePrepareAddToConference(
								DRV_REQUESTID       dwRequestID,
								HDRVCALL            hdConfCall,
								HTAPICALL           htConsultCall,
								LPHDRVCALL          lphdConsultCall,
								LPLINECALLPARAMS    const lpCallParams
								)
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineRedirect(
				  DRV_REQUESTID       dwRequestID,
				  HDRVCALL            hdCall,
				  LPCWSTR             lpszDestAddress,
				  DWORD               dwCountryCode
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineReleaseUserUserInfo(                                   // TSPI v1.4
							 DRV_REQUESTID       dwRequestID,
							 HDRVCALL            hdCall
							 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineRemoveFromConference(
							  DRV_REQUESTID       dwRequestID,
							  HDRVCALL            hdCall
							  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSecureCall(
					DRV_REQUESTID       dwRequestID,
					HDRVCALL            hdCall
					)
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSelectExtVersion(
						  HDRVLINE            hdLine,
						  DWORD               dwExtVersion
						  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSendUserUserInfo(
						  DRV_REQUESTID       dwRequestID,
						  HDRVCALL            hdCall,
						  LPCSTR              lpsUserUserInfo,
						  DWORD               dwSize
						  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetAppSpecific(
						HDRVCALL            hdCall,
						DWORD               dwAppSpecific
						)
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineSetCallData(                                           // TSPI v2.0
					 DRV_REQUESTID       dwRequestID,
					 HDRVCALL            hdCall,
					 LPVOID              lpCallData,
					 DWORD               dwSize
					 )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineSetCallParams(
					   DRV_REQUESTID       dwRequestID,
					   HDRVCALL            hdCall,
					   DWORD               dwBearerMode,
					   DWORD               dwMinRate,
					   DWORD               dwMaxRate,
					   LPLINEDIALPARAMS    const lpDialParams
					   )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineSetCallQualityOfService(                               // TSPI v2.0
								 DRV_REQUESTID       dwRequestID,
								 HDRVCALL            hdCall,
								 LPVOID              lpSendingFlowspec,
								 DWORD               dwSendingFlowspecSize,
								 LPVOID              lpReceivingFlowspec,
								 DWORD               dwReceivingFlowspecSize
								 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetCallTreatment(                                      // TSPI v2.0
						  DRV_REQUESTID       dwRequestID,
						  HDRVCALL            hdCall,
						  DWORD               dwTreatment
						  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineSetCurrentLocation(                                    // TSPI v1.4
							DWORD               dwLocation
							)
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetDefaultMediaDetection(
								  HDRVLINE            hdLine,
								  DWORD               dwMediaModes
								  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineSetDevConfig(
					  DWORD               dwDeviceID,
					  LPVOID              const lpDeviceConfig,
					  DWORD               dwSize,
					  LPCWSTR             lpszDeviceClass
					  )
{
	return 0;
}



LONG
TSPIAPI
TSPI_lineSetLineDevStatus(                                      // TSPI v2.0
						  DRV_REQUESTID       dwRequestID,
						  HDRVLINE            hdLine,
						  DWORD               dwStatusToChange,
						  DWORD               fStatus
						  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineSetMediaControl(
						 HDRVLINE                    hdLine,
						 DWORD                       dwAddressID,
						 HDRVCALL                    hdCall,
						 DWORD                       dwSelect,
						 LPLINEMEDIACONTROLDIGIT     const lpDigitList,
						 DWORD                       dwDigitNumEntries,
						 LPLINEMEDIACONTROLMEDIA     const lpMediaList,
						 DWORD                       dwMediaNumEntries,
						 LPLINEMEDIACONTROLTONE      const lpToneList,
						 DWORD                       dwToneNumEntries,
						 LPLINEMEDIACONTROLCALLSTATE const lpCallStateList,
						 DWORD                       dwCallStateNumEntries
						 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetMediaMode(
					  HDRVCALL            hdCall,
					  DWORD               dwMediaMode
					  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetStatusMessages(
						   HDRVLINE            hdLine,
						   DWORD               dwLineStates,
						   DWORD               dwAddressStates
						   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetTerminal(
					 DRV_REQUESTID       dwRequestID,
					 HDRVLINE            hdLine,
					 DWORD               dwAddressID,
					 HDRVCALL            hdCall,
					 DWORD               dwSelect,
					 DWORD               dwTerminalModes,
					 DWORD               dwTerminalID,
					 DWORD               bEnable
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetupConference(
						 DRV_REQUESTID       dwRequestID,
						 HDRVCALL            hdCall,
						 HDRVLINE            hdLine,
						 HTAPICALL           htConfCall,
						 LPHDRVCALL          lphdConfCall,
						 HTAPICALL           htConsultCall,
						 LPHDRVCALL          lphdConsultCall,
						 DWORD               dwNumParties,
						 LPLINECALLPARAMS    const lpCallParams
						 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSetupTransfer(
					   DRV_REQUESTID       dwRequestID,
					   HDRVCALL            hdCall,
					   HTAPICALL           htConsultCall,
					   LPHDRVCALL          lphdConsultCall,
					   LPLINECALLPARAMS    const lpCallParams
					   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineSwapHold(
				  DRV_REQUESTID       dwRequestID,
				  HDRVCALL            hdActiveCall,
				  HDRVCALL            hdHeldCall
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineUncompleteCall(
						DRV_REQUESTID       dwRequestID,
						HDRVLINE            hdLine,
						DWORD               dwCompletionID
						)
{
	return 0;
}

LONG
TSPIAPI
TSPI_lineUnhold(
				DRV_REQUESTID       dwRequestID,
				HDRVCALL            hdCall
				)
{
	return 0;
}


LONG
TSPIAPI
TSPI_lineUnpark(
				DRV_REQUESTID       dwRequestID,
				HDRVLINE            hdLine,
				DWORD               dwAddressID,
				HTAPICALL           htCall,
				LPHDRVCALL          lphdCall,
				LPCWSTR             lpszDestAddress
				)
{
	return 0;
}



LONG
TSPIAPI
TSPI_phoneClose(
				HDRVPHONE           hdPhone
				)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneDevSpecific(
					  DRV_REQUESTID       dwRequestID,
					  HDRVPHONE           hdPhone,
					  LPVOID              lpParams,
					  DWORD               dwSize
					  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetButtonInfo(
						HDRVPHONE           hdPhone,
						DWORD               dwButtonLampID,
						LPPHONEBUTTONINFO   lpButtonInfo
						)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetData(
				  HDRVPHONE           hdPhone,
				  DWORD               dwDataID,
				  LPVOID              lpData,
				  DWORD               dwSize
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetDevCaps(
					 DWORD               dwDeviceID,
					 DWORD               dwTSPIVersion,
					 DWORD               dwExtVersion,
					 LPPHONECAPS         lpPhoneCaps
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetDisplay(
					 HDRVPHONE           hdPhone,
					 LPVARSTRING         lpDisplay
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetExtensionID(
						 DWORD               dwDeviceID,
						 DWORD               dwTSPIVersion,
						 LPPHONEEXTENSIONID  lpExtensionID
						 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetGain(
				  HDRVPHONE           hdPhone,
				  DWORD               dwHookSwitchDev,
				  LPDWORD             lpdwGain
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetHookSwitch(
						HDRVPHONE           hdPhone,
						LPDWORD             lpdwHookSwitchDevs
						)
{
	return 0;
}


LONG
TSPIAPI
TSPI_phoneGetIcon(
				  DWORD               dwDeviceID,
				  LPCWSTR             lpszDeviceClass,
				  LPHICON             lphIcon
				  )
{
	return 0;
}



LONG
TSPIAPI
TSPI_phoneGetID(
				HDRVPHONE           hdPhone,
				LPVARSTRING         lpDeviceID,
				LPCWSTR             lpszDeviceClass,
				HANDLE              hTargetProcess                          // TSPI v2.0
				)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetLamp(
				  HDRVPHONE           hdPhone,
				  DWORD               dwButtonLampID,
				  LPDWORD             lpdwLampMode
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetRing(
				  HDRVPHONE           hdPhone,
				  LPDWORD             lpdwRingMode,
				  LPDWORD             lpdwVolume
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetStatus(
					HDRVPHONE           hdPhone,
					LPPHONESTATUS       lpPhoneStatus
					)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneGetVolume(
					HDRVPHONE           hdPhone,
					DWORD               dwHookSwitchDev,
					LPDWORD             lpdwVolume
					)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneNegotiateExtVersion(
							  DWORD               dwDeviceID,
							  DWORD               dwTSPIVersion,
							  DWORD               dwLowVersion,
							  DWORD               dwHighVersion,
							  LPDWORD             lpdwExtVersion
							  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneNegotiateTSPIVersion(
							   DWORD               dwDeviceID,
							   DWORD               dwLowVersion,
							   DWORD               dwHighVersion,
							   LPDWORD             lpdwTSPIVersion
							   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneOpen(
			   DWORD               dwDeviceID,
			   HTAPIPHONE          htPhone,
			   LPHDRVPHONE         lphdPhone,
			   DWORD               dwTSPIVersion,
			   PHONEEVENT          lpfnEventProc
			   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSelectExtVersion(
						   HDRVPHONE           hdPhone,
						   DWORD               dwExtVersion
						   )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetButtonInfo(
						DRV_REQUESTID       dwRequestID,
						HDRVPHONE           hdPhone,
						DWORD               dwButtonLampID,
						LPPHONEBUTTONINFO   const lpButtonInfo
						)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetData(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwDataID,
				  LPVOID              const lpData,
				  DWORD               dwSize
				  )
{
	return 0;
}


LONG
TSPIAPI
TSPI_phoneSetDisplay(
					 DRV_REQUESTID       dwRequestID,
					 HDRVPHONE           hdPhone,
					 DWORD               dwRow,
					 DWORD               dwColumn,
					 LPCWSTR             lpsDisplay,
					 DWORD               dwSize
					 )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetGain(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwHookSwitchDev,
				  DWORD               dwGain
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetHookSwitch(
						DRV_REQUESTID       dwRequestID,
						HDRVPHONE           hdPhone,
						DWORD               dwHookSwitchDevs,
						DWORD               dwHookSwitchMode
						)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetLamp(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwButtonLampID,
				  DWORD               dwLampMode
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetRing(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwRingMode,
				  DWORD               dwVolume
				  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetStatusMessages(
							HDRVPHONE           hdPhone,
							DWORD               dwPhoneStates,
							DWORD               dwButtonModes,
							DWORD               dwButtonStates
							)
{
	return 0;
}

LONG
TSPIAPI
TSPI_phoneSetVolume(
					DRV_REQUESTID       dwRequestID,
					HDRVPHONE           hdPhone,
					DWORD               dwHookSwitchDev,
					DWORD               dwVolume
					)
{
	return 0;
}



LONG
TSPIAPI
TSPI_providerConfig(
					HWND                hwndOwner,
					DWORD               dwPermanentProviderID
					)
{   //Tapi version 1.4 and earlier - now can be ignored.
	return 0;
}

LONG
TSPIAPI
TSPI_providerCreateLineDevice(                                  // TSPI v1.4
							  DWORD_PTR           dwTempID,
							  DWORD               dwDeviceID
							  )
{
	return 0;
}

LONG
TSPIAPI
TSPI_providerCreatePhoneDevice(                                 // TSPI v1.4
							   DWORD_PTR           dwTempID,
							   DWORD               dwDeviceID
							   )
{
	return 0;
}



LONG
TSPIAPI
TSPI_providerFreeDialogInstance(                                // TSPI v2.0
								HDRVDIALOGINSTANCE  hdDlgInst
								)
{
	return 0;
}


LONG
TSPIAPI
TUISPI_lineConfigDialogEdit(                                    // TSPI v2.0
							TUISPIDLLCALLBACK   lpfnUIDLLCallback,
							DWORD               dwDeviceID,
							HWND                hwndOwner,
							LPCWSTR             lpszDeviceClass,
							LPVOID              const lpDeviceConfigIn,
							DWORD               dwSize,
							LPVARSTRING         lpDeviceConfigOut
							)
{
	return 0;
}

LONG
TSPIAPI
TUISPI_phoneConfigDialog(                                       // TSPI v2.0
						 TUISPIDLLCALLBACK   lpfnUIDLLCallback,
						 DWORD               dwDeviceID,
						 HWND                hwndOwner,
						 LPCWSTR             lpszDeviceClass
						 )
{
	return 0;
}


LONG
TSPIAPI
TUISPI_providerGenericDialog(                                   // TSPI v2.0
							 TUISPIDLLCALLBACK   lpfnUIDLLCallback,
							 HTAPIDIALOGINSTANCE htDlgInst,
							 LPVOID              lpParams,
							 DWORD               dwSize,
							 HANDLE              hEvent
							 )
{
	return 0;
}

LONG
TSPIAPI
TUISPI_providerGenericDialogData(                               // TSPI v2.0
								 HTAPIDIALOGINSTANCE htDlgInst,
								 LPVOID              lpParams,
								 DWORD               dwSize
								 )
{
	return 0;
}




