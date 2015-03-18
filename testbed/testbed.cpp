// testbed.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "testbed.h"
#define MAX_LOADSTRING 100
#define DIAL_EXTEN L"3"

#include <tspi.h>
#pragma warning (disable:4996)

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void TSTTRACE(LPCSTR pszFormat, ...)
{
	char    buf[512];
	va_list ap;	

	va_start(ap, pszFormat);
	wvsprintf(buf, pszFormat, ap);
	OutputDebugString(buf);

	va_end(ap);
}
void CALLBACK Completion_Proc(
							  DRV_REQUESTID       dwRequestID,
							  LONG                lResult
							  )
{
	TSTTRACE("Completion_Proc(%lu,%lu) called\n", dwRequestID, lResult);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
					   HINSTANCE hPrevInstance,
					   LPTSTR    lpCmdLine,
					   int       nCmdShow)
{
	//	FreeLibrary(hDLL);

	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_TESTBED, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_TESTBED);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_TESTBED);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)IDC_TESTBED;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		50, 50, 200, 500, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
HINSTANCE hDLL = NULL;               // Handle to DLL
HDRVLINE ourLine;
HDRVCALL ourHandle;

typedef LONG (CALLBACK* LPFNDLLFUNC1)(
									  DWORD               dwTSPIVersion,
									  DWORD               dwPermanentProviderID,
									  DWORD               dwLineDeviceIDBase,
									  DWORD               dwPhoneDeviceIDBase,
									  DWORD_PTR           dwNumLines,
									  DWORD_PTR           dwNumPhones,
									  ASYNC_COMPLETION    lpfnCompletionProc,
									  LPDWORD             lpdwTSPIOptions);

typedef LONG (CALLBACK* LPFNDLLFUNC2)(
									  DWORD               dwDeviceID,
									  HTAPILINE           htLine,
									  LPHDRVLINE          phdLine,
									  DWORD               dwTSPIVersion,
									  LINEEVENT           pfnEventProc);


typedef LONG (CALLBACK* LPFNDLLFUNC3)(
									  DRV_REQUESTID       dwRequestID,
									  HDRVLINE            hdLine,
									  HTAPICALL           htCall,
									  LPHDRVCALL          phdCall,
									  LPCWSTR             pszDestAddress,
									  DWORD               dwCountryCode,
									  LPLINECALLPARAMS    const pCallParams
									  );
typedef LONG (CALLBACK* LPFNDLLFUNC4)(
									  DRV_REQUESTID       dwRequestID,
									  HDRVCALL            hdCall,
									  LPCSTR              lpsUserInfo,
									  DWORD               dwSize
									  );

typedef LONG (CALLBACK* LPFNDLLFUNC5)(
									  DWORD           dwDeviceID,
									  DWORD           dwTSPIVersion,
									  DWORD           dwExtVersion,
									  LPLINEDEVCAPS   pldc
									  );

typedef LONG (CALLBACK* LPFNDLLFUNC6)(
									  LPWSTR              lpszUIDLLName
									  );

typedef LONG (CALLBACK* LPFNDLLFUNC7)(
									  HDRVLINE hdLine
									  );

typedef LONG (CALLBACK* LPFNDLLFUNC8)(
									  HDRVLINE            hdLine,
									  LPDWORD             lpdwNumAddressIDs
									  );

typedef LONG (CALLBACK* LPFNDLLFUNC9)(
									  HDRVCALL            hdCall,
									  LPLINECALLINFO      lpCallInfo
									  );

typedef BOOL (CALLBACK* LPFNDLLFUNC10)(
									   TUISPIDLLCALLBACK   lpfnUIDLLCallback,
									   DWORD               dwDeviceID,
									   HWND                hwndOwner,
									   LPCWSTR             lpszDeviceClass);


LPFNDLLFUNC1 lpfnDllFunc1;    // Function pointer
LPFNDLLFUNC2 lpfnDllFunc2;
LPFNDLLFUNC3 lpfnDllFunc3;
LPFNDLLFUNC4 lpfnDllFunc4;
LPFNDLLFUNC5 lpfnDllFunc5;
LPFNDLLFUNC6 lpfnDllFunc6;
LPFNDLLFUNC7 lpfnDllFunc7;
LPFNDLLFUNC8 lpfnDllFunc8;
LPFNDLLFUNC9 lpfnDllFunc9;
LPFNDLLFUNC10 lpfnDllFunc10;
LPFNDLLFUNC7 lpfnDllFunc11;

void loadtsplib(void)
{
	DWORD dwParam1;

	if ( NULL != hDLL ) 
	{
		return;
	}


#ifdef _DEBUG
	hDLL = LoadLibrary("../Debug/bv-tapi.tsp");
#else
	hDLL = LoadLibrary("c:\\windows\\system32\\bv-tapi.tsp");
#endif

	
	//MessageBox(0, "Loading library", "Debug", MB_SETFOREGROUND);
	if (hDLL == NULL)
	{
		DWORD err = GetLastError();
		char buf[200];
		itoa(err, buf, 10);
		MessageBox(0, buf, "Debug Load library error", MB_SETFOREGROUND);
	}
	else
	{
		MessageBox(0, "1", "Debug", MB_SETFOREGROUND);

		
		lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(hDLL, "TSPI_providerInit");				

		MessageBox(0, "2", "Debug", MB_SETFOREGROUND);

		if (!lpfnDllFunc1)
		{
			// handle the error
			FreeLibrary(hDLL);       
			return;
		}
		else
		{
			// call the init function
			lpfnDllFunc1(2, 2, 0, 0, 1, 1, Completion_Proc, &dwParam1);
		}

		lpfnDllFunc2 = (LPFNDLLFUNC2)GetProcAddress(hDLL, "TSPI_lineOpen");
		lpfnDllFunc3 = (LPFNDLLFUNC3)GetProcAddress(hDLL, "TSPI_lineMakeCall");
		lpfnDllFunc4 = (LPFNDLLFUNC4)GetProcAddress(hDLL, "TSPI_lineDrop");
		lpfnDllFunc5 = (LPFNDLLFUNC5)GetProcAddress(hDLL, "TSPI_lineGetDevCaps");
		lpfnDllFunc6 = (LPFNDLLFUNC6)GetProcAddress(hDLL, "TSPI_providerUIIdentify");
		lpfnDllFunc7 = (LPFNDLLFUNC7)GetProcAddress(hDLL, "TSPI_lineClose");
		lpfnDllFunc8 = (LPFNDLLFUNC8)GetProcAddress(hDLL, "TSPI_lineGetNumAddressIDs");
		lpfnDllFunc9 = (LPFNDLLFUNC9)GetProcAddress(hDLL, "TSPI_lineGetCallInfo");
		lpfnDllFunc10 = (LPFNDLLFUNC10)GetProcAddress(hDLL, "TUISPI_lineConfigDialog");
		lpfnDllFunc11 = (LPFNDLLFUNC7)GetProcAddress(hDLL, "TSPI_lineClose");

	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	loadtsplib();

	// all other stuff
	DWORD dwNumAddressIDs;

	LPLINEDEVCAPS tmppp;
	LPLINECALLINFO lplinecallinfo;
	char *tmpp;
	char *todelete;
	char szUIDLLName[MAX_PATH*4];

	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	DWORD neededSize;

	switch (message) 
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam); 
		wmEvent = HIWORD(wParam); 
		// Parse the menu selections:

		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			break;

		case ID_FILE_CLOSELINE:
			lpfnDllFunc11(ourLine);
			break;

		case ID_FILE_LINEGETDEVCAPS:

			tmpp = new char[ sizeof(LINEDEVCAPS) + 200 ];

			tmppp = (LPLINEDEVCAPS)tmpp;
			tmppp->dwTotalSize = 0;
			lpfnDllFunc5(0,0,0,tmppp);
			tmppp->dwTotalSize = tmppp->dwNeededSize;

			todelete = tmpp;
			neededSize = ((LPLINEDEVCAPS)todelete)->dwNeededSize;
			tmpp = new char[neededSize];


			tmppp = (LPLINEDEVCAPS)tmpp;
			tmppp->dwTotalSize = ((LPLINEDEVCAPS)todelete)->dwNeededSize;

			delete todelete;
			lpfnDllFunc5(0,0,0,tmppp);


			delete tmppp;


			lpfnDllFunc6((LPWSTR)&szUIDLLName);
			break;

		case CMD_HANGUP:


			lpfnDllFunc4(0,ourHandle,0,0);

			break;

		case CMD_VOICE:

			
			/* stress test it - 100 calls */
			//for ( int i = 0; i < 100 ; i++ )
			//{
			// Open the line - TSPI_lineOpen
			if ( NULL == ourLine )
			{
				lpfnDllFunc2(0,0,&ourLine,0,0);
			}

			//
			lpfnDllFunc8(ourLine, &dwNumAddressIDs);

			// TSPI_lineMakeCall
			lpfnDllFunc3(255,ourLine,0,&ourHandle,DIAL_EXTEN,0,0);
			// lplinecallinfo - TSPI_lineGetCallInfo
			tmpp = new char[sizeof(LINECALLINFO) + 200];
			lplinecallinfo = (LPLINECALLINFO)tmpp;
			//lpfnDllFunc9(ourHandle, lplinecallinfo);
			delete lplinecallinfo;

			Sleep(1000);
			// Dropt the call - TSPI_lineDrop
			lpfnDllFunc4(0,ourHandle,0,0);

			Sleep(1000);

			//lpfnDllFunc7(ourLine);
			//Sleep(1000);
			//}
			break;

		case CMD_OPENLINE:
			MessageBox(0, "Open line", "Debug", MB_SETFOREGROUND);

			
			lpfnDllFunc2(0, 0, &ourLine, 0, 0);

			break;

		case CMD_CONFIGURE:
			lpfnDllFunc10(NULL,
				0,
				hWnd,
				NULL);


			break;

		case IDM_EXIT:

			DestroyWindow(hWnd);
			break;

		default:

			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:

		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:

		PostQuitMessage(0);
		break;

	default:

		return DefWindowProc(hWnd, message, wParam, lParam);

	}
	return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}
