/**
 * @file XboxOneMabUserInfo.h
 * @Xbox One user information and controller class.
 * @author jaydens
 * @22/10/2014
 */

#ifndef XBOX_ONE_MAB_USER_INFO_H
#define XBOX_ONE_MAB_USER_INFO_H

class XboxOneMabMatchMakingMessage;

#include "MabString.h"
#include "XboxOneMabSingleton.h"
//#include "MabMemSTLAllocation.h"
#include <functional>
#include <memory>
#include <concurrent_queue.h>

class XboxOneMabUserInfo:public XboxOneMabSingleton<XboxOneMabUserInfo>, public MabObserver<XboxOneMabMatchMakingMessage>
{
public:
    void Initialize();
    void RefreshUserList(bool userAdded = false);
    void SetupEventHandlers();
    void OnShutdown();

	MabString AddUser();
	bool RemoveUser(Platform::String^ xbox_user_id);

	bool IsLocalUser(Platform::String^ xbox_user_id);

	void GetUserList(std::vector<Platform::String^>& user_list);
	Microsoft::Xbox::Services::XboxLiveContext^ GetCurrentXboxLiveContext();
	Microsoft::Xbox::Services::XboxLiveContext^ GetXboxLiveContext(Platform::String^ xbox_user_id);
	Microsoft::Xbox::Services::XboxLiveContext^ GetXboxLiveContext(Platform::String^ xbox_user_id, bool add_if_missing);
	Microsoft::Xbox::Services::XboxLiveContext^ GetXboxLiveContext(Windows::Xbox::System::User^ user, bool add_if_missing);
	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ GetMultiplayerUserList();

	bool IsXboxUserIdInXboxLiveContextMap(Platform::String^ xbox_user_id);
	Windows::Xbox::System::User^ GetCurrentUser(__in int controller_id = 0);
	Windows::Xbox::System::User^ GetXboxUserByStringId(Platform::String^ xbox_user_id);
    bool HasSignedInUser( size_t controller_id = 0);
	//TODO:
	bool IsUserSignedIn(size_t controller_id) { return HasSignedInUser(controller_id) ;}
	bool GetUserGamertag(size_t controller_id, MabString& gametag);
	bool IsUserGuest(size_t controller_id) { return false; }
    Platform::String^ GetCurrentXboxLiveId();

	void SetUser(Windows::Xbox::System::User^ user);

	void OnResumed(Platform::Object^ sender = nullptr, Platform::Object^ args = nullptr);
	void OnSuspending();
	inline bool IsSuspending(){return bIsSuspend;};
	
	virtual void Update(MabObservable<XboxOneMabMatchMakingMessage>*, const XboxOneMabMatchMakingMessage &msg);
	void XboxOnePopupPopulator(const MabString& title, const MabString& content);
	void SetDefaultUser() { m_defaultUser = m_currentUser; };
	bool GetDefaultUserChanged() { return (m_defaultUser == m_currentUser); }
private:

	
	bool bInitialized;
	bool bIsSuspend;
	Platform::String^ pre_xuid;

	bool isInitialized(){return bInitialized;}

	std::map<uint64, Windows::Xbox::System::User^ > gamepad_user_pair;

    void OnUserAdded(Windows::Xbox::System::UserAddedEventArgs^ eventArgs);
    void HandleUserChange(bool userAdded = false);
    void OnUserRemoved(Windows::Xbox::System::UserRemovedEventArgs^ eventArgs);
    void OnSignInCompleted( Windows::Xbox::System::SignInCompletedEventArgs^ eventArgs );
    void OnSignOutCompleted( Windows::Xbox::System::SignOutCompletedEventArgs^ eventArgs );
	void OnAudioDeviceAdded(Windows::Xbox::System::AudioDeviceAddedEventArgs^ eventArgs);

	Windows::Foundation::IAsyncAction^ resumeUserLogin();

    Windows::Xbox::System::User^ m_currentUser;
	bool m_userSet;
    Concurrency::critical_section m_lock;
    Windows::Foundation::EventRegistrationToken m_userAddedToken;
    Windows::Foundation::EventRegistrationToken m_userRemovedToken;
    Windows::Foundation::EventRegistrationToken m_signInCompletedToken;
    Windows::Foundation::EventRegistrationToken m_signOutCompletedToken;
	Windows::Foundation::EventRegistrationToken m_audioDeviceAddedToken;
	//This list contains all user list of intersted, mainly used on Achievement and statistic manager
    Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ m_userList;
	std::map<Platform::String^, Windows::Xbox::System::User^ > m_userMap;
	std::map<Platform::String^, Microsoft::Xbox::Services::XboxLiveContext^> m_xboxLiveContextMap;
    Windows::Xbox::System::User^ m_defaultUser;

};

#endif