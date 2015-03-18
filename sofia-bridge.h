
/*
    bv_tapi.h, part of bv-tapi a SIP-TAPI bridge.
    Copyright (C) 2011 Nick Knight

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


#ifndef SOFIA_BRIDGE
#define SOFIA_BRIDGE

/* type for application context data */
typedef struct application application;
#define NUA_MAGIC_T   application

/* type for operation context data */
typedef union oper_ctx_u oper_ctx_t;
#define NUA_HMAGIC_T  oper_ctx_t


#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/auth_module.h>
#include <sofia-sip/su_md5.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/nea.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/tport_tag.h>
#include <sofia-sip/sip_extra.h>
#include "nua_stack.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/sip_parser.h"
#include "sofia-sip/tport_tag.h"
#include <sofia-sip/msg.h>

#include <string>
#include <list>

#include <tspi.h>
#include "phonecall.h"

/* Taken from su examples - thankyou */


/* example of application context information structure */
typedef struct application
{
  su_home_t       home[1];  /* memory home */
  su_root_t      *root;     /* root object */
  nua_t          *nua;      /* NUA stack object */

  /* other data as needed ... */
} application;

/* Example of operation handle context information structure */
#if 0
typedef union operation
{
  nua_handle_t    *handle;  /* operation handle */

  struct
  {
    nua_handle_t  *handle;  /* operation handle /
    //...                     /* call-related information */
  } call;

  struct
  {
    nua_handle_t  *handle;  /* operation handle /
    //...                     /* subscription-related information */
  } subscription;

  /* other data as needed ... */

} operation;
#endif

class operation
{

public:
	nua_handle_t  *handle;  /* operation handle */
	std::string url;
	std::string to;
	PhoneCall *call;
	DRV_REQUESTID requestid;
	HTAPICALL tapicall;

};


// public interfaces
void sofia_init(std::string localurl);
void run_registrations(std::string reg_url, std::string reg_secret);
void unsubscribe(std::string to);
void subscribe(std::string to);
void sofia_fini(void);
void sofia_create_mutexes(void);

void bv_logger(char const *format, ...);
HDRVCALL call(std::string to, std::string url, DRV_REQUESTID requestid, HTAPICALL tapicall);


#endif