/*--------------------------------------------------------------
|        Copyright (C) 1997-2007 by Prodigy Design Ltd         |
|                     Sidhe Interactive (TM)                   |
|                      All rights Reserved                     |
|--------------------------------------------------------------|
|                                                              |
|  Unauthorised use or distribution of this file or any        |
|  portion of it may result in severe civil and criminal       |
|  Penalties and will be prosecuted to the full extent of the  |
|  law.                                                        |
|                                                              |
---------------------------------------------------------------*/
#include "pch.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabSystemNotificationHub.h"


const MabTime XboxOneMabSystemNotificationHub::SIGNIN_WAITING_PERIOD( 1.0f );

XboxOneMabSystemNotificationHub::XboxOneMabSystemNotificationHub( MABMEM_HEAP _heap, XboxOneMabUserInfo& _user_info )
	:	heap ( _heap ), user_info( _user_info ), notification_handle ( NULL ), 
		last_sign_in_change ( 0.0f ), sign_in_changed( false ), is_system_ui_showing(false)
{}

bool XboxOneMabSystemNotificationHub::Initialise()
{
	return true;
}

void XboxOneMabSystemNotificationHub::CleanUp()
{
	// Close the notification handle
	CloseHandle( notification_handle );
	notification_handle = NULL;
}

void XboxOneMabSystemNotificationHub::Update()
{
    MABASSERT( notification_handle != NULL );

	bool sign_in_status_updated = false;

    // Check for system notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam = NULL;

	// TODO: Use messaging system for this

	// check if there is a pending sign in change
	if ( sign_in_changed )
	{
		MabTime current_time = MabTime::GetCurrentMabTime();
		MabTime delta_time = current_time - last_sign_in_change;
		if ( delta_time > SIGNIN_WAITING_PERIOD )
		{
			// timer has expired - update the sign in status
			MABLOGMSG( LOGCHANNEL_SYSTEM, LOGTYPE_INFO, "XboxOneUserManager::Update() processing SIGN-IN timer",  dwNotificationID, ulParam );
			//user_info.QuerySigninStatus();
			sign_in_status_updated = true;
			sign_in_changed = false;
			// notify all observers
		}
	}
}

bool XboxOneMabSystemNotificationHub::GetSignInWaitingPeriodInProgress() const
{
	if (sign_in_changed)
	{
		MabTime current_time = MabTime::GetCurrentMabTime();
		MabTime delta_time = current_time - last_sign_in_change;
		return (delta_time <= SIGNIN_WAITING_PERIOD);
	}
	return false;
}