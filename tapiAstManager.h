/*
  Name: tapiastmanager.cpp
  Copyright: Under the Mozilla Public License Version 1.1 or later
  Author: Nick Knight
  Date: 19/04/04 15:00
  Description: ties the astmanager connection to the extra information required for TAPI
*/
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

#pragma once
#include "astmanager.h"
#include "asttspglue.h"
#include <tapi.h>
#include <tspi.h>
#include <string>
#include <map>

#ifndef TAPIASTMANAGER
#define TAPIASTMANAGER

class tapiAstManager :
	public astManager
{
public:
	tapiAstManager(void);
	~tapiAstManager(void);

	DWORD setTapiLine(HTAPILINE line);
	HTAPILINE getTapiLine(void);
	DWORD getNumCalls(void);

	DWORD processMessages(void);
	DWORD setLineEvent(LINEEVENT callBack);
	astTspGlue * findCall(HDRVCALL tspCallID);
	DWORD dropCall(HDRVCALL tspiID);

	DWORD originate(std::string destination, HTAPICALL call, DRV_REQUESTID requestID);
	void setAsyncComp(ASYNC_COMPLETION pfnCompletionProc);
	void setThreadHandle(HANDLE thHandl);

	//V0.0.7.0
	void setSimpleDial(BOOL blSimpleDial);
	BOOL getSimpleDial(void);
	void tapiAstManager::tapiSendCompletion(DWORD dwRequest, DWORD dwCompletion);
	//V0.0.7.0

private:

	HTAPILINE htLine;
	typedef std::map<std::string, astTspGlue> mapCall;
	mapCall trackCalls;
	LINEEVENT lineEvent;

	//Listening thread
	HANDLE listThread;

	Mutex tspMut;

	//This is used to store any awaiting
	astTspGlue originated;
	DRV_REQUESTID tapiRequest;
	ASYNC_COMPLETION tapiCompletionProc;

protected:
	virtual DWORD handleResponce(bool sucsess, std::string message, std::string uniqueid, DWORD actionid);

	//V0.0.7.0
	BOOL SimpleDial;
	//V0.0.7.0

};


#endif