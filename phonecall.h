/*
    PhoneCall.cpp, part of bv-tapi a SIP-TAPI bridge.
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


#pragma once

#ifndef PHONE_CALL
#define PHONE_CALL

#include <string>
#include <list>

class tapi_info
{
public:
	tapi_info(HTAPILINE ourLine, HTAPICALL tapiCall, LINEEVENT lineEvent)
	{
		this->ourLine = ourLine;
		this->tapiCall = tapiCall;
		this->lineEvent = lineEvent;
	}

	LINEEVENT get_line_event(void){ return this->lineEvent; }
	HTAPILINE get_line(void) { return this->ourLine; }
	HTAPICALL get_call(void) { return this->tapiCall; }

private:
	// Variables required for TSPI
	HTAPILINE ourLine;
	// TAPI's ID to the call
	HTAPICALL tapiCall;
	
	// The line event to call back
	LINEEVENT lineEvent;

};


typedef std::list<tapi_info> tapi_info_list;

class PhoneCall
{
public:
	PhoneCall(void);
	void init(std::string CallID, std::string identity);
	void PhoneCall::init(std::string CallID, std::string identity, HTAPICALL call);
	~PhoneCall(void);

	void UpdateState(std::string identity, 
						   std::string dialog_id, 
						   std::string direction, 
						   std::string state, 
						   std::string remote_display, 
						   std::string remote_identity);

	void setCalledID(std::string calledID) { this->calledID = calledID; }
	void setCallID(std::string callID) { this->CallID = callID; }

	// What we currently support - maybe more to come...
	std::string getCallerID(void) { return this->callerID; }
	std::string getCallerIDName(void) { return this->callerName; }

	std::string getCalledID(void) { return this->calledID; }
	std::string getCalledIDName(void) { return ""; }

	std::string getConnectedID(void) { return ""; }
	std::string getConnectedIDName(void) { return ""; }

	inline int getUID(void) { return this->uid; }

	void markAsRefered(std::string identity, std::string calledID, HTAPICALL hcall);
	bool isRefered(void);

	DWORD getOrigin(void);
	DWORD getCurrentTapiState(void) { return this->CurrentTapiState; }
	SYSTEMTIME GetCurrentStateTime(void) { return this->CurrentStateTime;}

	void lineEvent(DWORD, DWORD_PTR, DWORD_PTR, DWORD_PTR);

	// Send signals to TAPI
	void signalTapiNewCall(void);
	void signalTapiDialtone(void);
	void signalTapiDialing(void);
	void signalTapiProceeding(void);
	void signalTapiOffering(void);
	void signalTapiRinging(void);
	void signalTapiConnected(void);
	void signalTapiIdle(void);

	// Updates the state to TAPI
	void signalTapiCallerID(void);
	void signalTapiCalledID(void);

	time_t getCallAge(void);
	inline std::string getCallID(void) { return this->CallID; }


private:

	tapi_info_list tapi;

	std::string identity;
	std::string callerID;
	std::string callerName;
	std::string calledID;

	int ringCount;
	std::string lastState;

	time_t callCreation;

	// This is the unique ID of the call according to our server.
	std::string CallID;

	bool refered;

	bool newcallSignalled;
	bool dialtoneSignalled;
	bool connectedSignalled;

	bool dialingSignaled;
	bool proceedingSignaled;

	bool offeringSignaled;
	bool ringingSignaled;
	bool idleSignaled;

	// unique id to convert to HDRV...
	int uid;

	DWORD Origin;
	DWORD CurrentTapiState;

	SYSTEMTIME CurrentStateTime;

};




// Global functions
bool OriginateCall(std::string CalledID, HTAPICALL htCall, LPHDRVCALL tspiID);
void UpdateCallState(std::string CallID, std::string State, std::string CalledID, std::string CallerID, std::string CallerIDName);
void CheckForTimeouts(void);
PhoneCall* GetPhoneCall(HDRVCALL tspiCall);
void RemovePhoneCall(PhoneCall *ourCall);

void FiniPhoneCalls(void);


#endif


