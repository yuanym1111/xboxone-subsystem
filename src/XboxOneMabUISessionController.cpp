#include "pch.h"
#include "XboxOneMabUISessionController.h"
#include "XboxOneMabStateManager.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUtils.h"
#include "MabNetAddress.h"
#include "XboxOneMabUserInfo.h"

using namespace XboxOneMabLib;

using namespace Concurrency;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Xbox::System;
using namespace Windows::Foundation;
using namespace Microsoft::Xbox::Services;
using namespace Windows::Xbox::Services;

XboxOneMabUISessionController::XboxOneMabUISessionController()
{
	local_client = new XboxOneMabNetOnlineClient();
}

XboxOneMabUISessionController::~XboxOneMabUISessionController()
{
	if(local_client!=nullptr) {
		delete local_client;
	}
}

void XboxOneMabUISessionController::initialize()
{

}

void XboxOneMabUISessionController::Update(MabObservable<XboxOneMabUISessionControllerMessage> *source, const XboxOneMabUISessionControllerMessage &msg)
{
	MABUNUSED(source);
	MABUNUSED(msg);
}


bool XboxOneMabUISessionController::GetAddressForClient(const MabNetOnlineClientDetails& details, MabNetAddress& address) const
{
	const PSMabNetOnlineClientDetails& ps_details = details.GetDetails();
	char c_hostname[2048];

	memset(c_hostname,0,sizeof(c_hostname));
	inet_ntop(AF_INET6, (void *)(&(ps_details.local_address.saddrV6.sin6_addr)), c_hostname, INET6_ADDRSTRLEN);

	MabString str_hostname = c_hostname;

	address.SetHost(str_hostname);
	address.SetPort(0);
	return true;
}

void XboxOneMabUISessionController::UpdateLocalClientInfo( )
{
	if ( !IsSessionActive() )
	{
		MABLOGMSG( LOGCHANNEL_NETWORK, LOGTYPE_ERROR,  "XboxOneMabUISessionController::UpdateLocalClientInfo() cannot update during active session" );
		MABBREAK();
		return;
	}

    // Get the local player info
	Platform::String^ xbox_user_id;
	xbox_user_id = XboxOneGame->GetCurrentUser()->XboxUserId;
	std::string narrow(xbox_user_id->Begin(), xbox_user_id->End());
	MabString str_xbox_user_id = narrow.c_str();
	local_client->member_id = str_xbox_user_id;

	Platform::String^ gamertag = XboxOneGame->GetCurrentUser()->DisplayInfo->Gamertag;
	std::string narrow_gametag(gamertag->Begin(), gamertag->End());
	local_client->game_tag = narrow_gametag.c_str();
	local_client->num_players = 1;

	XboxOneSessionManager->GetLocalMabXboxOneClientDetails(XboxOneSessionManager->GetGameSession(),local_client);
}

/// Returns a XboxOneOnlineLocalClient for the local client
const XboxOneMabNetOnlineClient& XboxOneMabUISessionController::GetLocalClient() const
{
	MABASSERT(local_client->num_players > 0 );
	return *local_client;
}

bool XboxOneMabUISessionController::AddPlayersToSession( const XboxOneMabNetOnlineClient& client )
{
	MABLOGDEBUG("Adding %s in to the session!", client.member_id.c_str());
	return true;
}

bool XboxOneMabUISessionController::IsMultiplayer() const
{
	return true;
}

//The session sever will wait for this mabnetOnlineClient to join
void XboxOneMabUISessionController::OnPeerJoin(const XboxOneMabNetOnlineClient& sessionClient)
{
	XboxOneMabNetOnlineClient* clientaddress = new XboxOneMabNetOnlineClient(sessionClient);
	Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_PEER_CONNECT, true, IsMultiplayer(), clientaddress ) );
	delete clientaddress;
}

//The session client is trying to connect for this mabnetOnlineClient
void XboxOneMabUISessionController::OnJoin(const XboxOneMabNetOnlineClient& sessionServer)
{
	XboxOneMabNetOnlineClient* hostaddress = new XboxOneMabNetOnlineClient(sessionServer);
	//Some Hack, wait untill server is established.
	//TODO:should be a handshake process
	Sleep(3000);
	Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_PEER_CONNECT, true, IsMultiplayer(), hostaddress ) );
	delete hostaddress;
}


void XboxOneMabUISessionController::onHostSession()
{
	Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_CREATE, true, IsMultiplayer()) );
}


bool XboxOneMabUISessionController::IsHost() const
{
	return XboxOneSessionManager->IsGameHost();
}

void XboxOneMabUISessionController::ShowInviteUI()
{
	XboxOneMabMatchMaking::Get()->ShowSendGameInvite();
}

bool XboxOneMabUISessionController::IsSessionActive() const
{
	//Todo: will update MabGame gamesession pointer too
	return (XboxOneSessionManager->GetGameSession() != nullptr);
}

bool XboxOneMabUISessionController::IsSessionStarted() const
{
	return (XboxOneGame->GetMultiplayerSession() != nullptr);
}

bool XboxOneMabUISessionController::IsHost( const XboxOneMabNetOnlineClient& client ) const 
{
	MabString xbox_user_id = client.member_id;
	Platform::String^ str_xbox_user_id = XboxOneMabUtils::ConvertCharStringToPlatformString(xbox_user_id.c_str());
	
	return XboxOneSessionManager->IsPlayerHost(str_xbox_user_id, XboxOneGame->GetMultiplayerSession());
}

bool XboxOneMabUISessionController::IsSessionDying() const
{
	if(XboxOneGame->GetCurrentUser() == nullptr)
		return false;
	bool isPlayerInSession = XboxOneSessionManager->IsPlayerInSession(XboxOneGame->GetCurrentUser()->XboxUserId,XboxOneSessionManager->GetGameSession());
	return !isPlayerInSession;
}

bool XboxOneMabUISessionController::IsMatchMakingStarted() const
{
	return XboxOneStateManager->GetMyState() == GameState::Matchmaking;
}

void XboxOneMabUISessionController::ShowFriendsUI()
{
#if 1
    User^ user = XboxOneUser->GetCurrentUser();
	IAsyncOperation<Windows::Xbox::UI::AddRemoveFriendResult^>^ partyOperation = 
		Windows::Xbox::UI::SystemUI::ShowAddRemoveFriendAsync( user, nullptr);
	

    create_task(partyOperation)
        .then([] (task<Windows::Xbox::UI::AddRemoveFriendResult^> t)
    {
		try
		{
			t.get(); // if t.get() didn't throw, it succeeded
			XboxOneMabUtils::LogComment(L"Friend UI shown");
		}
		catch(Platform::Exception^ ex)
		{
            XboxOneMabUtils::LogCommentWithError( L"ShowFriendsUI failed", ex->HResult );
		}

    }).wait();
#endif
}

void XboxOneMabUISessionController::AcceptInvitation()
{
	XboxOneGame->SetInvitation(true);
}

void XboxOneMabUISessionController::ConsumeInvitation()
{
	XboxOneGame->SetInvitation(false);
}

//a mark to check whether the game is left from an invitaion
bool XboxOneMabUISessionController::HasAcceptedInvite() const
{
	return XboxOneGame->IsInvitationAccepted();
}

void XboxOneMabUISessionController::setPresence(MabString &content)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabUISessionController::setPresence start");
	Platform::String^ presenceString = XboxOneMabUtils::ConvertMabStringToPlatformString(&content);
	if(XboxOneMabNetGame->IsNetworkOnline())
	{
		try{
			create_task(
				TrySetRichPresenceAsync(presenceString)
			);
		}catch(Platform::COMException^ ex)
		{
			XboxOneMabUtils::LogComment(L"set presence Service is not available");
		}
	}
}

void XboxOneMabUISessionController::ShowHelpUI()
{
	Windows::Xbox::System::User^ user = XboxOneUser->GetCurrentUser();
	if(user)
	{
		Windows::Foundation::IAsyncAction^ helpAction = Windows::Xbox::ApplicationModel::Help::Show(user);

		// Optionally, specify a completion handler.
		helpAction->Completed = ref new AsyncActionCompletedHandler(
			[](IAsyncAction^ action, Windows::Foundation::AsyncStatus status)
		{
			
		});

	}

}


bool XboxOneMabUISessionController::StringValidation(const MabString *userString)
{
	return XboxOneMabUtils::VerifyStringResult(XboxOneMabUtils::ConvertMabStringToPlatformString(userString));
}

//Send message when current user is null
bool XboxOneMabUISessionController::onUserBecomeNull(bool inGame /* =true */)
{
	Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_USER_LEFT, inGame, false, NULL ) );
	return true;
}


bool XboxOneMabUISessionController::onControllerUserAdded()
{
	Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_USER_ADDED, true, false, NULL ) );
	return true;
}


void XboxOneMabUISessionController::OnNetworkStatusChanged(bool status)
{
	if(status == true){
		XboxOneStateManager->OnResumed();
	}else{
		XboxOneStateManager->OnSuspending();
	}

}

void XboxOneMabUISessionController::OnMatchMakingFailed()
{
	Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_CREATE, false, IsMultiplayer()) );
}

Windows::Foundation::IAsyncAction^ XboxOneMabUISessionController::TrySetRichPresenceAsync(Platform::String^ presenceString)
{
	return create_async([this,presenceString]()
	{
		try
		{
			XboxLiveContext^ xboxLiveContext = XboxOneUser->GetCurrentXboxLiveContext();
			if(xboxLiveContext == nullptr && XboxOneUser->GetCurrentUser() == nullptr)
				return;

			Presence::PresenceData^ presenceData = ref new Presence::PresenceData(
				XboxLiveConfiguration::PrimaryServiceConfigId,
				presenceString
				);
			IAsyncAction^ setPresenceAction = xboxLiveContext->PresenceService->SetPresenceAsync(
				true,
				presenceData
				);
			create_task( setPresenceAction )
				.then([] ()
			{
				try
				{
					XboxOneMabUtils::LogComment( L"SetPresenceAsync Done" );
				}
				catch (Platform::COMException^ ex)
				{
					XboxOneMabUtils::LogComment(L"failed to set presence service inside");
				}
			}).wait();
			XboxOneMabUtils::LogComment(L"Success set presence");
		}catch(Platform::COMException^ ex)
		{
			XboxOneMabUtils::LogComment(L"failed to set presence service outside");
		}
	});
}

