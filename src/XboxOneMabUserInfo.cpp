/**
 * @file XboxOneMabUserInfo.cpp
 * @Xbox one user information and management implement..
 * @author Alex
 * @22/10/2014
 */


#include "pch.h"
#include "XboxOneMabUserInfo.h"
#include <collection.h>
#include "XboxOneMabUtils.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabVoiceManager.h"
#include "MabXboxOneController.h"
#include "XboxOneMabMatchMaking.h"
#include "XboxOneMabUISessionController.h"
#include "XboxOneMabStateManager.h"
#include "XboxOneMabSessionManager.h"

using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services;
//using namespace Microsoft::Xbox::Services;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Storage;
using namespace Windows::System;
using namespace Windows::UI::Core;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Input;

void XboxOneMabUserInfo::Initialize()
{
    SetupEventHandlers();
	gamepad_user_pair.clear();
    RefreshUserList();
	bInitialized = true;
	bIsSuspend = false;
	m_userSet = false;
	XboxOneMabMatchMaking::Get()->Attach(this);
}

void XboxOneMabUserInfo::SetupEventHandlers()
{
    // Register user sign-in event

    EventHandler<SignInCompletedEventArgs^>^ signInCompletedEvent = ref new EventHandler<SignInCompletedEventArgs^>(
        [this] (Platform::Object^, SignInCompletedEventArgs^ eventArgs)
    {
        OnSignInCompleted( eventArgs );
    });
    m_signInCompletedToken = User::SignInCompleted += signInCompletedEvent;

    // Register user sign-out event
    EventHandler<SignOutCompletedEventArgs^>^ signOutCompletedEvent = ref new EventHandler<SignOutCompletedEventArgs^>(
        [this] (Platform::Object^, SignOutCompletedEventArgs^ eventArgs)
    {
        OnSignOutCompleted( eventArgs );
    });
    m_signOutCompletedToken = User::SignOutCompleted += signOutCompletedEvent;

    EventHandler<UserAddedEventArgs^>^ userAddedEvent = ref new EventHandler<UserAddedEventArgs^>(
        [this] (Platform::Object^, UserAddedEventArgs^ eventArgs)
    {
        OnUserAdded(eventArgs);
    });
    m_userAddedToken = User::UserAdded += userAddedEvent;

    EventHandler<UserRemovedEventArgs^>^ userRemovedEvent = ref new EventHandler<UserRemovedEventArgs^>(
        [this] (Platform::Object^, UserRemovedEventArgs^ eventArgs)
    {
        OnUserRemoved(eventArgs);
    });
    m_userRemovedToken = User::UserRemoved += userRemovedEvent;
}

void XboxOneMabUserInfo::RefreshUserList(bool userAdded /* = false*/)
{

	XboxOneMabUtils::LogComment("XboxOneMabUserInfo::RefreshUserList");
    User^ currentUser = nullptr;
    User^ previousCurrentUser = nullptr;
    {
        Concurrency::critical_section::scoped_lock lock(m_lock);
        previousCurrentUser = m_currentUser;
		//If found more than one user signed in, just return

        m_userList = User::Users;

		if(m_userList->Size > 1){
			return;
		}

		m_currentUser = nullptr;
		unsigned int i;

		for (i=0; i<m_userList->Size;i++)
		{
			User^ user = m_userList->GetAt(i);
			m_userMap.insert(std::make_pair(user->XboxUserId, user));

            // Use the first signed-in non-guest account we can find.
			XboxOneMabUtils::LogCommentFormat(L"user status: %ws, %i,%i",user->XboxUserId->Data(),user->OnlineState,user->IsGuest);
            if( user->IsSignedIn  && !user->IsGuest )
            {
                if( previousCurrentUser != nullptr )
                {
                    // Try to keep the same user as we had 
                    if(_wcsicmp(previousCurrentUser->XboxUserId->Data(), user->XboxUserId->Data()) == 0 )
                    {
                        m_currentUser = previousCurrentUser;
						XboxOneGame->SetUser(user);
                    }
					else if (m_currentUser == nullptr)
					{
                        m_currentUser = user;
						XboxOneGame->SetUser(user);
					}
                }
                else
                {
                    // Use the first we find
                    if (m_currentUser == nullptr)
                    {
                        m_currentUser = user;
						XboxOneGame->SetUser(user);
                    }
                }
            }
        }

        if (m_currentUser == nullptr)
        {
            MABLOGDEBUG("Current user is null, send message to main UI, and my state is %d",XboxOneStateManager->GetMyState());
			if(XboxOneStateManager->GetMyState() > XboxOneMabLib::GameState::Initialize)
				XboxOneStateManager->SwitchState(XboxOneMabLib::GameState::AcquireUser);
        }
   
        currentUser = m_currentUser;
    }
}

//Not used anymore
void XboxOneMabUserInfo::OnSignInCompleted( 
    SignInCompletedEventArgs^ eventArgs 
    )
{
	User^ gamepad_user = eventArgs->User;

	bool active_user_changed = false;

	//If this user is not the previous user, go back to main menu

	IGamepad^ last_gamepad = MabControllerManager::GetLastGamepadInput();
	if(last_gamepad != nullptr) {
		MABLOGDEBUG("User signin detected an active Gamepad");
		if(gamepad_user_pair[last_gamepad->Id] != nullptr ) {
			if(_wcsicmp(gamepad_user->XboxUserId->Data(), m_currentUser->XboxUserId->Data()) != 0)
				active_user_changed = true;
			gamepad_user_pair[last_gamepad->Id] = gamepad_user;
		}
		else{
			//Replace previous signed in user.
			gamepad_user_pair[last_gamepad->Id] = gamepad_user;
		}
	}

	if(User::Users->Size > 1 && !active_user_changed)
	{
		XboxOneMessageController->onControllerUserAdded();
		return;
	}

	Concurrency::create_async([this, gamepad_user, active_user_changed]() -> void
	{
		Sleep(1000);
		if(m_currentUser != nullptr){
			if(active_user_changed && XboxOneStateManager->GetMyState() > XboxOneMabLib::GameState::Initialize ){
				//Not run this if not care about sign in the game
				XboxOneMessageController->onUserBecomeNull();
			}
		}
	});

}

void XboxOneMabUserInfo::OnSignOutCompleted( 
    SignOutCompletedEventArgs^ eventArgs 
    )
{
	User^ gamepad_user = eventArgs->User;
	IGamepad^ last_gamepad = MabControllerManager::GetLastGamepadInput();

	if(last_gamepad != nullptr) {
		MABLOGDEBUG("User sign out detected an active Gamepad");

		if(gamepad_user_pair[last_gamepad->Id] == nullptr ) {
			//Should not happen
			;
		}
		else {
			//Replace previous signed in user.
			gamepad_user_pair.erase(last_gamepad->Id);
			//Show account picker and prompt warning.
			if(_wcsicmp(gamepad_user->XboxUserId->Data(), m_currentUser->XboxUserId->Data()) == 0)
				XboxOneMessageController->onUserBecomeNull();
		}
	}

	if(m_currentUser != nullptr){
		if(_wcsicmp(gamepad_user->XboxUserId->Data(), m_currentUser->XboxUserId->Data()) == 0 ){
			m_currentUser = nullptr;
			XboxOneGame->SetUser(nullptr);
			XboxOneMessageController->onUserBecomeNull();
		}
	}

//	HandleUserChange();
}

void XboxOneMabUserInfo::OnUserAdded( 
    UserAddedEventArgs^ eventArgs 
    )
{
	MABASSERT(eventArgs != nullptr);
}

void XboxOneMabUserInfo::OnUserRemoved( 
    UserRemovedEventArgs^ eventArgs 
    )
{
    MABASSERT(eventArgs != nullptr);
//    HandleUserChange();
}


void XboxOneMabUserInfo::OnAudioDeviceAdded(Windows::Xbox::System::AudioDeviceAddedEventArgs^ eventArgs)
{
	XboxOneMabVoiceManager::Get()->OnUserAudioDeviceAdded(eventArgs);
}


void XboxOneMabUserInfo::HandleUserChange(bool userAdded /*= false*/)
{
    RefreshUserList(userAdded);
}

void XboxOneMabUserInfo::OnShutdown()
{

    User::UserAdded -= m_userAddedToken;
    User::UserRemoved -= m_userRemovedToken;
#if 0
    User::SignInCompleted -= m_signInCompletedToken;
#endif
    User::SignOutCompleted -= m_signOutCompletedToken;
	m_xboxLiveContextMap.clear();
}

bool XboxOneMabUserInfo::IsLocalUser(Platform::String^ xbox_user_id)
{
    Concurrency::critical_section::scoped_lock lock(m_lock);
    
	unsigned int i;

	for (i=0; i<m_userList->Size;i++)
	{
		User^ user = m_userList->GetAt(i);

		if( _wcsicmp(user->XboxUserId->Data(), xbox_user_id->Data()) == 0 )
		{
			return true;
		}
	}

    return false;
}

Windows::Xbox::System::User^ XboxOneMabUserInfo::GetCurrentUser(int controller_id /*= 0*/)
{
	MABUNUSED(controller_id);
//   Remove the lock which will block the user sign out account outside the application 
    Concurrency::critical_section::scoped_lock lock(m_lock);
    return m_currentUser;
}

void XboxOneMabUserInfo::GetUserList(std::vector<Platform::String^>& user_list) 
{ 
    Concurrency::critical_section::scoped_lock lock(m_lock);

	unsigned int i;

	for (i=0; i>m_userList->Size; i++)
	{
		User^ user = m_userList->GetAt(i);
		user_list.push_back(user->XboxUserId);
	}

}

bool XboxOneMabUserInfo::IsXboxUserIdInXboxLiveContextMap(Platform::String^ xboxUserId)
{
	if (xboxUserId->IsEmpty())
    {
        return false;
    }

    Concurrency::critical_section::scoped_lock lock(m_lock);
    for(auto iter1 = m_xboxLiveContextMap.begin(); iter1 != m_xboxLiveContextMap.end(); ++iter1)
    {
		if(_wcsicmp(iter1->first->Data(),xboxUserId->Data()) == 0)
        {
            return true;
        }
    }

    return false;
}

XboxLiveContext^ XboxOneMabUserInfo::GetXboxLiveContext(Platform::String^ xboxUserId)
{
	if (xboxUserId->IsEmpty())
    {
        return nullptr;
    }

    Concurrency::critical_section::scoped_lock lock(m_lock);
	if (m_xboxLiveContextMap.count(xboxUserId) > 0) {
		return m_xboxLiveContextMap[xboxUserId];
	}
	else {

		return nullptr;
	}
}

XboxLiveContext^ XboxOneMabUserInfo::GetXboxLiveContext(
	Platform::String^ xboxUserId, 
    bool addIfMissing
    )
{
	User^ user = m_userMap[xboxUserId];

    if (user == nullptr)
    {
        return nullptr;
    }

    XboxLiveContext^ xboxLiveContext = GetXboxLiveContext(xboxUserId);
    if (xboxLiveContext == nullptr && addIfMissing)
    {
        xboxLiveContext = ref new XboxLiveContext(user);
#ifdef _DEBUG
        // Turn on debug logging to Output debug window for Xbox Services
        xboxLiveContext->Settings->DiagnosticsTraceLevel = XboxServicesDiagnosticsTraceLevel::Verbose;

        // Show service calls from Xbox Services on the UI for easy debugging
        xboxLiveContext->Settings->EnableServiceCallRoutedEvents = true;
        xboxLiveContext->Settings->ServiceCallRouted += ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^>( 
            [=]( Platform::Object^, Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^ args )
        {
            if( args->HttpStatus != 200 )
            {
                XboxOneMabUtils::LogComment(L"");
                XboxOneMabUtils::LogComment(L"[Response]: " + args->HttpStatus.ToString() + " " + args->ResponseBody);
                XboxOneMabUtils::LogComment(L"");
            }
        });
#endif
        {
            Concurrency::critical_section::scoped_lock lock(m_lock);
            Platform::String^ xboxUserId = user->XboxUserId;

            m_xboxLiveContextMap[xboxUserId] = xboxLiveContext;
        }
    }

    return xboxLiveContext;
}

Windows::Xbox::System::User^ XboxOneMabUserInfo::GetXboxUserByStringId(Platform::String^ xbox_user_id)
{
	return m_userMap[xbox_user_id];
}

Windows::Foundation::Collections::IVectorView<User^>^ XboxOneMabUserInfo::GetMultiplayerUserList() 
{ 
    Concurrency::critical_section::scoped_lock lock(m_lock);

    Platform::Collections::Vector<User^>^ multiplayerUsers = ref new Platform::Collections::Vector<User^>();

    for (User^ user : m_userList)
    {
        if (!user->IsSignedIn || user->IsGuest)
        {
            continue;
        }
		XboxOneMabUtils::LogComment(L"Added a user into multiplayer list: " + user->XboxUserId);
        multiplayerUsers->Append(user);
    }

    return multiplayerUsers->GetView();
}

Microsoft::Xbox::Services::XboxLiveContext^ XboxOneMabUserInfo::GetCurrentXboxLiveContext()
{
	XboxOneMabUtils::LogComment(L"GetCurrentUser user is ");
    User^ user = GetCurrentUser();	
    return (user != nullptr) ? GetXboxLiveContext(user, true) : nullptr;
}

XboxLiveContext^ XboxOneMabUserInfo::GetXboxLiveContext(
    User^ user, 
    bool addIfMissing
    )
{
    if (user == nullptr)
    {
        return nullptr;
    }

    XboxLiveContext^ xboxLiveContext = GetXboxLiveContext(user->XboxUserId);
	if(xboxLiveContext != nullptr){
		XboxOneMabUtils::LogComment(L" GetXboxLiveContext user is " + user->XboxUserId);
		XboxOneMabUtils::LogComment(L" GetXboxLiveContext xboxLiveContext is " + xboxLiveContext->ToString());
	}
    if (xboxLiveContext == nullptr && addIfMissing)
    {
		try {
			//XboxLiveContext^ xboxLiveContext1 = ref new XboxLiveContext(Windows::Xbox::System::User::Users->GetAt(0));
			if( user->IsSignedIn && !user->IsGuest ) {
				xboxLiveContext = ref new XboxLiveContext(user);
			}
			//XboxLiveContext^ xboxLiveContext2 = ref new XboxLiveContext();
			//Windows::Xbox::System::User::Users->GetAt(0);
		}
		catch (Platform::COMException^ ex)
		{

			HRESULT hr = ex->HResult;
			Platform::String^ str = XboxOneMabUtils::FormatString(L" %s [0x%0.8x]", XboxOneMabUtils::ConvertHResultToErrorName(hr)->Data(), hr );
			XboxOneMabUtils::LogComment( L"System::User call failed"+ str +"||" + ex->Message);
		}

        // Turn on debug logging to Output debug window for Xbox Services
#ifdef _DEBUG
		xboxLiveContext->Settings->DiagnosticsTraceLevel = XboxServicesDiagnosticsTraceLevel::Verbose;
		xboxLiveContext->Settings->EnableServiceCallRoutedEvents = true;

        // Show service calls from Xbox Services on the UI for easy debugging
        xboxLiveContext->Settings->ServiceCallRouted += ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^>( 
            [=]( Platform::Object^, Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^ args )
        {
            if( args->HttpStatus != 200 )
            {
                XboxOneMabUtils::LogComment(L"ERROR [URL]: " + args->HttpMethod + " " + args->Url->AbsoluteUri);
                if( !args->RequestBody->IsEmpty() )
                {
                    XboxOneMabUtils::LogComment (L"[RequestBody1]: " + args->RequestBody );
                }
                XboxOneMabUtils::LogComment(L"");
                XboxOneMabUtils::LogComment(L"[Response1]: " + args->HttpStatus.ToString() + " " + args->ResponseBody);
                XboxOneMabUtils::LogComment(L"");
            }
			else {
				XboxOneMabUtils::LogComment(L"CORRECT [URL]: " + args->HttpMethod + " " + args->Url->AbsoluteUri);
				XboxOneMabUtils::LogComment(L"");
				XboxOneMabUtils::LogComment(L"[Response]: " + args->HttpStatus.ToString() + " " + args->ResponseBody);

			}


        });
		
		// (Optional.) Set up debug tracing of the Xbox Live Service call API failures to the game UI.
		Microsoft::Xbox::Services::Configuration::EnableDebugOutputEvents = true;
		Microsoft::Xbox::Services::Configuration::DebugOutput += ref new Windows::Foundation::EventHandler<Platform::String^>(
			[=]( Platform::Object^, Platform::String^ debugOutput )
			{
				// Display Xbox Live Services API debug trace to the screen for easy debugging.
				XboxOneMabUtils::LogComment( L"[XSAPI Trace]: " + debugOutput );
			});
#endif
        {
            Concurrency::critical_section::scoped_lock lock(m_lock);
            Platform::String^ xboxUserId = user->XboxUserId;
            m_xboxLiveContextMap[xboxUserId] = xboxLiveContext;
        }
    }
	MABLOGDEBUG("Getting xbox live ctx %s,%d", __FILE__, __LINE__);

    return xboxLiveContext;
}

bool XboxOneMabUserInfo::HasSignedInUser(size_t controller_id /* = 0*/)
{
	User^ user = GetCurrentUser();
	return (user != nullptr) && (user->IsSignedIn) && (user->IsGuest == false);
}

void XboxOneMabUserInfo::Update(MabObservable<XboxOneMabMatchMakingMessage>*, const XboxOneMabMatchMakingMessage &msg)
{
	XboxOneMabMatchMaking::XBOXONE_NETWORK_EVENT type = msg.GetType();

	switch(type) {
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_DISCONNECTED:
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_DOWN:
//		HandleUserChange();

		break;
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_CONNECTED:
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_RESTORE:
		HandleUserChange();
		break;
	default:
		break;
	}
}

void XboxOneMabUserInfo::SetUser(Windows::Xbox::System::User^ user)
{
	Concurrency::critical_section::scoped_lock lock(m_lock);
	m_currentUser = user;
	XboxOneGame->SetUser(user);

	for each (IController^ user_controller in user->Controllers)
	{
		gamepad_user_pair[user_controller->Id] = user;
	}
}

bool XboxOneMabUserInfo::GetUserGamertag(size_t controller_id, MabString& gametag)
{
	if(m_currentUser != nullptr)
	{
		XboxOneMabUtils::ConvertPlatformStringToMabString(m_currentUser->DisplayInfo->Gamertag,gametag);
	}else{
		gametag = "TODO";
	}
	return true;
}

//Triggered when the title is resumed from suspend state
void XboxOneMabUserInfo::OnResumed(Platform::Object^ sender, Platform::Object^ args)
{

	bIsSuspend = false;

	SetupEventHandlers();
	MABLOGDEBUG("XboxOneMabUserInfo::onResumed");
	User^ currentUser = nullptr;
	User^ previousCurrentUser = nullptr;

	m_userList = User::Users;

	//If found more than one user signed in or no user, return to account pick page
	if(m_userList->Size != 1){
		resumeUserLogin();
		return;
	}

	User^ user = m_userList->GetAt(0);
	if( pre_xuid != nullptr )
	{
		// Try to keep the same user as we had 
		if(_wcsicmp(pre_xuid->Data(), user->XboxUserId->Data()) == 0 )
		{
			m_currentUser = user;
			XboxLiveContext^ xboxLiveContext = ref new XboxLiveContext(user);
			Platform::String^ xboxUserId = user->XboxUserId;
			m_xboxLiveContextMap[xboxUserId] = xboxLiveContext;
		}
		else
		{
			resumeUserLogin();
			return;
		}
	}else{
		m_currentUser = user;
		resumeUserLogin();
	}

}

void XboxOneMabUserInfo::OnSuspending()
{
	MABLOGDEBUG("XboxOneMabUserInfo::OnSuspending");
	bIsSuspend = true;
	if(m_currentUser != nullptr)
		pre_xuid = m_currentUser->XboxUserId;
	OnShutdown();
}

Windows::Foundation::IAsyncAction^ XboxOneMabUserInfo::resumeUserLogin()
{
	return Concurrency::create_async([this]()
	{
		Sleep(8000);
		XboxOneMessageController->onUserBecomeNull();
	});
}
