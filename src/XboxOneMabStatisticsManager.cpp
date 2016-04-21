#include "pch.h"
#include "XboxOneMabStatisticsManager.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabMatchMaking.h"
#include "XboxOneMabEventRegManager.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabAchievementManager.h"
#include "XboxOneMabStateManager.h"

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
using namespace Microsoft::Xbox::Services::Leaderboard;
using namespace Microsoft::Xbox::Services::UserStatistics;

static bool flag = false;

XboxOneMabStatisticsManager::XboxOneMabStatisticsManager()
	: m_multiplayer_round_start_time(0.0f),
	  m_multiplayer_round_end_time(0.0f)
{
	unique_id = 0;
	m_titleId = wcstoul( Windows::Xbox::Services::XboxLiveConfiguration::TitleId->Data(), nullptr, 10 ); 
	m_serviceConfigId = XboxLiveConfiguration::PrimaryServiceConfigId;

	//Register the Stats Sample ETW+ Provider
	GenerateGUID();
	ULONG result = EventRegisterHESL_6EF2533A();
	m_achievement_map.clear();

	if ( result == ERROR_SUCCESS )
	{
		XboxOneMabUtils::LogComment( L"Event register succeeded.\n");
	}
	else
	{
		XboxOneMabUtils::LogComment( L"Event register failed.\n");
	}
}



XboxOneMabStatisticsManager::~XboxOneMabStatisticsManager()
{
#ifdef _DEBUG
	DisableDiagnostics();
#endif 
	EventUnregisterHESL_6EF2533A();
}


bool XboxOneMabStatisticsManager::SetUser(User^ _user)
{
	if(_user != nullptr){
		m_user = _user; 
		m_xboxUserId = _user->XboxUserId;
#ifdef _DEBUG
		EnableDiagnostics();
#endif
		return true;
	}
	return false;
}

#ifdef _DEBUG
void XboxOneMabStatisticsManager::EnableDiagnostics(){


	// Setup debug tracing to the Output window in Visual Studio
	// Change this to XboxServicesDiagnosticsTraceLevel::Error to only show trace failed calls, or 
	// to XboxServicesDiagnosticsTraceLevel::Off for no tracing
	m_xboxLiveContext->Settings->DiagnosticsTraceLevel = XboxServicesDiagnosticsTraceLevel::Verbose; 

	// Enable debug tracing of the Xbox Service API traffic to the game UI
	m_xboxLiveContext->Settings->EnableServiceCallRoutedEvents = true;

	m_ServiceCallRoutedEventRegistrationToken = m_xboxLiveContext->Settings->ServiceCallRouted += ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^>( 
		[this]( Platform::Object^, Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^ args )
	{
		// Using a std::weak_ptr instead of 'this' to avoid dangling pointer if caller class is released.
		// Simply unregistering the callback in the destructor isn't enough to prevent a dangling pointer

		if( this != nullptr &&
			args != nullptr &&
			args->HttpStatus >= 300 )
		{
			// Display HTTP traffic to the screen for easy debugging
			XboxOneMabUtils::LogCommentFormat( L"[URL]: %ws - %ws ",  args->HttpMethod->Data() ,  args->Url->AbsoluteUri->Data());
			XboxOneMabUtils::LogCommentFormat( L"[Response]: %ws - %ws ",  args->HttpStatus.ToString()->Data() ,  args->ResponseBody->Data());
		}
	});
}



void XboxOneMabStatisticsManager::DisableDiagnostics()
{
	if( m_xboxLiveContext != nullptr &&
		m_xboxLiveContext->Settings->EnableServiceCallRoutedEvents )
	{
		// Disable debug tracing of the Xbox Service API traffic to the game UI
		m_xboxLiveContext->Settings->EnableServiceCallRoutedEvents = false;
		m_xboxLiveContext->Settings->ServiceCallRouted -= m_ServiceCallRoutedEventRegistrationToken;
	}
}

#endif

bool XboxOneMabStatisticsManager::SetLiveContext(XboxLiveContext^ _liveContext)
{
	if(_liveContext != nullptr){
		m_xboxLiveContext = _liveContext;
		return true;
	}
	return false;
}



const XboxOneLeaderboardRowDefinitionMap& XboxOneMabStatisticsManager::GetLeaderboardFields()
{
	//TODO: hardcode some leaderboard name here
//	leaderboard_type.push_back("MatchWinned_Friends_FIFTEEN");
//	leaderboard_type.push_back("MatchWinned_Myrank_FIFTEEN");
//	leaderboard_type.push_back("MatchWinned_Global_FIFTEEN");
//	leaderboard_type.push_back("MatchWinned_Friends_SEVEN");
//	leaderboard_type.push_back("MatchWinned_Myrank_SEVEN");
//	leaderboard_type.push_back("MatchWinned_Global_SEVEN");

	leaderboard_type.push_back("MatchWinned_FIFTEEN");
	leaderboard_type.push_back("MatchWinned_SEVEN");

	int i = 0;
	for( MabVector<MabString>::iterator iter=leaderboard_type.begin(); iter!=leaderboard_type.end(); iter++, i++)
	{
		MabXBoxOneLeaderboardRowDefinition online_match_table(*iter,i);
		online_match_table.addColumn(MabXBoxOneLeaderboardColumnDefinition("ID_LB_WINS","ID_LB_WINS"));
		online_match_table.addColumn(MabXBoxOneLeaderboardColumnDefinition("ID_LB_LOSSES","ID_LB_LOSSES"));
		online_match_table.addColumn(MabXBoxOneLeaderboardColumnDefinition("ID_LB_DRAWS","ID_LB_DRAWS"));
		online_match_table.addColumn(MabXBoxOneLeaderboardColumnDefinition("ID_LB_RELIABILITY","ID_LB_RELIABILITY"));
		online_match_table.addColumn(MabXBoxOneLeaderboardColumnDefinition("ID_LB_VISIBLE_SCORE","ID_LB_VISIBLE_SCORE"));
		leaderBoardMaps[i] = online_match_table;
	}
	return leaderBoardMaps;
}



bool XboxOneMabStatisticsManager::ReadLeaderboardGlobal(DWORD _view_id, const MabVector< WORD >& _column_list, INT start_rank,  UINT max_rows /*= MAX_READ_ROWS*/, UINT* overlapped_id /*= NULL */, UINT leaderboardID /*=0*/)
{
	String^ leaderboardName  = L"RUTest";;
	if(leaderboardID == 0)
		leaderboardName = L"RUFifteensScore";
	else if(leaderboardID == 1)
		leaderboardName = L"RUSevensScore";

	 *overlapped_id = unique_id++;
	 UINT callback_id = *overlapped_id;
	 //Get max number of itmes for each asyn request

	 if( m_xboxLiveContext == nullptr )
	 {
		 throw ref new Platform::InvalidArgumentException(L"A valid User is required");
	 }
	 auto asyncOp = m_xboxLiveContext->LeaderboardService->GetLeaderboardAsync(
		 Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId,
		 leaderboardName,
		 start_rank, // skip this many ranks
		 max_rows
		 );

	 try{

		 create_task( asyncOp )
			 .then( [this, max_rows, callback_id] (task<LeaderboardResult^> resultTask) 
		 {  
			 const int countLimit = max_rows;
			 int itemCount = 0;
			 MabVector<XBoxOneStatsGetData> resultList;
			 resultList.reserve(32);
			 Vector<Platform::String^>^ userList = ref new Vector<Platform::String^>;
			 Vector<Microsoft::Xbox::Services::Social::XboxUserProfile^>^ userprofileList = ref new Vector<Microsoft::Xbox::Services::Social::XboxUserProfile^>;
			 resultList.reserve(32);
			 try
			 {

				 LeaderboardResult^ lastResult = resultTask.get();
				 XboxOneMabUtils::LogCommentFormat(L"Name: %s", lastResult->DisplayName->Data());

				 for( UINT index = 0; index < lastResult->Rows->Size; index++ )
				 {
					 XBoxOneStatsGetData data = XBoxOneStatsGetData();
					 LeaderboardRow^ row = lastResult->Rows->GetAt(index);
					 ConvertResultToLeaderboardRow(row,data);
					 resultList.push_back(data);
					 userList->Append(row->XboxUserId);
					  XboxOneMabUtils::LogCommentFormat(L"%d XboxUserId: %s Percentile: %f Rank: %d", 
						 itemCount++,
						 row->XboxUserId->Data(),
						 row->Percentile,
						 row->Rank
						 );
				 }

				 while (lastResult != nullptr && lastResult->Rows->Size == max_rows && itemCount < countLimit)
				 {
					 create_task(lastResult->GetNextAsync(max_rows))
						 .then([this, &lastResult, &itemCount, &resultList, userList](LeaderboardResult^ result)
					 {
						 lastResult = result;
						 for( UINT index = 0; index < lastResult->Rows->Size; index++ )
						 {
							 XBoxOneStatsGetData data = XBoxOneStatsGetData();
							 LeaderboardRow^ row = lastResult->Rows->GetAt(index);
							 ConvertResultToLeaderboardRow(row,data);
							 resultList.push_back(data);
							 userList->Append(row->XboxUserId);
							 XboxOneMabUtils::LogCommentFormat(L"%d XboxUserId: %s Percentile: %f Rank: %d", 
								 itemCount++,
								 row->XboxUserId->Data(),
								 row->Percentile,
								 row->Rank
								 ) ;
						 }
					 }).wait();
				 }

	//			 XboxOneMabUtils::GetUserInfoFromXUIDs(userList->GetView(),userprofileList);


				 IVector< String^ >^ statNames = ref new Vector< String^ >;

				 statNames->Append( ref new String(L"RUsevenReliability") ); 

				 auto asyncOp = m_xboxLiveContext->UserStatisticsService->GetMultipleUserStatisticsAsync(
					 userList->GetView(), 
					 Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId, // the service config that contains the stats we want
					 statNames->GetView() // a list of stat names we want
					 );

				 create_task( asyncOp )
					 .then( [this,&resultList] (task<IVectorView<UserStatisticsResult^>^> resultTask) 
				 {  
					 try
					 {
						 IVectorView<UserStatisticsResult^>^ statisticResult = resultTask.get();
						 XboxOneMabUtils::LogCommentFormat(L"statistic count: %d", statisticResult->Size);
						 UINT index = 0; 
						 for each(UserStatisticsResult^ result in statisticResult)
						 {
							 ServiceConfigurationStatistic^ configStat = result->ServiceConfigurationStatistics->GetAt(0);
							 XboxOneMabUtils::LogCommentFormat(L"ServiceConfigurationId: %s", configStat->ServiceConfigurationId->Data());
							 if(configStat->Statistics->Size > 0){
								 Statistic^ stat = configStat->Statistics->GetAt(0);
								 if(wcslen(stat->Value->Data()) != 0)
									 resultList[index].reliable = _wtoi(stat->Value->Data());
							 }
							 index++;
						 }
					
					 }
					 catch (Platform::Exception^ ex)
					 {
					
					 }
				 }).wait();
				 //Send Message to contreoller here

	//			 results_data = reinterpret_cast<XStatsEnumeratorData*>pResults;
				 Notify( XboxOneMabStatisticsManagerMessage( STATS_STATE::STATS_STATE_ENUMERATE, true, callback_id, resultList ) );



			 }
			 catch (Platform::Exception^ ex)
			 {
				 if (ex->HResult == INET_E_DATA_NOT_AVAILABLE)
				 {
					 // we hit the end of the list
					  XboxOneMabUtils::LogComment(L"Reach the end");
				 }
				 else
				 {
					XboxOneMabUtils::LogCommentFormat( 
						 L"Error calling GetLeaderboardAsync function: 0x%0.8x", 
						 ex->HResult
						 );
				 }
				 Notify( XboxOneMabStatisticsManagerMessage( STATS_STATE::STATS_STATE_ENUMERATE, true, callback_id, resultList ) );
			 }
		 });
	}catch(Platform::Exception^ ex)
	{
		MABLOGDEBUG("Error to call get leaderboard service");
	}


	return true;
}


bool XboxOneMabStatisticsManager::ReadLeaderboardForFriends(DWORD _view_id, const MabVector< WORD >& _column_list, INT start_rank, UINT max_rows /*= MAX_READ_ROWS*/, UINT* overlapped_id /*= NULL */, UINT leaderboardID /*=0*/)
{
	String^ statisticName = L"RUTest.GameplayModeId.1";
	if(leaderboardID == 0)
		statisticName = L"RU3LeaderBoard.GamemodeID.0";
	else if(leaderboardID == 1)
		statisticName = L"RU3LeaderBoard.GamemodeID.1";

	*overlapped_id = unique_id++;
	UINT callback_id = *overlapped_id;

	if( m_xboxLiveContext == nullptr )
	{
		throw ref new Platform::InvalidArgumentException(L"A valid User is required");
	}

	auto asyncOp = m_xboxLiveContext->LeaderboardService->GetLeaderboardForSocialGroupAsync(
		m_xboxUserId,
		Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId,
		statisticName,
		Microsoft::Xbox::Services::Social::SocialGroupConstants::People,
		max_rows
		);

	try
	{

		create_task( asyncOp )
			.then( [this, max_rows, callback_id] (task<LeaderboardResult^> resultTask) 
		{  
			MabVector<XBoxOneStatsGetData> resultList;
			resultList.clear();
			resultList.reserve(32);
			Vector<Platform::String^>^ userList = ref new Vector<Platform::String^>;
			Vector<Microsoft::Xbox::Services::Social::XboxUserProfile^>^ userprofileList = ref new Vector<Microsoft::Xbox::Services::Social::XboxUserProfile^>;

			const int countLimit = max_rows;
			int itemCount = 0;
			try
			{
				LeaderboardResult^ lastResult = resultTask.get();
				// allocate the data container

				XboxOneMabUtils::LogCommentFormat(L"Name: %s", lastResult->DisplayName->Data());

				for( UINT index = 0; index < lastResult->Rows->Size; index++ )
				{
					XBoxOneStatsGetData data = XBoxOneStatsGetData();
					LeaderboardRow^ row = lastResult->Rows->GetAt(index);
					ConvertResultToLeaderboardRow(row,data);
					resultList.push_back(data);
					userList->Append(row->XboxUserId);
					XboxOneMabUtils::LogCommentFormat(L"%d GamerTag: %s Percentile: %f Rank: %d",
						itemCount++,
						row->Gamertag->Data(),
						row->Percentile, 
						row->Rank
						);
				}

				while (lastResult != nullptr && lastResult->Rows->Size == max_rows && itemCount < countLimit)
				{
					create_task(lastResult->GetNextAsync(max_rows))
						.then([this, &lastResult, &itemCount,&resultList,userList](LeaderboardResult^ result)
					{
						lastResult = result;
						for( UINT index = 0; index < lastResult->Rows->Size; index++ )
						{
							XBoxOneStatsGetData data = XBoxOneStatsGetData();
							LeaderboardRow^ row = lastResult->Rows->GetAt(index);
							ConvertResultToLeaderboardRow(row,data);
							resultList.push_back(data);
							userList->Append(row->XboxUserId);
							XboxOneMabUtils::LogCommentFormat(L"%d GamerTag: %s Percentile: %f Rank: %d",
								itemCount++,
								row->Gamertag->Data(),
								row->Percentile, 
								row->Rank
								);
						}

					}).wait();
				}
	/*
				XboxOneMabUtils::GetUserInfoFromXUIDs(userList->GetView(),userprofileList);
				UINT indexProfile = 0;
				for each(Microsoft::Xbox::Services::Social::XboxUserProfile^ userprofile in userprofileList)
				{
					if(userprofile->GameDisplayName)
						XboxOneMabUtils::ConvertPlatformStringToMabString(userprofile->GameDisplayName,resultList[indexProfile].gametag);
					indexProfile++;
				}
	*/
				IVector< String^ >^ statNames = ref new Vector< String^ >;

				statNames->Append( ref new String(L"RUsevenReliability") ); 

				auto asyncOp = m_xboxLiveContext->UserStatisticsService->GetMultipleUserStatisticsAsync(
					userList->GetView(), 
					Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId, // the service config that contains the stats we want
					statNames->GetView() // a list of stat names we want
					);

				create_task( asyncOp )
					.then( [this,&resultList] (task<IVectorView<UserStatisticsResult^>^> resultTask) 
				{  
					try
					{
						IVectorView<UserStatisticsResult^>^ statisticResult = resultTask.get();
						XboxOneMabUtils::LogCommentFormat(L"statistic count: %d", statisticResult->Size);
						UINT index = 0; 
						for each(UserStatisticsResult^ result in statisticResult)
						{
							ServiceConfigurationStatistic^ configStat = result->ServiceConfigurationStatistics->GetAt(0);
							XboxOneMabUtils::LogCommentFormat(L"ServiceConfigurationId: %s", configStat->ServiceConfigurationId->Data());
							if(configStat->Statistics->Size > 0){
								Statistic^ stat = configStat->Statistics->GetAt(0);
								if(wcslen(stat->Value->Data()) != 0)
									resultList[index].reliable = _wtoi(stat->Value->Data());
							}
							index++;
						}

					}
					catch (Platform::Exception^ ex)
					{

					}
				}).wait();

				Notify( XboxOneMabStatisticsManagerMessage( STATS_STATE::STATS_STATE_ENUMERATE, true, callback_id, resultList ) );
			}
			catch (Platform::Exception^ ex)
			{
				if (ex->HResult == INET_E_DATA_NOT_AVAILABLE)
				{
					// we hit the end of the list
					XboxOneMabUtils::LogComment(L"Reach the end");
				}
				else
				{
					XboxOneMabUtils::LogCommentFormat( 
						L"Error calling ReadLeaderboardForFriends function: 0x%0.8x", 
						ex->HResult
						);
				}
				Notify( XboxOneMabStatisticsManagerMessage( STATS_STATE::STATS_STATE_ENUMERATE, true, callback_id, resultList ) );
			}
		});
	}catch( Platform::Exception^ ex )
	{
		MABLOGDEBUG("Fail to call xboxone leaderboard service");
	}

	return true;
}

bool XboxOneMabStatisticsManager::ReadUserStats(const MabVector< MabString >& _xuid_list, const MabMap< DWORD, MabVector< WORD > >& view_map, UINT* overlapped_id /*= NULL*/, UINT leaderboardID /*=0*/)
{
	for (DWORD i=0; i<_xuid_list.size(); ++i)
	{
		XboxOneMabUtils::LogCommentFormat(L"Find a user with ID: %s",_xuid_list[i].data());
	}

	IVector< String^ >^ statNames = ref new Vector< String^ >;

	//Request for leaderboard and reliability
	if(leaderboardID == 0)
		statNames->Append( ref new String(L"RU3LeaderBoard.GamemodeID.0") );
	else if(leaderboardID == 1)
		statNames->Append( ref new String(L"RU3LeaderBoard.GamemodeID.1") );

//	statNames->Append( ref new String(L"RUTest.GameplayModeId.1") );
	statNames->Append( ref new String(L"RUsevenReliability") ); 

	auto asyncOp = m_xboxLiveContext->UserStatisticsService->GetSingleUserStatisticsAsync(
		m_xboxUserId, // the Xbox user ID of whose stats we want to retrieve
		Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId, // the service config that contains the stats we want
		statNames->GetView() // a list of stat names we want
		);

	create_task( asyncOp )
		.then( [this] (task<UserStatisticsResult^> resultTask) 
	{  

	});


	return true;
}



bool XboxOneMabStatisticsManager::ReadAchievementStats(UINT achievementID /*= 0*/)
{
	IVector< String^ >^ statNames = ref new Vector< String^ >;
	String^ achivementStat = ref new String(L"RU3Achievement.AchievementID.") + achievementID.ToString();
	statNames->Append(achivementStat);
	if(m_xboxUserId == nullptr || m_xboxLiveContext == nullptr)
		return false;

	bool is_gained = false;
	auto asyncOp = m_xboxLiveContext->UserStatisticsService->GetSingleUserStatisticsAsync(
		m_xboxUserId, // the Xbox user ID of whose stats we want to retrieve
		Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId, // the service config that contains the stats we want
		statNames->GetView() // a list of stat names we want
		);

	try{

		create_task( asyncOp )
			.then( [this, &is_gained] (task<UserStatisticsResult^> resultTask) 
		{  

			try
			{
				UserStatisticsResult^ statisticResult = resultTask.get();
				for( ServiceConfigurationStatistic^ serviceConfigStatistic : statisticResult->ServiceConfigurationStatistics )
				{
					for( Statistic^ statistic : serviceConfigStatistic->Statistics )
					{
						MABLOGDEBUG("Achievement data type is: %ws",statistic->StatisticType.ToString()->Data());
						if(_wtoi(statistic->Value->Data()) > 0)
						{
							is_gained = true;
						}else{
							is_gained = false;
						}
					}

				}
			}catch(Platform::Exception^ ex)
			{ 
			}
		}).wait();
	}catch(Platform::Exception^ ex){
		MABLOGDEBUG("Fail to call xboxone leaderboard service");
	}
	return is_gained;
}




bool XboxOneMabStatisticsManager::ReadLeaderboardByRank(DWORD _view_id, const MabVector< WORD >& _column_list, INT start_rank, UINT max_rows /*= MAX_READ_ROWS*/, UINT* overlapped_id /*= NULL */, UINT leaderboardID /*=0*/)
{
	return true;
}

bool XboxOneMabStatisticsManager::ReadLeaderboardForMyself(DWORD _view_id, const MabVector< WORD >& _column_list, UINT max_rows /*= MAX_READ_ROWS*/, UINT* overlapped_id /*= NULL */, UINT leaderboardID /*=0*/)
{
	String^ leaderboardName  = L"RUTest";
	if(leaderboardID == 0)
		leaderboardName = L"RUFifteensScore";
	else if(leaderboardID == 1)
		leaderboardName = L"RUSevensScore";

	*overlapped_id = unique_id++;
	UINT callback_id = *overlapped_id;

	if( m_xboxLiveContext == nullptr )
	{
		throw ref new Platform::InvalidArgumentException(L"A valid User is required");
	}
	auto asyncOp = m_xboxLiveContext->LeaderboardService->GetLeaderboardWithSkipToUserAsync(
		Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId,
		leaderboardName,
		m_xboxUserId, 
		max_rows
		);
	
	try
	{
		create_task( asyncOp )
			.then( [this, max_rows, callback_id] (task<LeaderboardResult^> resultTask) 
		{  
			MabVector<XBoxOneStatsGetData> resultList;
			resultList.reserve(32);

			const int countLimit = max_rows;
			int itemCount = 0;

			Vector<Platform::String^>^ userList = ref new Vector<Platform::String^>;

			try
			{

				LeaderboardResult^ lastResult = resultTask.get();
				XboxOneMabUtils::LogCommentFormat(L"Name: %s", lastResult->DisplayName->Data());

				for( UINT index = 0; index < lastResult->Rows->Size; index++ )
				{
					XBoxOneStatsGetData data = XBoxOneStatsGetData();
					LeaderboardRow^ row = lastResult->Rows->GetAt(index);
					ConvertResultToLeaderboardRow(row,data);
					resultList.push_back(data);
					userList->Append(row->XboxUserId);
					XboxOneMabUtils::LogCommentFormat(L"%d XboxUserId: %s Percentile: %f Rank: %d", 
						itemCount++,
						row->XboxUserId->Data(),
						row->Percentile,
						row->Rank
						);
				}

				while (lastResult != nullptr && lastResult->Rows->Size == max_rows && itemCount < countLimit)
				{
					create_task(lastResult->GetNextAsync(max_rows))
						.then([this, &lastResult, &itemCount, &resultList,userList](LeaderboardResult^ result)
					{
						lastResult = result;
						for( UINT index = 0; index < lastResult->Rows->Size; index++ )
						{
							XBoxOneStatsGetData data = XBoxOneStatsGetData();
							LeaderboardRow^ row = lastResult->Rows->GetAt(index);
							ConvertResultToLeaderboardRow(row,data);
							resultList.push_back(data);
							userList->Append(row->XboxUserId);
							XboxOneMabUtils::LogCommentFormat(L"%d XboxUserId: %s Percentile: %f Rank: %d", 
								itemCount++,
								row->XboxUserId->Data(),
								row->Percentile,
								row->Rank
								) ;
						}
					}).wait();
				}

				IVector< String^ >^ statNames = ref new Vector< String^ >;

				statNames->Append( ref new String(L"RUsevenReliability") ); 

				auto asyncOp = m_xboxLiveContext->UserStatisticsService->GetMultipleUserStatisticsAsync(
					userList->GetView(), 
					Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId, // the service config that contains the stats we want
					statNames->GetView() // a list of stat names we want
					);

				create_task( asyncOp )
					.then( [this,&resultList] (task<IVectorView<UserStatisticsResult^>^> resultTask) 
				{  
					try
					{
						IVectorView<UserStatisticsResult^>^ statisticResult = resultTask.get();
						XboxOneMabUtils::LogCommentFormat(L"statistic count: %d", statisticResult->Size);
						UINT index = 0; 
						for each(UserStatisticsResult^ result in statisticResult)
						{
							ServiceConfigurationStatistic^ configStat = result->ServiceConfigurationStatistics->GetAt(0);
							XboxOneMabUtils::LogCommentFormat(L"ServiceConfigurationId: %s", configStat->ServiceConfigurationId->Data());
							if(configStat->Statistics->Size > 0){
								Statistic^ stat = configStat->Statistics->GetAt(0);
								if(wcslen(stat->Value->Data()) != 0)
									resultList[index].reliable = _wtoi(stat->Value->Data());
							}
							index++;
						}

					}
					catch (Platform::Exception^ ex)
					{

					}
				}).wait();

				Notify( XboxOneMabStatisticsManagerMessage( STATS_STATE::STATS_STATE_ENUMERATE, true, callback_id, resultList ) );

			}
			catch (Platform::Exception^ ex)
			{
				if (ex->HResult == INET_E_DATA_NOT_AVAILABLE)
				{
					// we hit the end of the list
					XboxOneMabUtils::LogComment(L"Reach the end");
				}
				else
				{
					XboxOneMabUtils::LogCommentFormat( 
						L"Error calling GetLeaderboardAsync function: 0x%0.8x", 
						ex->HResult
						);
				}
				Notify( XboxOneMabStatisticsManagerMessage( STATS_STATE::STATS_STATE_ENUMERATE, true, callback_id, resultList ) );
			}
		});
	}catch(Platform::Exception^ ex){
		MABLOGDEBUG("Failed to call xbox leaderboard service");
	}

	return true;
}

void XboxOneMabStatisticsManager::ConvertResultToLeaderboardRow(LeaderboardRow^ row, XBoxOneStatsGetData& leaderboardData)
{
	uint64 stringData =_wtoi64( row->Values->GetAt(0)->Data());
	MabString userid, gametag;
	XboxOneMabUtils::ConvertPlatformStringToMabString(row->XboxUserId,userid);
	XboxOneMabUtils::ConvertPlatformStringToMabString(row->Gamertag,gametag);
	leaderboardData.GetFromServer(stringData,userid,gametag,row->Rank);
}

bool XboxOneMabStatisticsManager::sendMatchResult(int gamemode, bool winned, bool lost, bool draw, int score, int reliable, int visuable_score /* = 0*/)
{
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();

	XBoxOneStatsGetData data = XBoxOneStatsGetData();
	data.won = winned;
	data.lost = lost;
	data.draw = draw;
	data.score = score;

	unsigned __int64 matchResult = data.ConvertToServerData();

	ULONG result = EventWriteRU3MatchCompleted(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),playerSessionId,gamemode,1,matchResult,score,2,lost,draw,reliable);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}else{
		return false;
	}
}

bool XboxOneMabStatisticsManager::sendMatchComplete(int gamemode, unsigned int winned, unsigned int lost, unsigned int draw, int score, int reliable, int visuable_score /* = 0*/)
{
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();

	XBoxOneStatsGetData data = XBoxOneStatsGetData();
	data.won = winned;
	data.lost = lost;
	data.draw = draw;
	data.score = score;

	unsigned __int64 matchResult = data.ConvertToServerData();

	ULONG result = EventWriteRU3MatchCompleted(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),playerSessionId,gamemode,1,matchResult,score,winned,lost,draw,reliable);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}else{
		return false;
	}
}

bool XboxOneMabStatisticsManager::sendPlayerDefeated()
{
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();

	ULONG result = EventWritePlayerDefeated(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),
												0,
												playerSessionId,
												nullptr,
												0,
												0,
												nullptr,
												0,
												0,
												0,
												0,
												0.0f,
												0.0f,
												0.0f);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}else{
		return false;
	}
}


Windows::Foundation::IAsyncAction^ XboxOneMabStatisticsManager::sendAchievementResult(int achievement_id, int data)
{
	return create_async([this,achievement_id,data]()
	{
		try
		{
			if(XboxOneGame->GetCurrentUser() == nullptr || !XboxOneMabNetGame->IsNetworkOnline())
				return;
			auto it = m_achievement_map.find(achievement_id);
			if(it!= m_achievement_map.end())
			{
				it->second = 1;
				return;
			}
			else
			{
				m_achievement_map[achievement_id] = 1;
			}


			ULONG result = EventWriteRU3AwardAchievement(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),&m_guid,achievement_id,data);
			if (result != ERROR_SUCCESS)
			{

			}else
			{
				float percent = 0.0f;

				XboxOneAchievement->RefreshAchievementProgress();
				XboxOneAchievement->GetAchievementsProgressForCurrentUser(percent);
				sendGameProgress(percent);
			}

		}
		catch (Platform::Exception^ ex)
		{

		}
	});
}

bool XboxOneMabStatisticsManager::sendGameProgress(const float percent)
{
	if(XboxOneGame->GetCurrentUser() == nullptr)
		return false;
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();
	if(playerSessionId == nullptr)
		return false;

	ULONG result = EventWriteGameProgress(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),playerSessionId, percent);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool XboxOneMabStatisticsManager::sendSessionStart(int game_mode)
{
	if(XboxOneGame->GetCurrentUser() == nullptr)
		return false;
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();
	if(playerSessionId == nullptr)
		return false;

	ULONG result = EventWritePlayerSessionStart(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),
											playerSessionId, 
											nullptr, 
											game_mode, 
											100);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool XboxOneMabStatisticsManager::sendSessionEnd(int game_mode, int minutes)
{
	if(XboxOneGame->GetCurrentUser() == nullptr)
		return false;
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();
	if(playerSessionId == nullptr)
		return false;

	ULONG result = EventWritePlayerSessionEnd(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),
											playerSessionId, 
											nullptr, 
											game_mode, 
											100, 
											0, 
											minutes);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool XboxOneMabStatisticsManager::sendMultiplayerRoundStart(int game_mode, int match_type, int difficulty_level)
{
	if(XboxOneGame->GetCurrentUser() == nullptr)
		return false;
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();
	if(playerSessionId == nullptr)
		return false;

	GUID RoundGUID;
	CoCreateGuid( &RoundGUID );

	auto gameSession =XboxOneSessionManager->GetGameSession();
	if(gameSession == nullptr)
		return false;

	ULONG result = EventWriteMultiplayerRoundStart(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),
												&RoundGUID, 
												0, 
												playerSessionId, 
												gameSession->MultiplayerCorrelationId->Data(), 
												game_mode,
												match_type, 
												difficulty_level);

	if (result == ERROR_SUCCESS)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool XboxOneMabStatisticsManager::sendMultiplayerRoundEnd(int game_mode, int match_type, int difficulty_level, float time_in_seconds, int exit_status)
{
	if(XboxOneGame->GetCurrentUser() == nullptr)
		return false;
	GUID* playerSessionId = XboxOneSessionManager->GetUniqueName();
	if(playerSessionId == nullptr)
		return false;

	GUID RoundGUID;
	CoCreateGuid( &RoundGUID );

	auto gameSession =XboxOneSessionManager->GetGameSession();
	if(gameSession == nullptr)
		return false;

	ULONG result = EventWriteMultiplayerRoundEnd(XboxOneGame->GetCurrentUser()->XboxUserId->Data(),
												&RoundGUID, 
												0, 
												playerSessionId, 
												gameSession->MultiplayerCorrelationId->Data(), 
												game_mode,
												match_type, 
												difficulty_level,
												time_in_seconds,
												exit_status);
	if (result == ERROR_SUCCESS)
	{
		return true;
	}												
	else
	{
		return false;
	}
}
void XboxOneMabStatisticsManager::GenerateGUID()
{
	GUID sessionNameGUID;
	CoCreateGuid( &sessionNameGUID );
	m_guid = sessionNameGUID;
}

void XboxOneMabStatisticsManager::SetMultiplayerRoundStartTime()
{
	m_multiplayer_round_start_time = MabTime::GetCurrentTime();
}

void XboxOneMabStatisticsManager::SetMultiplayerRoundEndTime()
{
	m_multiplayer_round_end_time = MabTime::GetCurrentTime();
}

float XboxOneMabStatisticsManager::GetMultiplayerRoundElapsedTime()
{
	return (MabTime::GetCurrentTime() - m_multiplayer_round_start_time);
}