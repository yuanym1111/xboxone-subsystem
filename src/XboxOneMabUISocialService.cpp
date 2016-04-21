/**
 * @file XboxOneMabUISocialService.cpp
 * @Xbox one account information and management implement..
 * @author jaydens
 * @20/03/2015
 */

#include "pch.h"
#include "XboxOneMabUISocialService.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUserInfo.h"
#include "MabXboxOneController.h"
#include <collection.h>

using namespace Platform;
using namespace Platform::Collections;
using namespace Concurrency;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Presence;
using namespace Microsoft::Xbox::Services::Social;
using namespace Windows::Xbox::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services::System;

static const std::string NOT_A_STRING = "";

XBOX_ONE_ACCOUNT_TIER_ENUM XboxOneMabUISocialService::GetXboxOneAccountTier(const std::string& xbox_user_id)
{
	Platform::String^ xbox_user_id_platform = XboxOneMabUtils::ConvertCharStringToPlatformString(xbox_user_id.c_str());
	return GetXboxOneAccountTier(xbox_user_id_platform);
}

XBOX_ONE_ACCOUNT_TIER_ENUM XboxOneMabUISocialService::GetXboxOneAccountTier(Platform::String^ xbox_user_id)
{
	XboxLiveContext^ requester = XboxOneUser->GetXboxLiveContext(xbox_user_id,true);
	if(requester == nullptr)
	{
		return UNKNOWN;
	}
	auto pAsyncOp = requester->ProfileService->GetUserProfileAsync(xbox_user_id);
	Platform::String^ gameDisplayPicResizeUri;
	XboxAccountTier account_tier;

	create_task( pAsyncOp )
		.then( [&account_tier] (task<XboxUserProfile^> resultTask)
	{
		try
		{
			XboxUserProfile^ result = resultTask.get();

			if( result->GameDisplayPictureResizeUri != nullptr )
			{
				account_tier = result->AccountTier;
#ifdef _DEBUG
				MabString print_gametag;
				Platform::String^ gametag = result->Gamertag;
				XboxOneMabUtils::ConvertPlatformStringToMabString(gametag,print_gametag);
				MABLOGDEBUG("My Gametag is %s",print_gametag.c_str());
#endif
			}
		}
		catch (Platform::Exception^ ex)
		{
			// Handle error
		}
	}).wait();

	return static_cast<XBOX_ONE_ACCOUNT_TIER_ENUM> (account_tier);
}

std::string XboxOneMabUISocialService::GetXboxOneGameDisplayName(const std::string& xbox_user_id)
{
	if (xbox_user_id.compare(NOT_A_STRING) == 0)
	{
		return NOT_A_STRING;
	}
	Platform::String^ xbox_user_id_platform = XboxOneMabUtils::ConvertCharStringToPlatformString(xbox_user_id.c_str());
	XboxLiveContext^ requester = XboxOneUser->GetXboxLiveContext(xbox_user_id_platform);
	auto pAsyncOp = requester->ProfileService->GetUserProfileAsync(xbox_user_id_platform);
	Platform::String^ gameDisplayPicResizeUri;
	std::string* game_display_name;

	create_task( pAsyncOp )
	 .then( [game_display_name] (task<XboxUserProfile^> resultTask)
	{
	  try
	 {
		XboxUserProfile^ result = resultTask.get();
    
		if( result->GameDisplayPictureResizeUri != nullptr )
		{
			char temp[2048];
			memset(temp,0,sizeof(temp));
			XboxOneMabUtils::ConvertPlatformStringToCharArray (result->GameDisplayName, temp);
			*game_display_name = temp;
		}
	  }
	 catch (Platform::Exception^ ex)
	 {
		// Handle error
	 }
	}).wait();

	return *game_display_name;
}

MabString XboxOneMabUISocialService::GetFirstActiveXboxOneUserId()
{
	Windows::Xbox::System::User^ current_user = XboxOneUser->GetCurrentUser();
	MabString result;
	if(current_user != nullptr) 
	{
		XboxOneMabUtils::ConvertPlatformStringToMabString(current_user->XboxUserId, result);
	}
	else
	{
		result = "";
	}

	return result;
}

void XboxOneMabUISocialService::DecideActiveUser()
{
	auto gamepads = Gamepad::Gamepads;
	UINT user_counter = 0;
	auto last_active_gamepad = MabControllerManager::GetLastGamepadInput();

    for( UINT i = 0; i < gamepads->Size; ++i )
    {
        auto gamepad = gamepads->GetAt( i );
        Platform::String^ gamerTag = "Unknown";

        if ( gamepad == nullptr )
        {
			continue;
		}

		if (gamepad->User != nullptr)
		{
			user_counter++;
		}
	}

	Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync( MabControllerManager::GetLastGamepadInput(), Windows::Xbox::UI::AccountPickerOptions::AllowGuests );

}

bool XboxOneMabUISocialService::XboxOneStringValidation(const MabString &str, MabString &reason)
{
	Windows::Xbox::System::User^ current_user = XboxOneUser->GetCurrentUser();
	MABASSERT(current_user != nullptr);
	XboxLiveContext^ requester = XboxOneUser->GetXboxLiveContext(current_user->XboxUserId);
	MABASSERT(requester != nullptr);
	
	Platform::String^ ps_str = XboxOneMabUtils::ConvertCharStringToPlatformString(str.c_str());;
	
	auto pAsyncOp = requester->StringService->VerifyStringAsync(ps_str);
	bool check_result = false;

	create_task( pAsyncOp )
	 .then( [&reason, &check_result] (task<VerifyStringResult^> resultTask)
	{
	  try
	 {
		VerifyStringResult^ result = resultTask.get();

		if( result->ResultCode != VerifyStringResultCode::Success )
		{
			const wchar_t* txt = result->FirstOffendingSubstring->Data();
			std::wstring ws(txt);
			std::string str(ws.begin(), ws.end());
			reason.assign(str.c_str());

			check_result = false;
		}
		else
		{
			check_result = true;
		}
	  }
	 catch (Platform::Exception^ ex)
	 {
		// Handle error
	 }
	}).wait();

	return check_result;
}

bool XboxOneMabUISocialService::XboxOneStringsValidation(const MabVector<const char*> &strings, MabVector<bool>& results)
{
	Windows::Xbox::System::User^ current_user = XboxOneUser->GetCurrentUser();
	MABASSERT(current_user != nullptr);
	XboxLiveContext^ requester = XboxOneUser->GetXboxLiveContext(current_user->XboxUserId);
	MABASSERT(requester != nullptr);
	
	IVector<Platform::String^ >^ ps_strings = ref new Vector<Platform::String^>();
	for(auto it=strings.begin(); it!=strings.end(); ++it)
	{
		Platform::String^ ps_str = XboxOneMabUtils::ConvertCharStringToPlatformString(*it);
		ps_strings->Append(ps_str);
	}
	
	auto pAsyncOp = requester->StringService->VerifyStringsAsync(ps_strings->GetView());
	bool check_result = false;
	results.assign(strings.size(), true);

	create_task( pAsyncOp )
	 .then( [&results, &check_result] (task<IVectorView<VerifyStringResult^ >^> resultTask)
	{
	  try
	 {
		IVectorView<VerifyStringResult^ >^ result = resultTask.get();

		for (int i=0; i<result->Size; ++i)
		{
			auto cur = result->GetAt(i);
			if( cur->ResultCode != VerifyStringResultCode::Success )
			{
				results[i] = false;
			}
			else
			{
				results[i] = true;
			}
		}
	  }
	 catch (Platform::Exception^ ex)
	 {
		// Handle error
	 }
	}).wait();

	return check_result;
}
