#ifndef _XBOXONE_MAB_SESSION_SEARCH_MANAGER_H
#define _XBOXONE_MAB_SESSION_SEARCH_MANAGER_H

#include "MabString.h"
#include "XboxOneMabSingleton.h"
#include "XboxOneMabSubsystem.h"
#include "MabNetIClientAddressProvider.h"
#include "MabNetOnlineClientDetails.h"
#include "XboxOneMabNetTypes.h"
#include "MabNetAddress.h"
#include "XboxOneMabMatchmaking.h"

class MabNetIClientAddressProvider;
class XboxOneMabUISessionControllerMessage;
class XboxOneMabUISessionController;
const float FriendSessionUpdateInterval = 10.0f;



class XboxOneMabUISessionController :   public XboxOneMabSingleton<XboxOneMabUISessionController>, 
										public MabObservable< XboxOneMabUISessionControllerMessage >,
										public MabNetIClientAddressProvider
{

public:
	enum SESSION_EVENT
	{
		SESSION_EVENT_CREATE,		///< Create operation complete
		SESSION_EVENT_PRE_JOIN,		///< Player to be added to session
		SESSION_EVENT_POST_JOIN,	///< Player join operation complete
		SESSION_EVENT_PEER_CONNECT, 
		SESSION_EVENT_REGISTER,		///< Register operation complete
		SESSION_EVENT_MODIFY,		///< Session update operation complete
		SESSION_EVENT_FLUSH,		///< Stats flush operation complete
		SESSION_EVENT_START,		///< Session start operation complete
		SESSION_EVENT_PRE_LEAVE,	///< Player about to be removed from session (notify listeners so they can write stats if they need to)
		SESSION_EVENT_POST_LEAVE,	///< Player leave operation complete
		SESSION_EVENT_END,			///< Session end operation complete
		SESSION_EVENT_DELETE,		///< Session delete operation complete
		SESSION_EVENT_ACCEPT_INVITE,///< Nowhere else to put this for now, but consider moving later.
		SESSION_EVENT_TERMINATED,	///< Notification that the session has been terminated, and we're going to have to let it die.
		SESSION_SEARCH_SEARCH_COMPLETED,
		SESSION_SEARCH_QOS_POLLING_COMPLETED,
		SESSION_EVENT_ACCEPT_GMAEMODE,
		SESSION_EVENT_USER_LEFT,
		SESSION_EVENT_USER_ADDED,
		SESSION_EVENT_JOIN_RESTRICTION,
		SESSION_EVENT_GAME_FULL,
		SESSION_EVENT_GAME_NOMATCHINFO,
		SESSION_EVENT_OPPONENT_LEFT
	};
	XboxOneMabUISessionController();
	~XboxOneMabUISessionController();

	/// pair of controller index and bool flag if the controller consumes a private slot
	struct ControllerSlot
	{
		ControllerSlot( int _controller_id, bool _is_private ) : controller_id( _controller_id ), is_private( _is_private ) {};
		int controller_id;
		bool is_private;
	};

	static void initialize();

	void Update(MabObservable<XboxOneMabUISessionControllerMessage> *source, const XboxOneMabUISessionControllerMessage &msg);

	/** @name Session invite methods */
	//@{ 

	/// Returns the accepted invite structure
	void AcceptInvitation();

	void ConsumeInvitation();

	/// Returns true if the player has accepted an invitation and the game hasn't been joined yet
	bool HasAcceptedInvite() const;

	/// Shows the friends ui for the specified user.
	void ShowFriendsUI();
	
	/// Shows the invite other users UI .
	void ShowInviteUI();


	void ShowHelpUI();
	//@}

	/// Adds players to session - calls AddLocalPlayers if local otherwise AddRemotePlayers
	bool AddPlayersToSession( const XboxOneMabNetOnlineClient& client );

	/// Host call. On peer joins.
	void OnPeerJoin(const XboxOneMabNetOnlineClient& client);

	/// Peer call. On peer joins.
	void OnJoin(const XboxOneMabNetOnlineClient& host);

	void OnNetworkStatusChanged(bool status);

	//@}

	// Called when want to host a private match
	void onHostSession();

	/// Returns true if we have an active session (XSessionCreate)
	bool IsSessionActive() const;

	/// Returns true if the session is dying from some kind of error case.
	/// We need this, as we should not try to remove users from the session if the session _is_ dying.
	bool IsSessionDying() const;

	///Check whether the game state is still in match making, it still need to do leave game logic here
	bool IsMatchMakingStarted() const;

	/// Returns true if the session has been started (XSessionStart)
	bool IsSessionStarted() const;

	/// Returns true if this client is the host
	bool IsHost( const XboxOneMabNetOnlineClient& client ) const;

	/// Returns true if the local client is host
	bool IsHost() const;

	/// Returns true if this is a multiplayer session
	bool IsMultiplayer() const;

	/// Returns true if this client is the local client
	bool IsLocalClient( const XboxOneMabNetOnlineClient& client ) const;

	/// Returns the total number slots
	UINT GetTotalSlots() const {return 2;};

	/// Returns the number of public slots
	UINT GetTotalPublicSlots() const {return 2;};

	/// Returns the number of private slots
	UINT GetTotalPrivateSlots() const{return 2;};

	/// Returns the total number of filled slots
	UINT GetFilledSlots() const{return 0;};

	/// Returns the number of filled public slots
	UINT GetFilledPublicSlots() const {return 0;};

	/// Returns the number of filled private slots
	UINT GetFilledPrivateSlots() const {return 0;};

	/// Return the owner controller for the session
	int GetSessionOwner() const {return 0;};

	/// Update the local client structure with players in the controller list. 
	/// Should only be called before session is active. This is called internally when a new session is created.
	void UpdateLocalClientInfo();

	/// Returns a XboxOneOnlineLocalClient for the local client
	const XboxOneMabNetOnlineClient& GetLocalClient() const;
	
	bool GetAddressForClient( const MabNetOnlineClientDetails& details, MabNetAddress& address ) const;

	void setPresence(MabString &content);

	bool StringValidation(const MabString* userString);

	bool onUserBecomeNull(bool inGame = true);

	bool onControllerUserAdded();

	void OnMatchMakingFailed();

private:
	Windows::Foundation::IAsyncAction^ TrySetRichPresenceAsync(Platform::String^ presence);
private:

	//--------------------------------------------------------------------------------------
	// Constants
	//--------------------------------------------------------------------------------------
	static const DWORD PUBLICSLOTS  = 4;     // Default number of public slots
	static const DWORD PRIVATESLOTS = 4;     // Default number of private slots

	static const DWORD X_CONTEXT_GAME_MODE_INVALID = (DWORD)-1;

	// valid session states
	enum SESSION_STATE
	{
		SESSION_STATE_NONE,
		SESSION_STATE_CREATING,
		SESSION_STATE_PRE_GAME,
		SESSION_STATE_REGISTERING,
		SESSION_STATE_STARTING,
		SESSION_STATE_IN_GAME,
		SESSION_STATE_ENDING,
		SESSION_STATE_DELETING,
		SESSION_STATE_MAX,
	};

	// slot types for the session
	enum SLOTS
	{
		SLOTS_TOTALPUBLIC,
		SLOTS_TOTALPRIVATE,
		SLOTS_FILLEDPUBLIC,
		SLOTS_FILLEDPRIVATE,
		SLOTS_MAX
	};


	// base data container with virtual destructor so we can cast to this for deletion and the appropriate destructor is called
	struct XSessionData
	{
		virtual ~XSessionData() {};
	};

	struct XInviteInfo {
		MabString xbox_user_id;		///< the controller that accepted the invite
		LPCWSTR party_id;	///< the invite info structure
	};

	MABMEM_HEAP							heap;						///< Memory heap
	HANDLE								session_handle;				///< Session handle
	bool								is_host;					///< Is hosting
	ULONGLONG							session_nonce;				///< Nonce of the session
	DWORD								session_flags;				///< Session creation flags
	DWORD								game_type;					///< The session game type context
	DWORD								game_mode;					///< The session game mode context
	int									owner_controller;			///< Which controller created the session
	bool								cleanup_session;			///< flag if session manager should clean up because connection to live was lost
	XboxOneMabNetOnlineClient*			local_client;				///< local client structure
	SESSION_STATE						session_state;				///< current session state
	bool								have_accepted_invite;		///< accepted invite flag
	XInviteInfo							invite_info;				///< accepted invite info
	UINT								slots[ SLOTS_MAX ];			///< public/private slots for the session
};

///
/// Session update notification message
///
class XboxOneMabUISessionControllerMessage
{
public:
	XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT _type, bool _result, bool _multiplayer_session, const void* _data=NULL )
		: type(_type), result(_result), data(_data), multiplayer_session(_multiplayer_session) {};

	XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT _type, DWORD _hr,  bool _multiplayer_session, const void* _data=NULL )
		: type(_type), result(SUCCEEDED(_hr)), data(_data), multiplayer_session(_multiplayer_session) {};

	XboxOneMabUISessionController::SESSION_EVENT GetType() const { return type; }
	bool GetResult() const { return result; }

	// Because state values on the session manager may have already reset by the time we get a 
	// message, we need to store this on the message.
	bool GetSessionMultiplayer() const { return multiplayer_session; }

	const void* GetPayload() const {return data;};

	/// return pointer to online client who was just added / removed
	const XboxOneMabNetOnlineClient* GetOnlineClient() const 
	{
		MABASSERT( type == XboxOneMabUISessionController::SESSION_EVENT_PRE_JOIN || type == XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN || type == XboxOneMabUISessionController::SESSION_EVENT_PRE_LEAVE || type == XboxOneMabUISessionController::SESSION_EVENT_POST_LEAVE );
		return ( type == XboxOneMabUISessionController::SESSION_EVENT_PRE_JOIN || type == XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN || type == XboxOneMabUISessionController::SESSION_EVENT_PRE_LEAVE || type == XboxOneMabUISessionController::SESSION_EVENT_POST_LEAVE ) ? reinterpret_cast<const XboxOneMabNetOnlineClient*>( data ) : NULL; 
	}


private:
	XboxOneMabUISessionController::SESSION_EVENT type;
	bool result;
	const void* data;
	bool multiplayer_session;
};



#endif