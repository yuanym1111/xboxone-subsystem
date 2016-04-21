/**
 * @file XboxOneMabMatchMaking.h
 * @Match making class.
 * @author jaydens
 * @05/11/2014
 */
#pragma once

#define SECURE_ASSOCIATION_TEMPLATE             L"PeerTrafficUDP"

#define MATCH_HOPPER_NAME						L"RC3_HOPPER"					//For RUBY FIFTEENS
#define MATCH_HOPPER_NAME_SEVEN					L"RC3_HOPPER_SEVEN"

#define MPSD_LOBBY_TEMPLATE                     L"PUBLIC_LOBBY_TEMPLATE"

#define MPSD_GAME_TEMPLATE_SEVEN                L"RC3_GAME_SESSION_TEST_SEVEN" 
#define MPSD_GAME_TEMPLATE                      L"RC3_GAME_SESSION_TEST"         //For RUBY FIFTEENS

//Define some game type
#define RUBY_MATCH_FIFTEEEN						L"GAMETYPE_0"
#define RUBY_MATCH_SEVEN						L"GAMETYPE_1"

#include <stdio.h>
#include "XboxOneMabSingleton.h"

#include <functional>
#include <memory>
#include <concurrent_queue.h>


class XboxOneMabNetOnlineClient;
class XboxOneMabMatchMakingMessage;


class XboxOneMabMatchMaking : public XboxOneMabSingleton<XboxOneMabMatchMaking>,
	public MabObservable<XboxOneMabMatchMakingMessage>
{
public:
	enum MATCH_EVENT {
		MATCH_EVENT_CREATE,
		MATCH_EVENT_JOIN,
		MATCH_EVENT_PEER_CONNECT,
		MATCH_EVENT_HOST_INVITE_SENT,
		MATCH_EVENT_HOST_INVITE_ACCEPTED,
		MATCH_EVENT_PEER_INVITE_RECEIVED,
		MATCH_EVENT_PEER_INVITE_ACCEPTED,
		MATCH_EVENT_LEAVE,
		MATCH_EVENT_READY,
		MATCH_EVENT_START,
		MATCH_EVENT_PLAYING,
		MATCH_EVENT_CONNECTION_LOST,
		MATCH_EVENT_WAITING_FOR_PEER,
		MATCH_EVENT_TERMINATING,

		MAX_MATCH_EVENT
	};

	enum XBOXONE_NETWORK_EVENT {
		XBOXONE_NETWORK_INIT = 0,
		XBOXONE_NETWORK_CONNECTED,
		XBOXONE_NETWORK_DISCONNECTED,
		XBOXONE_NETWORK_RESTORE,
		XBOXONE_NETWORK_DOWN,
		XBOXONE_NETWORK_MATCH_ERROR,
		XBOXONE_NETWORK_MAX
	};
	static void initialize();

	void destroyAll();
	void SetupEventHandler();
	void OnNetworkStatusChange();
	int GetGameMode();

	bool CreateGame(int gamemode = 0, bool bCreateMatchSession = false);
	bool XboxOneStateLauchQuickMatch(int gamemode = 0, bool showTeamSelection = true);

	//Remove the static, it is a singleton already
	bool leave(bool networkStatus = true);
	bool invite();  

	static bool isPlayerHost();
	static bool isConsoleHost();
	void ValidUserSignin();

	void ShowAccountPicker();
	void ShowAccountPickerPending(bool& isPicked);
	void ShowAccountPickerExtra();

	static void ShowSendGameInvite(int gamemode = 0);
	void ProcessProtocalActivate(Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs^ args);

	static void ShowGameCard( MabString* targetXuid, Windows::Xbox::System::IUser^ requestingUser = nullptr);

	bool HasInvitePending();
	bool HasAcceptedInvite();
	bool JoinLobbyAndGame();

	int GetTotalPlayerInSession();

	bool JoinParty(wchar_t* partyID = L"");

	bool HasSignedInUser();
	bool HaveMultiplayerPrivilege();

	//Added staff
	//Todo: Need to implement these functions!!
	void ResetGameplayData();
	void DeleteAllPlayerStates();

	/// Returns a Xbox360OnlineLocalClient for the local client
	const XboxOneMabNetOnlineClient& GetLocalClient() const;

	//Main loop for statemachine to work
	void Update();
	void CheckPrivilege(uint32 priviledgeID, Platform::String^ reason, bool& privilge);
	bool CheckMultiplayerPrivilege();
	bool CheckViewOthersPrivilege(); //Check whether I can view others UGC
	bool GetViewOthersPrivilege(); //Not to show the system UI
	bool CheckContentUploadPrivilege(); //Check whether I can share my UGC with others

	bool ReSet();
	bool CanUserLauchInvitation();
	void ResetMultiplayer();

	void GetMyID(MabString& xboxId);

	bool GetControllerReady(int controller_id);
	void SetControllerReady(int controller_id, bool value);

	void MatchStart();
	bool isInLobby();
	bool isMatchStarted();

	void GetUserName(MabString& xboxUserDisplayName, const MabString* xuid = nullptr);

	bool IsNetworkOnline() {return m_isOnline;};
	bool IsQuickMatch();

	void SetAutoStartGame() { m_automaticStartGame = true; }
	void ClearAutoStartGame() { m_automaticStartGame = false; }
	bool GetAutoStartGame() { return m_automaticStartGame;}

	void OnSuspending();
	void OnResumed();
private:
	int m_userId;
	MabMap<int, bool> controllers_ready;
	bool m_userPrivillege;
	bool m_accountPickerWorking;
	bool m_isOnline;
	Concurrency::critical_section m_processLock;

	//Some privilege to check
	bool m_UGCprivilege;
	bool m_UGCChecked;

	bool m_multiplayPrivilege;
	bool m_multiplayChecked;

	bool m_voicePrivilege;
	bool m_voiceChecked;

	bool m_contentUploadPrivilege;
	bool m_contentUploadChecked;

	bool m_automaticStartGame;

};

class XboxOneMabMatchMakingMessage {
public:
	XboxOneMabMatchMakingMessage( XboxOneMabMatchMaking::XBOXONE_NETWORK_EVENT _type, const void* _data=NULL )
		: type(_type), data(_data) {};

	XboxOneMabMatchMaking::XBOXONE_NETWORK_EVENT GetType() const { return type; }
	const void* GetPayload() const {return data;};

private:
	XboxOneMabMatchMaking::XBOXONE_NETWORK_EVENT type;
	const void* data;
};

