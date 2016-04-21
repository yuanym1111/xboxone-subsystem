/**
 * @file XboxOneMabMatchMaking.cpp
 * @Matchmaking logic.
 * @author jaydens
 * @22/10/2014
 */

#include "pch.h"
#include <wrl\event.h>
#include <xstudio.h>
#include <queue>
#include <collection.h>

#include "MabLog.h"
#include "MabXboxOneController.h"

#include "XboxOneMabMatchMaking.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabPartyManager.h"  //Should be removed later, only include XboxOneStateManager, party api will not be used later
#include "XboxOneMabStateManager.h"
#include "XboxOneMabUISessionController.h"
#include "XboxOneMabStatisticsManager.h"
#include "XboxOneMabAchievementManager.h"
#include "XboxOneMabEventHandler.h"
#include "XboxOneMabConnectedStorageManager.h"
#include "XboxOneMabUISocialService.h"

#include "XboxOneMabGame.h"

using namespace Concurrency;
using namespace Platform;
using namespace Windows::Data::Json;
using namespace Windows::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::ApplicationModel;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::System;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace Windows::Xbox::UI;

using namespace Windows::UI::Core;

using namespace XboxOneMabLib;

static User^ previoud_user = nullptr;

void XboxOneMabMatchMaking::initialize()
{
	XboxOneMabUserInfo::Create();
	XboxOneMabSessionManager::Create();
	XboxOneMabGame::Create();
	XboxOneMabStateManager::Create();
	XboxOneMabUISessionController::Create();
	XboxOneMabStatisticsManager::Create();
	XboxOneMabAchievementManager::Create();
	XboxOneMabStateManager::Get()->Initialize();
	XboxOneMabConnectedStorageManager::Create();
	XboxOneMabConnectedStorageManager::Get()->Initialize();

	XboxOneMabMatchMaking::Get()->ClearAutoStartGame();
}

void XboxOneMabMatchMaking::SetupEventHandler()
{
    // Register user network status change event
    Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged += 
        ref new Windows::Networking::Connectivity::NetworkStatusChangedEventHandler(
        [this] ( Platform::Object^ ) 
	{
		OnNetworkStatusChange();
	});
}

void XboxOneMabMatchMaking::OnNetworkStatusChange()
{
    auto connectionLevel = Windows::Networking::Connectivity::NetworkConnectivityLevel::None;
    auto internetProfile = Windows::Networking::Connectivity::NetworkInformation::GetInternetConnectionProfile();
    if (internetProfile != nullptr)
    {
        connectionLevel = internetProfile->GetNetworkConnectivityLevel();
    }
    XboxOneMabUtils::LogComment(L"Connection Level Now " + connectionLevel.ToString());

    if (connectionLevel != Windows::Networking::Connectivity::NetworkConnectivityLevel::XboxLiveAccess)
    {
        // No internet connectivity
        XboxOneMabUtils::LogComment(L"Network connectivity lost");
		m_isOnline = false;
		m_multiplayChecked = false;
		m_UGCChecked = false;
		m_contentUploadChecked = false;
        // Go into a blocking error condition

		Notify(XboxOneMabMatchMakingMessage(XboxOneMabMatchMaking::XBOXONE_NETWORK_DISCONNECTED, nullptr));
    }
    else
    {
		m_isOnline = true;
		Notify(XboxOneMabMatchMakingMessage(XboxOneMabMatchMaking::XBOXONE_NETWORK_CONNECTED, nullptr));
    }
}

bool XboxOneMabMatchMaking::CreateGame(int gamemode, bool bCreateMatchSession)
{
	XboxOneMabStateManager::Get()->SelectSessionMode(gamemode);
	XboxOneStateManager->SwitchState(GameState::CreateLobby);
	return true;

}

//Call this function when leave quick match
bool XboxOneMabMatchMaking::leave(bool networkStatus /*true*/)
{
	//Check if I'm still in matchmaking state, I need cancel this ticket when left
	critical_section::scoped_lock lock(m_processLock);
	XboxOneMabLib::GameState mystate = XboxOneStateManager->GetMyState();
	MABLOGDEBUG("My state when I want to leave the game is: %d",mystate);
	if(XboxOneSessionManager->GetGameSession() == nullptr){
		MABLOGDEBUG("Game session is already null");
	}
	//If I'm still on a leaving process
	if(mystate == GameState::leavingMultiplayer)
	{
		return false;
	}
	if(XboxOneGame->GetCurrentUser() != nullptr && networkStatus == true){

		if(mystate == GameState::Matchmaking){
			XboxOneStateManager->CancelMatchmaking();
			return true;
		}

		//If the game is started already
		if(mystate == GameState::MultiplayerGame)
		{
			//Leave from a match
			XboxOneStateManager->ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
			return true;
		}

		//If else where
		if( XboxOneSessionManager->IsPlayerInSession(XboxOneGame->GetCurrentUser()->XboxUserId,XboxOneSessionManager->GetGameSession()))
		{	
			XboxOneStateManager->ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
			return true;
		}else
		{
			XboxOneStateManager->ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus,false);
		}
	}else{
		XboxOneStateManager->ResetMultiplayer(false);
	}
	return false;  

}

//This invite will invoke Microsoft's UI to list people and send invite. Under testing.
//This call is only worked to send invitation to join party not game!
bool XboxOneMabMatchMaking::invite()
{
#if 1
	XboxOneMabUserInfo* userInfo = XboxOneMabUserInfo::Get();

    User^ user = userInfo->GetCurrentUser();
    IAsyncAction^ partyOperation = Windows::Xbox::UI::SystemUI::ShowSendInvitesAsync( user );

    create_task(partyOperation)
        .then([] (task<void> t)
    {
		try
		{
			t.get(); // if t.get() didn't throw, it succeeded
			XboxOneMabUtils::LogComment(L"Friend invited into party");
		}
		catch(Platform::Exception^ ex)
		{
            XboxOneMabUtils::LogCommentWithError( L"InviteToPartyAsync failed", ex->HResult );
		}

    }).wait();
#endif
	return true;
}

void XboxOneMabMatchMaking::CheckPrivilege(uint32 priviledgeID, Platform::String^ reason, bool& privilege)
{

	if(XboxOneMabUserInfo::Get()->GetCurrentUser() == nullptr || XboxOneMabUserInfo::Get()->GetCurrentUser()->IsSignedIn == false)
	{
		privilege = false;
		return;
	}


	if(XboxOneMabUISocialService::GetXboxOneAccountTier(XboxOneMabUserInfo::Get()->GetCurrentUser()->XboxUserId) < XBOX_ONE_ACCOUNT_TIER_GOLD)
	{
		privilege = false;
//		return;
	}

	//  attemptResolution?will show UI; false just reports the user state.
	bool attemptResolution = true;
//	XboxOneMabUtils::LogComment(" XboxOneMabMatchMaking::CheckPrivilege");
    auto AsyncOp = Store::Product::CheckPrivilegeAsync(
        XboxOneMabUserInfo::Get()->GetCurrentUser(), 
        priviledgeID, 
        attemptResolution, 
        reason);
	try{
		create_task( AsyncOp ).then( [this, &privilege] ( task<Store::PrivilegeCheckResult> privilegeCheckTask )
		{
			try
			{
				auto priv_check = privilegeCheckTask.get();

				if (priv_check != Windows::Xbox::ApplicationModel::Store::PrivilegeCheckResult::NoIssue){
					XboxOneMabUtils::LogCommentFormat(L"   =PrivilegeCheckResult code: %d\n", priv_check);
					m_userPrivillege = false;
					privilege = false;
				}else{
					XboxOneMabUtils::LogComment(L"PrivilegeCheck succeeded\n");
					m_userPrivillege = true;
					privilege = true;
				}
			}
			catch ( Platform::Exception^ ex )
			{
				XboxOneMabUtils::LogCommentFormat( L"CheckPrivilege: %S\n", ex->Message );
				privilege = false;
			}
		}).wait();
		}catch( Platform::Exception^ ex )
		{
			privilege = false;
		}
}


bool XboxOneMabMatchMaking::CheckMultiplayerPrivilege()
{
	critical_section::scoped_lock lock(m_processLock);
	if(m_multiplayChecked)
		return m_multiplayPrivilege;
	m_multiplayChecked = true;
	CheckPrivilege(XPRIVILEGE_MULTIPLAYER_SESSIONS,MULTIPLAYER_RESTRICTION, m_multiplayPrivilege);
	if(m_multiplayPrivilege)
		XboxOneSessionManager->SetXboxLiveContext(XboxOneUser->GetCurrentXboxLiveContext());
	return m_multiplayPrivilege;
}

bool XboxOneMabMatchMaking::CheckViewOthersPrivilege()
{
	m_UGCChecked = true;
	CheckPrivilege(XPRIVILEGE_USER_CREATED_CONTENT, SHOW_OTHERS_RESTRICTION, m_UGCprivilege);
	return m_UGCprivilege;
}

bool XboxOneMabMatchMaking::GetViewOthersPrivilege()
{
	if(m_UGCChecked)
		return m_UGCprivilege;
	m_UGCChecked = true;
	CheckPrivilege(XPRIVILEGE_USER_CREATED_CONTENT, SHOW_OTHERS_RESTRICTION, m_UGCprivilege);
	return m_UGCprivilege;
}

bool XboxOneMabMatchMaking::CheckContentUploadPrivilege()
{
	/*
	if(m_contentUploadChecked)
		return m_contentUploadPrivilege;
	m_contentUploadChecked = true;
	*/
	CheckPrivilege(XPRIVILEGE_SHARE_CONTENT,UPLOAD_RESTRICTION,m_contentUploadPrivilege);
	return m_contentUploadPrivilege;
}




void XboxOneMabMatchMaking::ShowAccountPicker()
{
	MABLOGDEBUG("Show account picker.");
	auto AsyncOp = Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync( MabControllerManager::GetLastGamepadInput(), Windows::Xbox::UI::AccountPickerOptions::None );
	create_task( AsyncOp ).then( [this] ( task<AccountPickerResult^> AccountPickerTask )
	{
		try
		{
			auto account_check = AccountPickerTask.get();
			if (account_check->User != nullptr){
				XboxOneMabUtils::LogComment(L"Login successful.");
				XboxOneUser->SetUser( (Windows::Xbox::System::User^) account_check->User);

				//XboxOneStatistics->sendMultiplayerRoundEnd(0,0,100,120.0f,0);
				Concurrency::create_async([this]() -> void
				{
					float progress = 0.0f;
					XboxOneAchievement->RefreshAchievementProgress();
					XboxOneAchievement->GetAchievementsProgressForCurrentUser(progress);
					XboxOneStatistics->sendGameProgress(progress);

				});
			}
			else
			{
				MABLOGDEBUG("AccountPicker fail to get signin user");
				ShowAccountPicker();
			}

		}
		catch ( Platform::Exception^ ex )
		{
			XboxOneMabUtils::LogCommentFormat( L"Show account picker: %S\n", ex->Message );
		}
		catch(Concurrency::task_canceled& ex)
		{
			XboxOneMabUtils::LogCommentFormat( L"Show account picker is cancelled" );
			ShowAccountPicker();
		}
	}).wait();

}

void XboxOneMabMatchMaking::ShowAccountPickerPending(bool& isPicked)
{
	MABLOGDEBUG("Show account picker pending.");
	isPicked = false;
	if(previoud_user != nullptr) return;

    auto AsyncOp = Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync(MabControllerManager::GetLastGamepadInput(), Windows::Xbox::UI::AccountPickerOptions::None);

    create_task( AsyncOp ).then( [this, &isPicked] ( task<AccountPickerResult^> AccountPickerTask )
    {
        try
        {
            auto account_check = AccountPickerTask.get();
			if (account_check->User != nullptr)
			{
				if (!account_check->User->IsGuest)
				{
					XboxOneMabUtils::LogComment(L"Login successful.");
					m_userId = account_check->User->Id;
					XboxOneUser->SetUser( (Windows::Xbox::System::User^) account_check->User);
					m_multiplayChecked = false;
					m_UGCChecked = false;
					m_voiceChecked = false;
					m_contentUploadChecked = false;
					m_UGCprivilege = false;
					m_multiplayPrivilege = false;
					m_voicePrivilege = false;
					m_contentUploadPrivilege = false;

					//XboxOneStatistics->sendMultiplayerRoundEnd(0,0,100,120.0f,0);
					Concurrency::create_async([this]() -> void
					{
						float progress = 0.0f;
						XboxOneAchievement->RefreshAchievementProgress();
						XboxOneAchievement->GetAchievementsProgressForCurrentUser(progress);
						XboxOneStatistics->sendGameProgress(progress);
					});
					isPicked = true;
				}
				else
				{
					XboxOneMabUtils::LogComment(L"Login successful but GUEST is not permitted.");
					XboxOneMessageController->onUserBecomeNull(false);
					isPicked = false;
				}

			}
			else
			{
				MABLOGDEBUG("AccountPicker fail to get signin user");
				XboxOneMessageController->onUserBecomeNull(false);
				isPicked = false;
			}

        }
        catch ( Platform::Exception^ ex )
        {
            XboxOneMabUtils::LogCommentFormat( L"ShowAccountPickerPending: %S\n", ex->Message );
			isPicked = false;
        }
		catch(Concurrency::task_canceled& ex)
		{
			XboxOneMabUtils::LogCommentFormat( L"ShowAccountPickerPending is cancelled" );
			XboxOneMessageController->onUserBecomeNull(false);
			isPicked = false;
		}
    }).wait();
}

void XboxOneMabMatchMaking::ShowAccountPickerExtra()
{
	MABLOGDEBUG("Show account picker extra.");

    auto AsyncOp = Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync(MabControllerManager::GetLastGamepadInput(), Windows::Xbox::UI::AccountPickerOptions::AllowGuests);

    create_task( AsyncOp ).then( [this] ( task<AccountPickerResult^> AccountPickerTask )
    {
        try
        {
            auto account_check = AccountPickerTask.get();
			if (account_check->User != nullptr)
			{
				XboxOneUser->SetUser( (Windows::Xbox::System::User^) account_check->User);
				if ((Windows::Xbox::System::User^)account_check->User != XboxOneUser->GetCurrentUser())
				{
					m_userId = account_check->User->Id;
					previoud_user = (Windows::Xbox::System::User^) account_check->User;

					m_multiplayChecked = false;
					m_UGCChecked = false;
					m_voiceChecked = false;
					m_contentUploadChecked = false;
					m_UGCprivilege = false;
					m_multiplayPrivilege = false;
					m_voicePrivilege = false;
					m_contentUploadPrivilege = false;

					float progress = 0.0f;
					XboxOneAchievement->RefreshAchievementProgress();
					XboxOneAchievement->GetAchievementsProgressForCurrentUser(progress);
					XboxOneStatistics->sendGameProgress(progress);
					//XboxOneStatistics->sendMultiplayerRoundEnd(0,0,100,120.0f,0);
				}
			}
			else
			{
				MABLOGDEBUG("AccountPicker fail to get signin user");
			}

        }
        catch ( Platform::Exception^ ex )
        {
            XboxOneMabUtils::LogCommentFormat( L"ShowAccountPickerPending: %S\n", ex->Message );
        }
    }).wait();

}
bool XboxOneMabMatchMaking::JoinParty(wchar_t* partyID)
{
	MABUNUSED(partyID);

//	XboxOneMabPartyManager::Get()->JoinFriendsParty(XboxOneMabUtils::FormatString(partyID));
	XboxOneStateManager->SwitchState(GameState::VerifyPrivileges);
	return true;
}

bool XboxOneMabMatchMaking::HasSignedInUser()
{
	return XboxOneMabUserInfo::Get()->HasSignedInUser();
}

void XboxOneMabMatchMaking::ResetGameplayData()
{

}

void XboxOneMabMatchMaking::DeleteAllPlayerStates()
{

}

void XboxOneMabMatchMaking::Update()
{
	XboxOneStateManager->Update();
}

bool XboxOneMabMatchMaking::XboxOneStateLauchQuickMatch(int gamemode /*= 0*/, bool bShowTeamSelection /*= true*/)
{

	//To make quick match, there must be a non-guest signed-in user.
	MABLOGDEBUG("Call lauch quick match state is: %d", XboxOneStateManager->GetMyState());
	if(!HasSignedInUser() ) {
//		ShowAccountPicker();
		return false;
	}

		//Should put to a task here!,try to sleep for a long time to avoid timing issue
		create_task([this,gamemode,bShowTeamSelection]()
		{
			if(bShowTeamSelection == false)
			{
				if(XboxOneStateManager->GetMyState() == GameState::leavingMultiplayer){
					//Some hack for the added step, wait until the leaving multiplayer finished
					while(XboxOneStateManager->GetMyState() != GameState::MultiplayerMenus)
					{
						Sleep(1000);
					}
					XboxOneMabStateManager::Get()->SelectSessionMode(gamemode, bShowTeamSelection);
					XboxOneStateManager->SwitchState(GameState::Matchmaking);
					return;
				}else if(XboxOneStateManager->GetMyState() != GameState::Lobby){
					return;
				}
				leave();
				while(XboxOneStateManager->GetMyState() != GameState::MultiplayerMenus)
				{
					Sleep(1000);
				}
			}
			XboxOneMabStateManager::Get()->SelectSessionMode(gamemode, bShowTeamSelection);
			XboxOneStateManager->SwitchState(GameState::Matchmaking);
		});
		return true;

}

bool XboxOneMabMatchMaking::isPlayerHost()
{
	return XboxOneSessionManager->IsGameHost();
}

bool XboxOneMabMatchMaking::ReSet()
{
	XboxOneStateManager->Reset();
	return true;
}

void XboxOneMabMatchMaking::ShowSendGameInvite(int gamemode /* =0 */)
{

	// show send invite
	if(XboxOneMabNetGame->IsNetworkOnline())
	{
		create_async([gamemode]()
		{
			try
			{
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^  sessionRef = XboxOneSessionManager->GetLobbySession()->SessionReference;

				Platform::String^ m_sessionName = sessionRef->SessionName;
				Platform::String^ m_sessionConfigID = sessionRef->ServiceConfigurationId;
				Platform::String^ m_sessionTempID = sessionRef->SessionTemplateName;
				XboxOneMabSessionRef^ checkedsessionRef = ref new XboxOneMabSessionRef(m_sessionConfigID,m_sessionName, m_sessionTempID);
		//		IMultiplayerSessionReference^ needParam = ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference(nullptr,nullptr,nullptr);
				//Platform::String^ sessionName = needParam->SessionName;
				//It should be set from either Seven or Fifteen Game
				//Platform::String^ gameType = L"GAMETYPE_";
				Platform::String^ gameType = XboxOneMabUtils::FormatString(L"gametype=GAMETYPE_%d",gamemode);

				create_task(Windows::Xbox::UI::SystemUI::ShowSendGameInvitesAsync(
					XboxOneGame->GetCurrentUser(),
					checkedsessionRef,
					gameType)
					).then([](task<void> t)
				{
					try
					{
						t.get();
					}
					catch (Platform::Exception^ ex)
					{
						XboxOneMabUtils::LogCommentFormat(L"ShowSendGameInvitesAsync task threw exception (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
					}
				}).wait();
		
		
			}
			catch (Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"ShowSendGameInvitesAsync threw exception (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
			}
		});
	}

}

void XboxOneMabMatchMaking::ProcessProtocalActivate(Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs^ args)
{
	auto info = XboxOneSessionManager->GetMultiplayerActivationInfo(args);
	if(info == nullptr){
		Notify(XboxOneMabMatchMakingMessage(XboxOneMabMatchMaking::XBOXONE_NETWORK_MATCH_ERROR, nullptr));
		return;
	}
	XboxOneStateManager->ProcessJoinGameFromStart(info);
}

bool XboxOneMabMatchMaking::HasInvitePending()
{
	//TODO: Is it right when left game?
	return XboxOneGame->IsInvitationAccepted();
}

int XboxOneMabMatchMaking::GetTotalPlayerInSession()
{
	auto session  = XboxOneSessionManager->GetGameSession();
	if(session != nullptr){
		return session->Members->Size;
	}else{
		return 0;
	}
}

bool XboxOneMabMatchMaking::HasAcceptedInvite()
{
	return XboxOneGame->IsInvitationAccepted();
}

bool XboxOneMabMatchMaking::CanUserLauchInvitation(){
	return XboxOneStateManager->CanlaunchByInvitation();
}

bool XboxOneMabMatchMaking::JoinLobbyAndGame()
{
	XboxOneStateManager->SwitchState(GameState::JoinLobby);
	return true;
}

void XboxOneMabMatchMaking::destroyAll()
{
	Destroy();
	XboxOneGame->Destroy();
	XboxOneStateManager->Destroy();
	XboxOneUser->Destroy();
	XboxOneSessionManager->Destroy();
	XboxOneMessageController->Destroy();
}

int XboxOneMabMatchMaking::GetGameMode()
{
	return XboxOneStateManager->GetRUGameType();
}

void XboxOneMabMatchMaking::GetMyID(MabString& xboxId)
{
	Platform::String^ xuid = XboxOneGame->GetCurrentUser()->XboxUserId;
	XboxOneMabUtils::ConvertPlatformStringToMabString(xuid,xboxId);
}

bool XboxOneMabMatchMaking::GetControllerReady(int controller_id)
{
	return controllers_ready[controller_id] ? controllers_ready[controller_id] : false;
}

void XboxOneMabMatchMaking::SetControllerReady(int controller_id, bool value)
{
	controllers_ready[controller_id] = value;
}

void XboxOneMabMatchMaking::MatchStart()
{
	XboxOneStateManager->SwitchState(GameState::MultiplayerGame);
}

bool XboxOneMabMatchMaking::isInLobby()
{
	return XboxOneStateManager->IsInLobby();
}

bool XboxOneMabMatchMaking::isMatchStarted()
{
	return XboxOneStateManager->IsInGame();
}

void XboxOneMabMatchMaking::ShowGameCard(MabString* targetXuid , IUser^ requestingUser /*= nullptr*/)
{
	try
	{
		create_task(Windows::Xbox::UI::SystemUI::ShowProfileCardAsync(
			XboxOneGame->GetCurrentUser(),
			XboxOneMabUtils::ConvertMabStringToPlatformString(targetXuid))
			).then([](task<void> t)
		{
			try
			{
				t.get();
			}
			catch (Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"ShowGameCard task threw exception (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
			}
		});


	}
	catch (Platform::Exception^ ex)
	{
		XboxOneMabUtils::LogCommentFormat(L"ShowGameCard threw exception (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
	}

}

void XboxOneMabMatchMaking::GetUserName(MabString& xboxUserDisplayName, const MabString* xuid /*= nullptr*/)
{
	if (xuid == nullptr)
	{
		if( XboxOneGame->GetCurrentUser()){
			Platform::String^ xuid = XboxOneGame->GetCurrentUser()->DisplayInfo->Gamertag;
			XboxOneMabUtils::ConvertPlatformStringToMabString(xuid,xboxUserDisplayName);
		}
	}else{
		xboxUserDisplayName = XboxOneSessionManager->GetSessionUserGametag(*xuid);
	}
}

void XboxOneMabMatchMaking::ValidUserSignin()
{
	//Leave it if network is down
	if(m_isOnline == false)
		return;
	XboxOneStateManager->SwitchState(GameState::AcquireUser);
}

void XboxOneMabMatchMaking::ResetMultiplayer()
{
	XboxOneStateManager->ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
}

void XboxOneMabMatchMaking::OnSuspending()
{
	m_multiplayChecked = false;
	m_UGCChecked = false;
	m_voiceChecked = false;
	m_contentUploadChecked = false;
	XboxOneStateManager->OnSuspending();
}

void XboxOneMabMatchMaking::OnResumed()
{
	XboxOneStateManager->OnResumed();
}

bool XboxOneMabMatchMaking::IsQuickMatch()
{
	return XboxOneStateManager->IsQuickMatch();
}
