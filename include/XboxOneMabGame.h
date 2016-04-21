/**
 * @file XboxOneMabGame.h
 * @This is a game data module, it manages all status related data of XboxOne online services.
 * @author jaydens
 * @22/10/2014
 */

#ifndef __XBOXONEMABGAME_H
#define __XBOXONEMABGAME_H
#include "XboxOneMabSingleton.h"
#include "XboxOneMabEventHandler.h"

#define RUGBY_CHALLENGE_3_TITLE_ID							0x6EF2533A
#define GAME_SERVICE_CONFIG_ID                              L"9e280100-bd30-4d81-a197-39d76ef2533a"
#define MATCH_SESSION_TEMPLATE_NAME                         L"RC3_MATCH_TICKET_SESSION_TEMPLATE"
#define MAX_MEMBER_LIMIT									0	//0 means the session will use the max member limit in the session template.
#define MAX_SESSION_MEMBERS									16  // max members as specified in the session template.
#define XPRIVILEGE_MULTIPLAYER_SESSIONS                     (uint32)Windows::Xbox::ApplicationModel::Store::KnownPrivileges::XPRIVILEGE_MULTIPLAYER_SESSIONS
#define XPRIVILEGE_SOCIAL_NETWORK_SHARING					(uint32)Windows::Xbox::ApplicationModel::Store::KnownPrivileges::XPRIVILEGE_SOCIAL_NETWORK_SHARING
#define XPRIVILEGE_USER_CREATED_CONTENT						(uint32)Windows::Xbox::ApplicationModel::Store::KnownPrivileges::XPRIVILEGE_USER_CREATED_CONTENT    //View others content online, View popular content for example
#define XPRIVILEGE_COMMUNICATION_VOICE_INGAME				(uint32)Windows::Xbox::ApplicationModel::Store::KnownPrivileges::XPRIVILEGE_COMMUNICATION_VOICE_INGAME
#define XPRIVILEGE_SHARE_CONTENT							(uint32)Windows::Xbox::ApplicationModel::Store::KnownPrivileges::XPRIVILEGE_SHARE_CONTENT			//Share my contents with others, upload content

#define LAST_INVITED_TIME_TIMEOUT                           3 * 60  //secs
#define MATCH_HOPPER_AUTO_EVAL_HOPPER_NAME                  L"PlayerSkillAutoEval"
#define MATCH_HOPPER_TITLE_EVAL_HOPPER_NAME                 L"PlayerSkillTitleEval"

#define MAX_PLAYER_SESSION_ENTRIES							15
#define MAX_GAME_PLAYERS									2

#define RUGBY_SEVEN_TYPE									"SEVEN"
#define RUGBY_FIFTEEN_TYPE									"FIFTEEN"
#define ONLINE_QUICK_MATCH									"QUICKMATCH"
#define ONLINE_PRIVATE_MATCH								"PRIVATEMATCH"


#define MULTIPLAYER_RESTRICTION								L"Accept, multiplayer online matches"
#define UPLOAD_RESTRICTION									L"upload/share your players with others, please check your privacy setting"
#define SHOW_OTHERS_RESTRICTION								L"view others' contents, please check your privacy setting"

enum ENUM_GAME_STATE {
	E_GAME_NOT_START,
	E_GAME_CREATED,
	E_GAME_RUNNING,
	E_GAME_LEAVING_PARTY,
	E_GAME_TERMINATING,
	E_GAME_INVALID
} ;

enum QOS_OPTION
{
    QOS_OPTION_SYSTEM = 0,
    QOS_OPTION_TITLE_MEASURE,
    QOS_OPTION_TITLE_EVAL,
    QOS_OPTION_COUNT
};

struct LobbyData
{
	bool inGame;
	bool lobbyReady;
};

ref class XboxOnePlayerState sealed
{
internal:
	XboxOnePlayerState(Platform::String^ displayName = "");

	// Lobby functions
	void DeactivatePlayer() {};
	void EnterLobby() {};
	void ReactivatePlayer() {};

	void DeserializeLobbyData(Platform::Array<unsigned char>^ data) {};
	Platform::Array<unsigned char>^ SerializeLobbyData() {return nullptr;};

	// State properties
	property Platform::String^ DisplayName;
	property bool IsLocalPlayer;
	property bool InGame;
	property bool LobbyReady;

	property bool IsInactive
	{
		bool get() { return m_isInactive; }
	}


private:
	bool m_isInactive;
};




class XboxOneMabGame : public XboxOneMabSingleton<XboxOneMabGame>
{
public:
	XboxOneMabGame();
	~XboxOneMabGame();

	void Initialize();
	ENUM_GAME_STATE GetGameState() { return m_gameState; }
	void setGameState(ENUM_GAME_STATE state) { m_gameState = state; }
	void SetMultiplayerSession( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session );
	bool IsSlotFillingInProgress();
	void SetIsSlotFillingInProgress( bool isInProgress );
	void SetOnlineUser();

    bool IsMatchmakingInProgress();
    void SetMatchmakingInProgress( bool val );

    bool HasMatchTicketExpired();
    void SetMatchTicketExpired( bool val );

	//Game data maintain
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetMultiplayerSession();

    void UpdateCountOfActivePlayers();

    void PushAssociationIncoming( Windows::Xbox::Networking::SecureDeviceAssociation^ peerDeviceAssociation );
    Windows::Xbox::Networking::SecureDeviceAssociation^ PopAssociationIncoming();

	bool IsHost() {return m_isHost;};
    bool IsArbiter();
    bool IsMatchSession();
    void SetIsHost(bool isHost);
    void SetIsArbiter(bool isArbiter);
    void SetIsMatchSession(bool isMatchSession);
    void ClearGameConsoleInfoList();
    
	void SetUser(Windows::Xbox::System::User^ user);
	inline Windows::Xbox::System::User^ GetCurrentUser(){return m_user;}
	void SetCurrentGamePad(Windows::Xbox::Input::IGamepad^ gamePad);
	inline Windows::Xbox::Input::IGamepad^ GetCurrentGamePad(){return m_currentGamepad;}
	void UpdateFriendXuidToNameMap();

    void SetServerName( Platform::String^ serverName );
    Platform::String^ GetServerName();
//	void OnSessionChanged( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session );
//	void OnSessionCleared();
//	void ResubmitMatchTicket(bool bTicketExpired);
//	HRESULT DeleteMatchTicket( Platform::String^ matchTicketId );
	Platform::String^ GetHopperName();
//	QOS_OPTION GetQosOption();
//	Platform::String^ GetMatchTicketId();
//  void HandleSessionChange( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session );

	void UpdateFriendActivityDetails(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails ^>^ details);
	void UpdateFriendSessionList();

	inline void SetInvitation(bool inviteState) {m_have_accepted_invite = inviteState;};
	inline bool IsInvitationAccepted(){return m_have_accepted_invite;};

private:
    Concurrency::critical_section m_stateLock;

	// Friend map lock
	Concurrency::critical_section m_friendMapLock;

	ENUM_GAME_STATE m_gameState;
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ m_multiplayerSession;
    bool m_isSlotFillInProgress;
    bool m_shouldMatchTryFillingSlots;
    DWORD m_countOfActivePlayers;
    bool m_isHost;                  // The console hosting the game state for all players.
    bool m_isArbiter;               // The console managing game session and party updates on behalf of all players.
    bool m_isMatchmakingInProgress;
    bool m_hasMatchTicketExpired;
	bool m_isMatchSession;
    QOS_OPTION m_qosOption;
    Platform::String^ m_matchTicketId;

	bool m_have_accepted_invite;

	//Record my state using this class, part from XboxOneMabUserinfo
	//
	// User-Related Properties
	//

	// CurrentGamepad
	Windows::Xbox::Input::IGamepad^ m_currentGamepad;
	Windows::Xbox::System::User^    m_user;
//	Platform::Collections::Map<Platform::String^, XboxOnePlayerState^>^ m_playerStates;

	std::map<Platform::String^, Platform::String^> FriendXuidToNameMap;


	std::vector<XboxOneMabLib::MultiplayerActivationInfo^> m_friendSessionHandles;  //Update when search session
};

#endif