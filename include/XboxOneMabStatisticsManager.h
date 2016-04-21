#ifndef _XBOXONE_MAB_STATISTICS_MANAGER_H
#define _XBOXONE_MAB_STATISTICS_MANAGER_H

#include "XboxOneMabSingleton.h"
#include "XboxOneMabStatisticsEntity.h"
#include <collection.h>

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Achievements;
using namespace Windows::Xbox::System;

class XboxOneMabStatisticsManagerMessage;

// base data container with virtual destructor so we can cast to this for deletion and the appropriate destructor is called
struct XBoxOneUserStatsData
{
	virtual ~XBoxOneUserStatsData() {};
};

struct XBoxOneStatsGetData : public XBoxOneUserStatsData
{
	XBoxOneStatsGetData() : 
		xboxuserid(""),gametag(""),percentile(0.0f),rank(0),won(0),lost(0),draw(0),
		data(0), reliable(0) {};

	void GetFromServer(uint64& data, MabString& userid,  MabString& userTag, uint32 myrank)
	{
		draw = ((uint16)(data)) & 0x0FFF;
		lost = ((uint16)(data >> 12))  & 0x0FFF;
		won = ((uint16)(data >> 24))  & 0x0FFF;
		score = ((uint16)(data >> 36));
		reliable = 1;
		xboxuserid = userid;
		gametag = userTag;
		rank = myrank;
	};
	unsigned __int64 ConvertToServerData()
	{
		data += ((unsigned __int64)score << 36);
		data += ((unsigned __int64)won << 24);
		data += ((unsigned __int64)lost << 12);
		data += (unsigned __int64)draw;
		return data;
	};

	virtual ~XBoxOneStatsGetData() 
	{ 

	}
	MabString xboxuserid;
	MabString gametag;
	float64 percentile;
	uint32 rank;
	uint16 won;
	uint16 draw;
	uint16 lost;
	uint16 score;  //According to SIFLeaderboardPopulator, LEADERBOARD_COLUMN_VALUES it should be point
	uint16 reliable;
	//It should be a vector of value, currently only use the first one


	//data which send to server
	//NT: The uint64 will be  truncated to 52 bits
	//The largest value can be set is 4503599627370495
	//We need to gave score field enough space
	//16 bits for score || 12 bits for wins || 12 bits for losses || 12 bits for draws
	//Set like that first to avoid any bug created, we dont know how server do the sum operator for numbers larger than 52bits
	unsigned __int64 data;
};

struct XBoxOneUserSetStatsData : public XBoxOneUserStatsData
{
	XBoxOneUserSetStatsData() : pXuids(NULL), pSpecs(NULL), pResults(NULL) {}
	virtual ~XBoxOneUserSetStatsData() 
	{
		if (pXuids) MabMemArrayDelete( pXuids );
		if (pSpecs) MabMemArrayDelete( pSpecs );
		if (pResults) MabMemArrayDelete( pResults );
	}
	MabChar* pXuids;
	MabChar* pSpecs;
	void* pResults;
};




class XboxOneMabStatisticsManager: public XboxOneMabSingleton<XboxOneMabStatisticsManager>, public MabObservable<XboxOneMabStatisticsManagerMessage>
{

public:

	/// The maximum number of rows that can be read in by a single read
	static const UINT MAX_READ_ROWS = 100;

	// Substates
	enum STATS_STATE
	{
		STATS_STATE_NONE,			///< No stats operation

		/// \name Read substates
		///	Both these are valid read substates. If you're checking for READ, you almost certainly need to
		///	check for ENUMERATE as well.
		/// @{
		STATS_STATE_READUSERSTATS,	///< Read stats for a specific user
		STATS_STATE_ENUMERATE,		///< Read stats by enumeration--rank or friends
		/// @}

		STATS_STATE_WRITEUSERSTATS	///< Write the users stats
	};

	XboxOneMabStatisticsManager();
	~XboxOneMabStatisticsManager();

	void Initilize();
	bool SetUser(User^ _user);
	bool SetLiveContext(XboxLiveContext^ _liveContext);

	const XboxOneLeaderboardRowDefinitionMap& GetLeaderboardFields();

	///Red leaderboard for my friends
	virtual bool ReadLeaderboardForFriends(DWORD _view_id, const MabVector< WORD >& _column_list, INT start_rank, UINT max_rows = MAX_READ_ROWS,UINT* overlapped_id = NULL ,UINT LeaderboardID = 0);

	/// Read stats for the given leaderboard, starting from (and including) the given rank
	/// NOTE: This function is being kept as being tied to a single leaderboard because it's assumed 
	///	that any call to this function will be made ONLY when you care about a single leaderboard,
	///	on the leaderboard display screen. All reads prior to stats writes should call ReadUserStats
	///	or one of its wrappers.
	/// @param _view_id			The leaderboard to retrieve stats for
	///	@param _column_list		The list of columns to retriece stats for
	///	@param start_rank		The rank (inclusive) to begin the stats read from
	///	@param max_rows			The maximum number of rows to read
	///	@param overlapped_id	The id of the overlapped operation that's performed; for checking against the Overlapped ID
	///							in the message that gets returned
	virtual bool ReadLeaderboardByRank(DWORD _view_id, const MabVector< WORD >& _column_list, INT start_rank, UINT max_rows = MAX_READ_ROWS, UINT* overlapped_id = NULL ,UINT LeaderboardID = 0 );

	/// Read stats for the given leaderboard by xuid. This reads all stats from the leaderboard but uses the XUID
	/// as a pivot. That is returns scores ranked above and below the XUID.
	/// NOTE: This function is being kept as being tied to a single leaderboard because it's assumed 
	///	that any call to this function will be made ONLY when you care about a single leaderboard,
	///	on the leaderboard display screen. All reads prior to stats writes should call ReadUserStats
	///	or one of its wrappers.
	/// @param _view_id			The leaderboard to retrieve stats for
	/// @param _column_list		The list of column data
	/// @param user				The user to use as a pivot for the leaderboard read
	///	@param max_rows			The maximum number of rows to read
	///	@param overlapped_id	The id of the overlapped operation that's performed; for checking against the Overlapped ID
	///							in the message that gets returned
	virtual bool ReadLeaderboardForMyself(DWORD _view_id, const MabVector< WORD >& _column_list, UINT max_rows = MAX_READ_ROWS, UINT* overlapped_id = NULL, UINT LeaderboardID = 0);


	virtual bool ReadLeaderboardGlobal(DWORD _view_id, const MabVector< WORD >& _column_list,INT start_rank, UINT max_rows = MAX_READ_ROWS, UINT* overlapped_id = NULL, UINT LeaderboardID = 0 );

	/// Synchronously delete all records of a given leaderboard. 
	///	WORKS ONLY ON PARTNER.NET, NOT ON RETAIL
	/// @param _view_id			The id of the leaderboard to delete -- defined in spa.h
	bool ClearLeaderboardData( DWORD view_id );

	/// Synchronously delete the records on a given leaderboard for a given player.
	/// WORKS ONLY ON PARTNER.NET, NOT ON RETAIL
	/// @param controller_id	The controller index of the signed in user that you wish to delete the data for
	///	@param view_id			The id of the leaderboard to delete -- defined in spa.h
	bool ClearLeaderboardDataForUser( int controller_id, DWORD view_id );


	unsigned int GetUniqueID() const {return unique_id;}


	/// Not to be used. Accessor for string_to_data_type_map--see comments for that member for more info
	static const MabMap< MabString, BYTE >& GetStringToDataTypeMap() { return string_to_data_type_map; };

	/// Retrieve stats for the list of XUID's on a given leaderboard, for a given set of columns
	/// @param _xuid_list		The list of users to retrieve stats for
	///	@param view_map			A mapping from view_id (aka leaderboard id) to a list of column_ids. Each entry in this map represents 
	///							a leaderboard, and the columns that are mapped to are the list of columns to gather data for.
	/// @param overlapped_id	The id of the overlapped operation that's performed; for checking against the Overlapped ID
	///							in the message that gets returned
	virtual bool ReadUserStats( const MabVector< MabString >& _xuid_list, const MabMap< DWORD, MabVector< WORD > >& view_map, UINT* overlapped_id = NULL, UINT leaderboardID = 0);
	bool ReadAchievementStats(UINT achievementID = 0);
	//  Submit the result to xboxlive to generate the leaderboard
	//  For test purposed only
	bool sendMatchResult(int gamemode, bool winned, bool lost, bool draw, int score, int reliable, int visuable_score = 0);
	bool sendMatchComplete(int gamemode, unsigned int winned, unsigned int lost, unsigned int draw, int score, int reliable, int visuable_score = 0);
	Windows::Foundation::IAsyncAction^ sendAchievementResult(int achivement_id, int result);

	//Game progress update funtion.
	bool sendGameProgress(const float percent);
	bool sendSessionStart(int game_mode);
	bool sendSessionEnd(int game_mode, int minutes);

	//Multiplayer round start/end
	bool sendMultiplayerRoundStart(int game_mode, int match_type, int difficulty_level);
	bool sendMultiplayerRoundEnd(int game_mode, int match_type, int difficulty_level, float time_in_seconds, int exit_status);

	bool sendPlayerDefeated();

	void SetMultiplayerRoundStartTime();
	void SetMultiplayerRoundEndTime();
	float GetMultiplayerRoundElapsedTime();


private:
#ifdef _DEBUG
	Windows::Foundation::EventRegistrationToken m_ServiceCallRoutedEventRegistrationToken;
	void EnableDiagnostics();
	void DisableDiagnostics();
#endif

	void GenerateGUID();
private:

	XboxLiveContext^ m_xboxLiveContext;
	uint32 m_titleId;
	User^ m_user;
	Platform::String^ m_xboxUserId;
	Platform::String^ m_serviceConfigId;
	GUID			  m_guid;
	void readStatisticForUsers();
	std::map<int, int> m_achievement_map;

	void ConvertResultToLeaderboardRow(Microsoft::Xbox::Services::Leaderboard::LeaderboardRow^ row, XBoxOneStatsGetData& leaderboardData);
	//Lock to perform get and set
//	Concurrency::critical_section m_stateLock;

	/// A continuously incrementing unique identifier value.  Each request for an overlapped object increments this
	/// Used to be on overlappedManager, we dont need this class at this stage, will do the logic here
	unsigned int unique_id;

	//Not to be used now, to make it compile first, move to XboxOneMabStatisticEnttity.h
	static MabMap< MabString, BYTE > string_to_data_type_map;
	XboxOneLeaderboardRowDefinitionMap leaderBoardMaps;
	MabVector<MabString> leaderboard_type;

	float m_multiplayer_round_start_time;
	float m_multiplayer_round_end_time;
};


class XboxOneMabStatisticsManagerMessage {
public:

	/// Construct a new message.
	/// @param _type The type of message
	/// @param _result True if the operation completed successfully
	/// @param _data A pointer to the stats read results if a successful read
	XboxOneMabStatisticsManagerMessage( XboxOneMabStatisticsManager::STATS_STATE _type, bool _result, UINT _id, MabVector<XBoxOneStatsGetData>& _data )
		: type(_type), result(_result), id(_id), data(_data)
	{
		MABASSERT( type != XboxOneMabStatisticsManager::STATS_STATE_NONE );
	}

	/// Return the update type
	XboxOneMabStatisticsManager::STATS_STATE GetType() const { return type; }

	/// Return the result of the update
	bool GetResult() const { return result; }

	/// Return the overlapped id
	UINT GetID() const { return id; }

	/// Return a pointer to the stats data
	MabVector<XBoxOneStatsGetData>& GetStatsReadResults() const { return data; }

private:
	XboxOneMabStatisticsManager::STATS_STATE		type;
	bool										result;
	UINT										id; ///< The ID of the overlapped operation - so the caller can match the request to the result
	MabVector<XBoxOneStatsGetData>&				data;
};





#endif