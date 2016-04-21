/**
 * @file XboxOneMabSessionManager.h
 * @Session management class.
 * @author jaydens
 * @22/10/2014
 */

#ifndef XBOX_ONE_MAB_SESSION_MANAGER_H
#define XBOX_ONE_MAB_SESSION_MANAGER_H

#include "MabString.h"
#include "XboxOneMabSingleton.h"

#include "XboxOneMabEventHandler.h"
#include <functional>
//#include "MabMemSTLAllocation.h"
#include "XboxOneMabConnect.h"

typedef std::function<void(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^, XboxOneMabLib::SessionState)> MultiplayerStateChanged;

typedef std::function<void( XboxOneMabLib::MatchmakingArgs^)> MatchStatusChanged;


class XboxOneMabSessionManager:public XboxOneMabSingleton<XboxOneMabSessionManager>
{
public:
    static Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ WriteSessionHelper(
        Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession,
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode writeMode,
        HRESULT& hr
        );

    static HRESULT RegisterGameSessionCompareHelper(
        Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
        Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReference,
		Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReferenceComparand		
        );

    static HRESULT RegisterMatchSessionCompareHelper(
        Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
		Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReference,
		Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReferenceComparand
        );

    static Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^
    GetSessionHelper(
        Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef
        );

    static Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^
    GetCurrentSessionHelper(
        Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
        );

	XboxOneMabSessionManager();

	~XboxOneMabSessionManager();

	void Initalize(  
		Platform::String^ lobbySessionTemplate,
		Platform::String^ gameSessionTemplate,
		Platform::String^ matchmakingHopper
		);

	static Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ CreateMultiplayerSession();
    static bool IsPlayerHost( Platform::String^ xboxUserId, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session );
    static bool IsConsoleHost( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);
    static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ GetHostMember( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession );

    static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ GetCurrentUserMemberInSession( 
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
        );

    static bool IsPlayerInSession(
        Platform::String^ xboxUserId,
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);

    static Concurrency::task<void> LeaveAllSessions();

    static bool IsMemberTheHost( 
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session,
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ member 
        );

    static int GetEmptySlots( 
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
        );
	static void ListAllExistingSessions();

	Windows::Foundation::IAsyncAction^ RefreshSessionsAsync();
	Windows::Foundation::IAsyncOperation<XboxOneMabLib::HostMigrationResultArgs^>^ HandleHostMigration(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);

	bool IsLobbySession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);
	bool IsGameSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);
	bool IsLobbyHost();
	bool IsGameHost();

	/************************************************************************/
	/*        Added New function to support 2015 multiplayer function       */
	/************************************************************************/
	// Parses protocol activation args and returns an enum value for the action
	static XboxOneMabLib::MultiplayerActivationInfo^
		GetMultiplayerActivationInfo(
		Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs^ args
	);

	// Generates a GUID as a string that can be used for a unique session name
	Platform::String^
		GenerateUniqueName(
	);

	GUID* GetUniqueName(){ return &m_sessionID;}

	// Subscribes to RTA events for multiplayer and should be
	// called before other non-static public functions
	void SetXboxLiveContext(
		Microsoft::Xbox::Services::XboxLiveContext^ context
	);

	// Stop listening to RTA events and attempt to cleanly leave all mutliplayer sessions
	void LeaveMultiplayer(
		bool leaveCleanly
		);

	// Leave the current game session
	void LeaveGameSession();

	// Leave the current lobby session
	void LeaveLobbySession();

	//Put the session reference Uri to lobby session
	void AdvertiseGameSession(MabMap<MabString,MabString>* customerParams = NULL);

	//Get session uri from lobby session
	Platform::String^ GetAdvertisedGameSessionUri();

	// Call this with the info from a protocol activation to join an existing multiplayer session
	Windows::Foundation::IAsyncAction^ JoinMultiplayerAsync(
		XboxOneMabLib::MultiplayerActivationInfo^ activationInfo
		);

	// Use this to join an existing game session from a session reference uri
	Windows::Foundation::IAsyncAction^ JoinGameSessionAsync(
		Platform::String^ gameSessionUri
		);

	// Call as part of the Become Joinable flow.  This establishes the lobby session and joins
	// the current user to it with open visibility so that others may join or be invited.
	Windows::Foundation::IAsyncAction^ CreateLobbySessionAsync();

	// Call to move the current lobby users into the game session.  If no game sessions exists
	// it will be created first.
	Windows::Foundation::IAsyncAction^ CreateGameSessionFromLobbyAsync();

	// Same as above except the session is populated with a list of xuids instead of from the
	// lobby session.
	Windows::Foundation::IAsyncAction^ CreateGameSessionForUsersAsync(
		Windows::Foundation::Collections::IVectorView<Platform::String^>^ xuids
		);

	// Submit the current lobby session for matchmaking.  The game session will be created by MPSD.
	Windows::Foundation::IAsyncAction^ BeginSmartMatchAsync();

	// Attempt to set the current user as the host of the lobby session
	Windows::Foundation::IAsyncOperation<bool>^ TryBecomeLobbyHostAsync();

	// Attempt to set the current user as the host of the game session
	Windows::Foundation::IAsyncOperation<bool>^ TryBecomeGameHostAsync();

	//Call to cancel the match ticket
	Windows::Foundation::IAsyncAction^ CancelMatchmakingAsync();

	// Set the join restriction - dictating who is allowed to join this session
	void SetLobbySessionJoinRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionJoinRestriction joinRestriction);


	// Fired when the Session is updated, callback function for StateManager

	MultiplayerStateChanged  OnMultiplayerStateChanged;

	// Fired when the Matchmaking state changed
	MatchStatusChanged OnMatchmakingChanged;


	//Getter,Setter Functions
	inline Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLobbySession() {Concurrency::critical_section::scoped_lock lock(m_stateLock); return m_lobbySession;};
	inline Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetGameSession() {Concurrency::critical_section::scoped_lock lock(m_stateLock); return m_gameSession;};
	inline XboxOneMabLib::SessionManagerState GetMyState(){return m_state;};
	inline Microsoft::Xbox::Services::XboxLiveContext^ GetMyContext(){Concurrency::critical_section::scoped_lock lock(m_stateLock); return m_xblContext;};

	//Todo: Implement it on XboxOneMabConnectManager
	bool GenerateMabXboxOneClient(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);
	bool GetConnectedMabXboxOneClientDetails(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, XboxOneMabNetOnlineClient* host);
	inline XboxOneMabNetOnlineClient* GetRemoteClient(){return m_remoteClient;};
	bool GetLocalMabXboxOneClientDetails(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, XboxOneMabNetOnlineClient* local_client);

	Windows::Xbox::Networking::SecureDeviceAssociation^ GetAssociation(MabString xboxid);
	Windows::Xbox::Networking::SecureDeviceAssociation^ GetRemoteAssociation();

	MabString GetSessionUserGametag(const MabString& xuid);
private:


	Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^
		WriteSession(
		Microsoft::Xbox::Services::XboxLiveContext^ context,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode writeMode
		);

	Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^
		UpdateAndProcessSession(
		Microsoft::Xbox::Services::XboxLiveContext^ context,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode writeMode
		);

	Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^  CreateLobbySession();

	Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ CreateGameSession(
		Windows::Foundation::Collections::IVectorView<Platform::String^>^ xuids
		);

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ CreateNewDefaultSession(
		Platform::String^ templateName
		);


	void OnSessionChanged(
		Platform::Object^ object, 
		Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityMultiplayerSessionChangeEventArgs^ arg
		);

	void OnSubscriptionLost(
		Platform::Object^ object,
		Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityMultiplayerSubscriptionsLostEventArgs^ arg
		); 


	void ProcessSessionChange(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
		);

	void ProcessSessionDeltas(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ previousSession,
		unsigned long long lastChange
		);

	XboxOneMabLib::MatchmakingArgs^ HandleMatchmakingStatusChanged(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession
		);

	void HandleMatchSessionFound(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession
		);


	XboxOneMabLib::MatchmakingArgs^ HandleInitilizationStateChanged(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession
		);

	bool ResubmitTicketSession();

	void BeginQoSMeasureAndUpload(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession
		);


	inline bool IsMultiplayerSessionChangeTypeSet(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes value,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes check
		)
	{
		return (value & check) == check;
	}

	void StartListeningToMultiplayerEvents();
	void StopListeningToMultiplayerEvents();

	void OnAssociationIncoming( 
		Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ associationTemplate, 
		Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^ args 
		);
	void AddAssociation(MabString xboxid, Windows::Xbox::Networking::SecureDeviceAssociation^ deviceAsso);

	Microsoft::Xbox::Services::XboxLiveContext^                          m_xblContext;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^          m_gameSession;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^          m_lobbySession;
	Microsoft::Xbox::Services::Matchmaking::CreateMatchTicketResponse^   m_matchTicketResponse;
	XboxOneMabLib::SessionManagerState                               m_state;
	XboxOneMabLib::MatchmakingState                                      m_matchmakingState;
	Platform::String^                                                    m_gameSessionTemplate;
	Platform::String^                                                    m_lobbySessionTemplate;
	Platform::String^                                                    m_hopperName;
	Platform::String^                                                    m_serviceConfigId;
	bool                                                                 m_bLeavingSession;
	unsigned long long                                                   m_lastProcessedLobbyChange;
	unsigned long long                                                   m_lastProcessedGameChange;

	Concurrency::critical_section                                        m_stateLock;
	Concurrency::critical_section                                        m_processLock;
	Concurrency::critical_section                                        m_taskLock;

	Windows::Foundation::EventRegistrationToken                          m_sessionChangeToken;
	Windows::Foundation::EventRegistrationToken                          m_subscriptionLostToken;
	Windows::Foundation::EventRegistrationToken							 m_associationIncomingToken;

	//Keep track of old operation to avoid start it twice
	Windows::Foundation::IAsyncOperation<XboxOneMabLib::HostMigrationResultArgs^>^      m_lobbyMigrationOperation;
	Windows::Foundation::IAsyncOperation<XboxOneMabLib::HostMigrationResultArgs^>^      m_gameMigrationOperation;


	//To keep track of the remote connection, Currently only one is stored here, it should be removed
	//Todo: Implement it on XboxOneMabConnectManager
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^   m_peerDeviceAssociationTemplate;
	XboxOneMabNetOnlineClient*			 m_remoteClient;

	GUID														 m_sessionID;


	MabMap<MabString, Windows::Xbox::Networking::SecureDeviceAssociation^>	m_SecureDeviceMap;
	 Windows::Xbox::Networking::SecureDeviceAssociation^ m_testRemoteSecureAssociation;
};

#endif