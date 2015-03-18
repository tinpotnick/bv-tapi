/*
    sofia-bridge.cpp, part of bv-tapi a SIP-TAPI bridge.
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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "sofia-bridge.h"

#include <string>

#include "presence-tapi-bridge.h"
#include "config.h"
#include "utilities.h"

#pragma warning (disable:4996)

#define AGENT_STRING "bv-0.15"

/* Application context structure */
static application appl[1] = {{{{(sizeof appl)}}}};

HANDLE startupEvent;

extern ASYNC_COMPLETION g_pfnCompletionProc;

static std::string auth_reg_user;
static std::string auth_reg_secret;
static std::string auth_reg_url;
static std::string contact_url_host;

static std::string localurl;

static bool logMutexinited = false;
static CRITICAL_SECTION logMut;

extern Mutex *g_CallMut;


////////////////////////////////////////////////////////////////////////////////
// Function app_r_register
//
// Called in responce to a register
//
////////////////////////////////////////////////////////////////////////////////
void app_r_register(int			 status, 
					char const   *phrase, 
					nua_t        *nua, 
					nua_magic_t  *magic, 
					nua_handle_t *nh, 
					nua_hmagic_t *hmagic, 
					sip_t const  *sip, 
					tagi_t        tags[])
{
	switch ( status )
	{

	case 200:

		contact_url_host = sip->sip_contact->m_url->url_host;
		SetEvent(startupEvent);
		break;
	case 401:
	case 407:

		char auth[256];
		if ( 407 == status )
		{
			sprintf_s(auth, "%s:%s:%s:%s", 
				sip->sip_proxy_authenticate->au_scheme, 
				msg_params_find(sip->sip_proxy_authenticate->au_params, "realm="),
				auth_reg_user.c_str(), 
				auth_reg_secret.c_str());

		}
		else
		{
			sprintf_s(auth, "%s:%s:%s:%s", 
				sip->sip_www_authenticate->au_scheme, 
				msg_params_find(sip->sip_www_authenticate->au_params, "realm="),
				auth_reg_user.c_str(), 
				auth_reg_secret.c_str());
		}

		// we need to add authenitcation
		nua_authenticate(nh,
			NUTAG_AUTH(auth),
			TAG_END()
			);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function app_r_subscribe
//
// Called in responce to a subscribe
//
////////////////////////////////////////////////////////////////////////////////
void app_r_subscribe(int			 status, 
					 char const   *phrase, 
					 nua_t        *nua, 
					 nua_magic_t  *magic, 
					 nua_handle_t *nh, 
					 nua_hmagic_t *hmagic, 
					 sip_t const  *sip, 
					 tagi_t        tags[])
{

}

////////////////////////////////////////////////////////////////////////////////
// Function app_i_notify
//
// Action i_notify events - i.e. when a change happens to the watched resource
//
////////////////////////////////////////////////////////////////////////////////
void app_i_notify(int			 status, 
					 char const   *phrase, 
					 nua_t        *nua, 
					 nua_magic_t  *magic, 
					 nua_handle_t *nh, 
					 nua_hmagic_t *hmagic, 
					 sip_t const  *sip, 
					 tagi_t        tags[])
{

	bv_logger("app_i_notify\n");

	/*
	Example XML we are processing here (dialog presence)
	http://www.rfc-editor.org/rfc/rfc4235.txt

	<?xml version="1.0"?>
		<dialog-info xmlns="urn:ietf:params:xml:ns:dialog-info" version="7" state="partial" entity="sip:1001@omniis.babblevoice.com">
		<dialog id="5e9e04a0-a199-11e0-a661-495a2db86d6d" direction="initiator">
		<state>confirmed</state>
		<local>
			<identity display="1001">sip:1001@omniis.babblevoice.com</identity>
			<target uri="sip:1001@omniis.babblevoice.com">
				<param pname="+sip.rendering" pvalue="yes"/>
			</target>
		</local>
		<remote>
			<identity display="3">sip:3@omniis.babblevoice.com</identity>
			<target uri="sip:**1001@omniis.babblevoice.com"/>
		</remote>
		</dialog>
		</dialog-info>

	*/

	boost::property_tree::ptree pt;

	std::string dialog_info_entity;
	std::string dialog_id;
	std::string direction;
	std::string state;
	std::string remote_display;
	std::string remote_identity;
	
	if ( NULL == sip->sip_payload ) return;
	bv_logger("Got presence XML: %s", sip->sip_payload->pl_data);
	std::stringstream xml_buffer(sip->sip_payload->pl_data);

	try
	{
		read_xml(xml_buffer, pt);
		
		dialog_info_entity = pt.get<std::string>("dialog-info.<xmlattr>.entity");
		dialog_id = pt.get<std::string>("dialog-info.dialog.<xmlattr>.id");
		direction = pt.get<std::string>("dialog-info.dialog.<xmlattr>.direction");
		state = pt.get<std::string>("dialog-info.dialog.state");
		remote_display = pt.get("dialog-info.dialog.remote.identity.<xmlattr>.display", "");
		remote_identity = pt.get("dialog-info.dialog.remote.identity", "");

		bv_logger("dialog_info_entity: %s\n", dialog_info_entity.c_str());
		bv_logger("dialog_id: %s\n", dialog_id.c_str());
		bv_logger("direction: %s\n", direction.c_str());
		bv_logger("state: %s\n", state.c_str());
		bv_logger("remote_display: %s\n", remote_display.c_str());
		bv_logger("remote_identity_str: %s\n", remote_identity.c_str());

		SafeMutex locker(g_CallMut, "app_i_notify");

		handle_presence_event(dialog_info_entity,
								dialog_id,
								direction,
								state,
								remote_display,
								remote_identity
								);
		locker.Unlock();
	}
	catch(...) { bv_logger("Exception whilst trying to parse XML and handle event\n"); }
}


////////////////////////////////////////////////////////////////////////////////
// Function app_r_invite
//
// We have sent an invite and this is a responce to it. We work with until 
// we have sent a refer to to teh phone actually handling the call.
//
////////////////////////////////////////////////////////////////////////////////
void app_r_invite(int			 status, 
					 char const   *phrase, 
					 nua_t        *nua, 
					 nua_magic_t  *magic, 
					 nua_handle_t *nh, 
					 nua_hmagic_t *hmagic, 
					 sip_t const  *sip, 
					 tagi_t        tags[])
{
	operation *op;
	op = (operation *)hmagic;

	switch ( status )
	{
	case 183:

		break;
	case 200:
		// we are responcable for sending out an ack...
		nua_ack(nh, TAG_END());
		bv_logger("app_r_invite: 200 received\n");


		// This is needed or the refer gets sent out too quickly and freeswitch
		// at the very least does not like that.
		// I hate sleeps to do this type of thing, is there a better way of doing this anyone? :)
		::Sleep(1000);
		bv_logger("app_r_invite: inital call setup complete calling 0x%x for request id %u\n", g_pfnCompletionProc, op->requestid);
		if ( 0 != g_pfnCompletionProc ) g_pfnCompletionProc(op->requestid, 0L);

		// now we send a refer...
		nua_refer(nh, 
			SIPTAG_REFER_TO_STR(op->to.c_str() ),
			TAG_END());
		break;
	case 401:
	case 407:

		char auth[256];

		if ( 407 == status )
		{
			sprintf_s(auth, "%s:%s:%s:%s", 
				sip->sip_proxy_authenticate->au_scheme, 
				msg_params_find(sip->sip_proxy_authenticate->au_params, "realm="),
				auth_reg_user.c_str(), 
				auth_reg_secret.c_str());

		}
		else
		{
			sprintf_s(auth, "%s:%s:%s:%s", 
				sip->sip_www_authenticate->au_scheme, 
				msg_params_find(sip->sip_www_authenticate->au_params, "realm="),
				auth_reg_user.c_str(), 
				auth_reg_secret.c_str());
		}

		// we need to add authenitcation
		nua_authenticate(nh,
			NUTAG_AUTH(auth),
			TAG_END()
			);
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////
// Function app_r_refer
//
// We have sent a refer and this is a responce to it. Look for new call id?
//
////////////////////////////////////////////////////////////////////////////////
void app_r_refer(int			 status, 
					 char const   *phrase, 
					 nua_t        *nua, 
					 nua_magic_t  *magic, 
					 nua_handle_t *nh, 
					 nua_hmagic_t *hmagic, 
					 sip_t const  *sip, 
					 tagi_t        tags[])
{
	switch(status)
	{
	case 100:
		// trying
		break;

	case 200:
	case 202:
		// accepted
		break;
	}

}

////////////////////////////////////////////////////////////////////////////////
// Function app_i_terminated
//
// 
//
////////////////////////////////////////////////////////////////////////////////
void app_i_terminated(int			 status, 
					 char const   *phrase, 
					 nua_t        *nua, 
					 nua_magic_t  *magic, 
					 nua_handle_t *nh, 
					 nua_hmagic_t *hmagic, 
					 sip_t const  *sip, 
					 tagi_t        tags[])
{
	operation *op;
	op = (operation *)hmagic;

	bv_logger("app_i_terminated\n");
	if ( NULL != sip )
	{
		handle_presence_event(op->to.c_str(), sip->sip_call_id->i_id, "", "terminated", "", "");
	}
}


////////////////////////////////////////////////////////////////////////////////
// Function app_r_terminated
//
// 
//
////////////////////////////////////////////////////////////////////////////////
void app_r_terminated(int			 status, 
					 char const   *phrase, 
					 nua_t        *nua, 
					 nua_magic_t  *magic, 
					 nua_handle_t *nh, 
					 nua_hmagic_t *hmagic, 
					 sip_t const  *sip, 
					 tagi_t        tags[])
{
	bv_logger("app_r_terminated\n");
	
	operation *op;
	op = (operation *)hmagic;

	if ( NULL != sip )
	{
		handle_presence_event(op->to.c_str(), sip->sip_call_id->i_id, "", "terminated", "", "");
	}
}


void app_r_shutdown(int           status,
					char const   *phrase,
					nua_t        *nua,
					nua_magic_t  *magic,
					nua_handle_t *nh,
					nua_hmagic_t *hmagic,
					sip_t const  *sip,
					tagi_t        tags[])
{
	bv_logger("shutdown: %d %s\n", status, phrase);

	if (status < 200) {
		/* shutdown in progress -> return */
		return;
	}

	/* end the event loop. su_root_run() will return */
	su_root_break(magic->root);

}



////////////////////////////////////////////////////////////////////////////////
// Function app_callback
//
// Called by the sofia framework.
//
////////////////////////////////////////////////////////////////////////////////
void app_callback(nua_event_t   event,
                  int           status,
                  char const   *phrase,
                  nua_t        *nua,
                  nua_magic_t  *magic,
                  nua_handle_t *nh,
                  nua_hmagic_t *hmagic,
                  sip_t const  *sip,
                  tagi_t        tags[])
{
	bv_logger("app_callback\n");
	switch (event) {
  case nua_r_register:
	  app_r_register(status, phrase, nua, magic, nh, hmagic, sip, tags);
  case nua_i_invite:
	  //app_i_invite(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_r_invite:
	  app_r_invite(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_r_subscribe:
	  app_r_subscribe(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_i_notify:
	  app_i_notify(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_r_refer:
		app_r_refer(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_i_terminated:
		app_i_terminated(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_r_terminate:
		app_r_terminated(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

  case nua_r_shutdown:
	  app_r_shutdown(status, phrase, nua, magic, nh, hmagic, sip, tags);
	  break;

	  /* and so on ... */

  default:
	  /* unknown event -> print out error message */
	  if (status > 100) {
		  bv_logger("unknown event %d: %03d %s\n",
			  event,
			  status,
			  phrase);
	  }
	  else {
		  printf("unknown event %d\n", event);
	  }
	  //tl_print(stdout, "", tags);
	  break;
	}
} /* app_callback */


static nua_handle_t *nua_register_handle = NULL;
////////////////////////////////////////////////////////////////////////////////
// Function subscribe
//
// Subscribe against a target
//
// to: "sip:1001@omniis.babblevoice.com"
////////////////////////////////////////////////////////////////////////////////
void subscribe(std::string to)
{
	nua_handle_t *sub_handle;

	/*auth_reg_secret
	auth_reg_user
	auth_reg_url*/

	sub_handle = nua_handle(appl->nua,
		NULL,
		//SIPTAG_TO_STR(auth_reg_url.c_str()),
		NUTAG_URL(to.c_str()),
		SIPTAG_FROM_STR(auth_reg_url.c_str()),
		SIPTAG_USER_AGENT_STR(AGENT_STRING),
		TAG_END()
		);

	if ( NULL != sub_handle )
	{
		nua_subscribe(sub_handle,
			SIPTAG_EVENT_STR("dialog"),
			SIPTAG_ACCEPT_STR("application/dialog-info+xml"),
			//SIPTAG_TO_STR(to.c_str()),
			TAG_END());
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function unsubscribe
//
// Unsubscribe against a target
//
////////////////////////////////////////////////////////////////////////////////
void unsubscribe(std::string to)
{
	if ( NULL != nua_register_handle )
	{
		nua_unsubscribe(nua_register_handle,
			SIPTAG_EVENT_STR("dialog"),
			SIPTAG_ACCEPT_STR("application/dialog-info+xml"),
			SIPTAG_TO_STR(to.c_str()),
			TAG_END());
	}
}


////////////////////////////////////////////////////////////////////////////////
// Function call
//
// call a target, url is the phone device the caller is using (i.e. the first 
// phone we call) the to is the final dest (i.e. the device we do the blind 
// transfer to)
//
////////////////////////////////////////////////////////////////////////////////
HDRVCALL call(std::string to, std::string url, DRV_REQUESTID requestid, HTAPICALL tapicall)
{
	operation *op;

	/* create operation context information */
	//op = (operation *)su_zalloc(appl->home, (sizeof *op));
	op = new operation();
	if (!op)
	{
		return false;
	}

	op->to = to;
	op->url = url;
	op->requestid = requestid;

	SafeMutex locker(g_CallMut, "call");
	int id = registerIntiatedCall(url, to, tapicall);
	bv_logger("Storing phone call for %s\n", url.c_str());
	locker.Unlock();

	op->tapicall = tapicall;

	/* create operation handle */
	op->handle = nua_handle(appl->nua, 
				(nua_hmagic_t *)op, 
				//NUTAG_URL(url.c_str()), 
				SIPTAG_TO_STR(url.c_str()),
				SIPTAG_FROM_STR(auth_reg_url.c_str()),
				SIPTAG_USER_AGENT_STR(AGENT_STRING),
				TAG_END());

	if (op->handle == NULL) 
	{
		bv_logger("cannot create operation handle\n");
		return false;
	}

	std::string sdp_string;

	sdp_string = "v=0\r\no=- 2 2 IN IP4 ";
	sdp_string += contact_url_host;
	sdp_string += "\r\ns=bv_tapi\r\nc=IN IP4 ";
	sdp_string += contact_url_host;
	sdp_string += "\r\nt=0 0\r\nm=audio 60702 RTP/AVP 0 8 101\r\na=fmtp:101 0-15\r\na=rtpmap:101 telephone-event/8000\r\na=sendrecv";

	nua_invite(op->handle,
		//SOATAG_HOLD("*"),
		//NUTAG_MEDIA_ENABLE(0),
		// Should be able to send any old rubish(well enough to get going!) as we will send through a refer as soon as setup...
		//SOATAG_USER_SDP_STR("v=0\r\no=- 2 2 IN IP4 192.168.1.4\r\ns=bv_tapi\r\nc=IN IP4 192.168.1.4\r\nt=0 0\r\nm=audio 60702 RTP/AVP 0 8 101\r\na=fmtp:101 0-15\r\na=rtpmap:101 telephone-event/8000\r\na=sendrecv"),
		SOATAG_USER_SDP_STR(sdp_string.c_str()),
		SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
		SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL),
		TAG_END());

	return (HDRVCALL)id;
}

////////////////////////////////////////////////////////////////////////////////
// Function run_registrations
//
// Register user against url with secret.
//
////////////////////////////////////////////////////////////////////////////////
void run_registrations(std::string reg_url, std::string reg_secret)
{

	std::string username = getuserfromsipuri(reg_url);
	
	auth_reg_secret = reg_secret;
	auth_reg_user = username;
	auth_reg_url = reg_url;

#ifdef SIP_TEST_PROXY
	url_string_t outbound_proxy;
	strcpy(outbound_proxy.us_str,
				SIP_TEST_PROXY);
#endif

	nua_register_handle = nua_handle(appl->nua,
		NULL,
		SIPTAG_TO_STR(reg_url.c_str()),
		SIPTAG_FROM_STR(reg_url.c_str()),
		SIPTAG_USER_AGENT_STR(AGENT_STRING),
		TAG_END()
		);

#ifdef SIP_TEST_PROXY
	nua_set_params(appl->nua, NUTAG_PROXY(&outbound_proxy), TAG_END());
#endif

	// at the very least we have to register to something.
	nua_register(nua_register_handle,
		NUTAG_M_DISPLAY(username.c_str()),
		NUTAG_M_USERNAME(username.c_str()),
		NUTAG_M_PARAMS("user=phone"),
		NUTAG_M_FEATURES("audio"),
		NUTAG_CALLEE_CAPS(0),
		NUTAG_OUTBOUND("no-options-keepalive"),
		NUTAG_OUTBOUND("no-validate"),
		NUTAG_KEEPALIVE(0),
		TAG_END());

	WaitForSingleObject(startupEvent, 30000);

}




////////////////////////////////////////////////////////////////////////////////
// Function su_logger
//
// Our function to log debugging information if required.
//
////////////////////////////////////////////////////////////////////////////////
void su_logger(void *logarg, char const *format, va_list va)
{
	if (!format) return;

	std::string debug_file = getConfigValue("debug_file");
	//std::string debug_file = "c:\\bv.txt";
	if ( debug_file == "" )
	{
		return;
	}

	if ( false == logMutexinited )
	{
		logMutexinited = true;
		/* pretty nasty way of donig this - but better than no syncronization
		and cannot find a better way to catch all at the moment.
		We risk the first one clashnig with another thread.*/
		InitializeCriticalSection(&logMut);
		
	}
	
	EnterCriticalSection(&logMut);

	time_t rawtime;
	struct tm * timeinfo;
	char buffer [80];

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	strftime (buffer,80,"%Y-%m-%d %H:%M:%S",timeinfo);



	FILE *fp = fopen(debug_file.c_str(), "a+");
	if ( fp )
	{
		char tmp[2000];
		vsprintf(&tmp[0], format, va);

		fprintf(fp, "[");
		fprintf(fp, buffer);
		fprintf(fp, "] ");
		fprintf(fp, tmp);
		fclose(fp);	
	}

	LeaveCriticalSection(&logMut);
}

////////////////////////////////////////////////////////////////////////////////
// Function bv_logger
//
// General purpose logging function to be used by all functions.
//
////////////////////////////////////////////////////////////////////////////////
void bv_logger(char const *format, ...)
{
	va_list argptr;
	va_start( argptr, format );

	su_logger(NULL, format, argptr);
}

////////////////////////////////////////////////////////////////////////////////
// Function SofiaThread
//
// Our main thread to handle sip signalling.
//
////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI SofiaThread(LPVOID lpParameter)
{
	HRESULT hr = CoInitialize(NULL);

	/* initialize system utilities */
	su_init();

	/* initialize memory handling */
	su_home_init(appl->home);

	/* initialize root object */
	appl->root = su_root_create(appl);	


	// setup logging
	//su_log_init (&our_su_log);
	su_log_set_level(NULL, 9);
	su_log_redirect(NULL, su_logger, NULL);

	if (appl->root != NULL) {
		/* create NUA stack */
		appl->nua = nua_create(appl->root,
			app_callback,
			appl,
			NUTAG_URL(localurl.c_str()),
			/* tags as necessary ...*/
			TAG_NULL());

		if (appl->nua != NULL) {
			/* set necessary parameters */
			nua_set_params(appl->nua,
				NUTAG_AUTOACK(1),
				NUTAG_OUTBOUND("use-rport"),	
				//SIPTAG_CONTACT_STR("sip:192.168.1.4:34987"),
				/* tags as necessary ... */
				TAG_NULL());
		}
	}

	if (appl->root != NULL) 
	{
		if (appl->nua != NULL) 
		{

			/* enter main loop for processing of messages */
			SetEvent(startupEvent);
			su_root_run(appl->root);

			/* destroy NUA stack */
			nua_destroy(appl->nua);
		}

		/* deinit root object */
		su_root_destroy(appl->root);
		appl->root = NULL;
	}

	/* deinitialize memory handling */
	su_home_deinit(appl->home);

	/* deinitialize system utilities */
	su_deinit();

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function sofia_fini
//
// Clean up, shutdown
////////////////////////////////////////////////////////////////////////////////
void sofia_fini(void)
{
	nua_shutdown(appl->nua);
}

////////////////////////////////////////////////////////////////////////////////
// Function sofia_init
//
// Initialze and create our worker thread.
//
// url - our local information: sip:1000@0.0.0.0:34567
////////////////////////////////////////////////////////////////////////////////
void sofia_init(std::string url)
{

	localurl = url;

	HANDLE ourThread;
	startupEvent = ::CreateEvent(NULL, // no security attributes
                                  TRUE, // manual-reset
                                  FALSE,// initially non-signaled
                                  NULL);// anonymous

	// Our background thread to service SIP signaling.
	ourThread = CreateThread(NULL,0,SofiaThread,NULL,CREATE_SUSPENDED ,NULL);
	SetThreadPriority(ourThread, THREAD_PRIORITY_BELOW_NORMAL);
	ResumeThread(ourThread);

	bv_logger("Sofia thread handle: 0x%x\n", GetCurrentThread());

	// wait for it to start up so that when the calling thread calls 
	// subscriptions etc there is a stack there to call.
	WaitForSingleObject(startupEvent, 1000000);
	ResetEvent(startupEvent);

}

////////////////////////////////////////////////////////////////////////////////
// Function DropCall
//
// Drop a call using our interface, may not be able to do this!
//
////////////////////////////////////////////////////////////////////////////////
bool DropCall(std::string Server, std::string UserName, std::string Secret, HDRVCALL hdCall)
{
	return true;
}


