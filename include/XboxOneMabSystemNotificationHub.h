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

#ifndef XBOXONE_MAB_SYSTEM_NOTIFICATION_HUB_H
#define XBOXONE_MAB_SYSTEM_NOTIFICATION_HUB_H

#include <MabTime.h>
#include "XboxOneMabSubsystem.h"

///
/// Class to encapsulate the xbox system notification message
///
class XboxOneMabSystemNotificationMessage
{
public:
	XboxOneMabSystemNotificationMessage( DWORD _id, ULONG_PTR _param )
		: id(_id), param(_param) {};

	DWORD GetID() const { return id; }

	ULONG_PTR GetParam() const { return param; }

private:
	DWORD id;
	ULONG_PTR param;
};

// Forward-declare UserInfo class; needs to be passed in to ctor
class XboxOneMabUserInfo;

/** Class to manage Notifications that sent by the Xbox 360's internal systems.
	@author Cameron Hart, original code; renamed, made non-singleton, and split out from UserInfo for Mab by Mark Barrett
*/

class XboxOneMabSystemNotificationHub
	:	public MabObservable< XboxOneMabSystemNotificationMessage >,
		public XboxOneMabSubsystem,
		public MabNonCopyable
		
{
public:

	/** @name Constructors/deconstructors/initialisers **/
	//@{

	XboxOneMabSystemNotificationHub( MABMEM_HEAP _heap, XboxOneMabUserInfo& _user_info );
	virtual ~XboxOneMabSystemNotificationHub() {}

	virtual bool Initialise();
	virtual void Update();
	virtual void CleanUp();

	inline bool IsSystemUIActive() const {return is_system_ui_showing;}

	/// This tells us if we're currently waiting on the 1 second timeout (See below & long comment at the 
	/// top of the cpp file). It makes it trivial to block input while we wait to see what's going on.
	bool GetSignInWaitingPeriodInProgress() const;

	//@}

	/// The time we need to wait in order to work around an MS bug--see comment in .cpp
	static const MabTime SIGNIN_WAITING_PERIOD;

private:

	MABMEM_HEAP heap;
	/// The handle used by the system call
	HANDLE notification_handle;

	/// A time that gets set to the present time whenever a sign-in operation happens
	MabTime last_sign_in_change;

	/// Communicates that the signin has changed recently
	bool sign_in_changed;

	/// A reference to the UserInfo instance
	XboxOneMabUserInfo& user_info;

	/// Is set to true when the system UI is activated
	bool is_system_ui_showing;
};

#endif //XBOXONE_MAB_SYSTEM_NOTIFICATION_HUB_H