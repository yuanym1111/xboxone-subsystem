#include "pch.h"

#include "XboxOneMabAchievementManager.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUserInfo.h"

using namespace concurrency;
using namespace Platform;
using namespace Windows::Data;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Services;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Details;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Achievements;


XboxOneMabAchievementManager::XboxOneMabAchievementManager(int _num_achievements)
{
	 m_titleId = wcstoul( Windows::Xbox::Services::XboxLiveConfiguration::TitleId->Data(), nullptr, 10 ); 
	 m_serviceConfigId = XboxLiveConfiguration::PrimaryServiceConfigId;
	 m_num_achievements = _num_achievements;
	 m_total_achievements_count = 0;
	 m_total_unlocked_achievements_count = 0;
	 m_total_locked_achievements_count = 0;
	  //Register the Stats
	 //Register the Stats Sample ETW+ Provider
	 //Enable it when the Event registration has been added to the project
	 //TODO: Switch it on
	 ULONG result = ERROR;
	 m_achievementList = ref new Vector<Achievement^>();
	 m_achievements_in_progress.reserve(20);
	 m_descriptionDataList.clear();
	 for( int i = 0; i < XBOX_ONE_MAX_USERS; ++i )
	 {
		 awarded_map[ i ] = 0uLL;
	 }

	 //ULONG result = EventRegisterHESL_6EF2533A();

	 if ( result == ERROR_SUCCESS )
	 {
		 XboxOneMabUtils::LogComment(L"Event register succeeded.\n");

	 }
	 else
	 {
		 XboxOneMabUtils::LogComment(L"Event register failed.\n");
		 MABASSERT(result == ERROR);

	 }
}


XboxOneMabAchievementManager::~XboxOneMabAchievementManager()
{

}


void XboxOneMabAchievementManager::Initilize()
{
	//Never used?
	if(m_num_achievements > 0){
		Reinitialise(m_num_achievements);
	}
}


bool XboxOneMabAchievementManager::Reinitialise(int _num_achievements)
{
	m_num_achievements = _num_achievements;
	m_achievementList->Clear();

	for ( int i = 0; i < XBOX_ONE_MAX_USERS; ++i )
	{
		have_achievements_been_loaded_once.push_back( false );
	}

	for( int i = 0; i < m_num_achievements; ++i ){
		//		m_achievementList->Append(ref new Achievement());
	}

	return true;

}


bool XboxOneMabAchievementManager::SetUser(User^ _user)
{
	if(_user != nullptr){
		m_user = _user; 
		m_xboxUserId = _user->XboxUserId;
		return true;
	}
	return false;
}

bool XboxOneMabAchievementManager::SetLiveContext(XboxLiveContext^ _liveContext)
{
	if(_liveContext != nullptr){
		m_xboxLiveContext = _liveContext;
		return true;
	}
	return false;
}

void XboxOneMabAchievementManager::RequestAchievementsForTitleId(int controller_index)
{
	MABUNUSED(controller_index);
	if(m_xboxLiveContext == nullptr)
	{
		XboxOneMabUtils::LogComment(L"Xbox Live Context is null.\n");
		return;
	}

	int maxItems = 10;

	auto pAsyncOp = m_xboxLiveContext->AchievementService->GetAchievementsForTitleIdAsync(
		m_xboxUserId,                           // Xbox LIVE user Id
		m_titleId,                                    // Title Id to get achievement data for
		AchievementType::All,                       // AchievementType filter: All mean to get Persistent and Challenge achievements
		false,                                      // All possible achievements including accurate unlocked data
		AchievementOrderBy::TitleId,                // AchievementOrderBy filter: Default means no particular order
		0,                                  // The number of achievement items to skip
		maxItems                                    // The maximum number of achievment items to return in the response
		);

	create_task( pAsyncOp )
		.then( [this, maxItems] (task<AchievementsResult^> achievementsTask)
	{
		try
		{
			AchievementsResult^ achievements = achievementsTask.get();

			if (achievements != nullptr && achievements->Items != nullptr)
			{
				for each (Achievement^ member in achievements->Items)
				{
					m_achievementList->Append(member);
					m_num_achievements++;
					LoadAchievementData(member);
				}
			}

			// Get next page until the end
			AchievementsResult^ lastResult = achievements;
			while(lastResult != nullptr)
			{
				// Note: GetNextAsync will throw an exception with HResult INET_E_DATA_NOT_AVAILABLE
				// when there are no more items to fetch. This exception will be thrown when the task
				// is created in GetNextAsync() as opposed to when it's executed.
				// This behavior may be a little unintuitive because it's different than how
				// GetAchievementsForTitleIdAsync behaves
				try{
					create_task(lastResult->GetNextAsync(maxItems))
						.then([this, &lastResult](AchievementsResult^ achievements)
					{
						lastResult = achievements;
						if (achievements != nullptr && achievements->Items != nullptr)
						{
							for each (Achievement^ member in achievements->Items)
							{
								m_achievementList->Append(member);
								m_num_achievements++;
								LoadAchievementData(member);
							}
						}

					}).wait();
				}catch(Platform::COMException^ ex){
					if(ex->HResult == INET_E_DATA_NOT_AVAILABLE)
						lastResult = nullptr;
					//no more items to fetch
				}
			}
		}
		catch (Platform::Exception^ ex)
		{
			if (ex->HResult == INET_E_DATA_NOT_AVAILABLE)
			{
				// we hit the end of the achievements
			}
			else
			{
				XboxOneMabUtils::LogCommentFormat(L"GetAchievementsForTitleIdAsync failed.",ex);
			}
		}

	});


}

void XboxOneMabAchievementManager::RequestAchievement(int controller_index, Platform::String^ achievementId)
{
	MABUNUSED(controller_index);

	if(m_xboxLiveContext == nullptr)
	{
		XboxOneMabUtils::LogComment(L"Xbox Live Context is null.\n");
		return;
	}

	if(m_serviceConfigId == nullptr)
	{
		XboxOneMabUtils::LogComment(L"Xbox Service Configuration is null.\n");
		return;
		auto pAsyncOp = m_xboxLiveContext->AchievementService->GetAchievementAsync(
			m_xboxUserId,       // Xbox LIVE user Id
			m_serviceConfigId, // Achievements are associated with a service configuration Id
			achievementId           // The achievement Id of the achievement being requested
			);

		create_task( pAsyncOp )
			.then( [this] (task<Achievement^> achievementTask)
		{  
			try
			{
				Achievement^ achievement = achievementTask.get();
				if (achievement != nullptr)
				{
					//Do Something here for this record
				}
			}
			catch (Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogComment(L"Failed to get this achivement by the given ID");
			}
		});

	}
}

void XboxOneMabAchievementManager::ClearAchievementDataForController(int controller_index)
{
	MABUNUSED(controller_index);
	m_descriptionDataList.clear();
	m_displayDataList.clear();
	m_scoreDataList.clear();
	m_achievements_in_progress.clear();
}

void XboxOneMabAchievementManager::LoadAchievementData(IVectorView<Achievement^>^ achievementList)
{
	for( UINT index = 0; index < achievementList->Size; index++ )
	{
		Achievement^ ach = achievementList->GetAt(index);
		LoadAchievementData(ach);
	}

}

void XboxOneMabAchievementManager::LoadAchievementData(Achievement^ achievement)
{

	if (achievement->ProgressState == AchievementProgressState::Achieved)
	{
		m_descriptionDataList.push_back( std::pair< Platform::String^, Platform::String^ > (achievement->Id,achievement->UnlockedDescription));
	}
	else
	{
		m_descriptionDataList.push_back( std::pair< Platform::String^, Platform::String^ > (achievement->Id,achievement->LockedDescription));
		m_achievements_in_progress.push_back(achievement->Id);
	}

	//TODO: Rewards will be a list, the code only checked the first one with a gamescore type
	if(achievement->Rewards->GetAt(0)->RewardType == AchievementRewardType::Gamerscore)
	{
		m_scoreDataList.push_back(std::pair< Platform::String^, int >(achievement->Id, _wtoi(achievement->Rewards->GetAt(0)->Data->Data())) );
	}else{
		m_scoreDataList.push_back(std::pair< Platform::String^, int >(achievement->Id, 0) );
	}

	m_displayDataList.push_back( std::pair< Platform::String^, Platform::String^ >(achievement->Id,achievement->Name));

}

void XboxOneMabAchievementManager::UserChanged()
{
	ClearAchievementDataForController(0);
}

void XboxOneMabAchievementManager::GetAchievementsCountForCurrentUser()
{
	Concurrency::critical_section::scoped_lock lock(m_achievementDataLock);

	XboxLiveContext^ context = XboxOneUser->GetCurrentXboxLiveContext();
	if (context == nullptr) 
	{
		return;
	}

	m_total_achievements_count = 0;
	m_total_unlocked_achievements_count = 0;
	m_total_locked_achievements_count = 0;

	User^ user = XboxOneUser->GetCurrentUser();
	UINT first_item_index = 0;
	UINT* first_item_index_ptr = &first_item_index;
	UINT skip_item_pace = 10;
	int count = 0;
	int *count_ptr = &count;

	do 
	{
		auto pAsyncOp = context->AchievementService->GetAchievementsForTitleIdAsync(
			user->XboxUserId,                       // Xbox LIVE user Id
			RUGBY_CHALLENGE_3_TITLE_ID,             // Title Id to get achievement data for
			AchievementType::All,                   // AchievementType filter: All mean to get Persistent and Challenge achievements
			false,                                  // All possible achievements including accurate unlocked data
			AchievementOrderBy::Default,            // AchievementOrderBy filter: Default means no particular order
			first_item_index,						// The number of achievement items to skip
			10										// The maximum number of achievment items to return in the response
			);


		create_task( pAsyncOp )
		.then( [this,count_ptr, first_item_index_ptr] (task<AchievementsResult^> achievementsTask)
		{
			try
			{
				AchievementsResult^ achievements = achievementsTask.get();

				if (achievements != nullptr && achievements->Items != nullptr)
				{
					*count_ptr += achievements->Items->Size;

					for (UINT index = 0; index < achievements->Items->Size; index++)
					{
						Achievement^ item = achievements->Items->GetAt(index);
						XboxOneMabUtils::LogComment(L"   Name: " + item->Name );

						if (item->ProgressState == AchievementProgressState::Achieved)
						{
							XboxOneMabUtils::LogComment(L"     Unlocked Description:   " + item->UnlockedDescription);
							m_total_unlocked_achievements_count++;
						}
						else
						{
							XboxOneMabUtils::LogComment(L"     Locked Description:   " + item->LockedDescription);
							m_total_locked_achievements_count++;
						}
					}
				}
			}
			catch (Platform::Exception^ ex)
			{
				if (ex->HResult == INET_E_DATA_NOT_AVAILABLE)
				{
					// we hit the end of the achievements
				}
				else
				{
					XboxOneMabUtils::LogComment(
						L"GetAchievementsForTitleIdAsync failed.");
				}
			}

		}).wait();

		if (count - first_item_index < 10) 
		{
			break;
		}
		else
		{
			first_item_index += 10;
			continue;
		}
	} while(1);

	m_total_achievements_count = count;
	MABLOGDEBUG("count number is %d", count);
}

void XboxOneMabAchievementManager::GetAchievementsProgressForCurrentUser(float& percent)
{
	if( m_total_achievements_count == 0) 
	{
		percent = 0.0f;
	}
	else
	{
		percent = ((float)m_total_unlocked_achievements_count / (float)m_total_achievements_count) * 100;
	}

}

void XboxOneMabAchievementManager::RefreshAchievementProgress()
{
	GetAchievementsCountForCurrentUser();
}

int XboxOneMabAchievementManager::GetAchievementGamerscore(Platform::String^ achievement_id) const
{
	for ( MabVector< std::pair< Platform::String^, int > >::const_iterator iter = m_scoreDataList.begin(); iter != m_scoreDataList.end(); ++iter )
	{
		if( iter->first == achievement_id ) return  iter->second;
	}
	return false;
}

void XboxOneMabAchievementManager::GetAchievementDisplayString(Platform::String^ achievement_id, MabString& string_out) const
{
	for ( MabVector< std::pair< Platform::String^, Platform::String^ > >::const_iterator iter = m_displayDataList.begin(); iter != m_displayDataList.end(); ++iter )
	{
		if( iter->first == achievement_id ){
			XboxOneMabUtils::ConvertPlatformStringToMabString(iter->second, string_out);
			return;
		}
	}
	string_out = MabString();
}

void XboxOneMabAchievementManager::GetAchievementDescriptionString(Platform::String^ achievement_id, MabString& string_out) const
{
	for ( MabVector< std::pair< Platform::String^, Platform::String^ > >::const_iterator iter = m_descriptionDataList.begin(); iter != m_descriptionDataList.end(); ++iter )
	{
		if( iter->first == achievement_id ){
			XboxOneMabUtils::ConvertPlatformStringToMabString(iter->second, string_out);
			return;
		}
	}
	string_out = MabString();
}

bool XboxOneMabAchievementManager::IsAchievementObtained(int controller_index, Platform::String^ achievement_id) const
{
	MABUNUSED(controller_index);
	for ( MabVector< Platform::String^ >::const_iterator iter = m_achievements_in_progress.begin(); iter != m_achievements_in_progress.end(); ++iter )
	{
		if( *iter == achievement_id ) return  true;
	}
	return false;
}