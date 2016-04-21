/*--------------------------------------------------------------
|        Copyright (C) 1997-2008 by Prodigy Design Ltd         |
|                     Sidhe Interactive (TM)                   |
|                      All rights Reserved                     |
|--------------------------------------------------------------|
|                                                              |
|  Unauthorised use or distribution of this file or any        |
|  portion of it may result in severe civil and criminal       |
|  Penalties and will be prosecuted to the full extent of the  |
|  law.                                                        |
|                                                              |
---------------------------------------------------------------*/

#ifndef _SIF_XBOXONE_LEADERBOARD_H
#define _SIF_XBOXONE_LEADERBOARD_H

#include <XboxOneMabSubsystem.h>			//< Needed b/c the Queue is one
#include <XboxOneMabStatisticsManager.h>	//< needed b/c we need to know about the ColumnData that are an internal struct here
#include "SIFLeaderboard.h"					//< Needed b/c the class is one of these

/// @name forward declarations
/// @{

class XboxOneMabUserInfo;
class XboxOneMabOverlappedManager;
class SIFXboxOneXLASTResource;
class SIFXboxOneLeaderboardDataRow;

/// @}

/// \class SIFXboxOneLeaderboard
///	A representation of an XboxOne Leaderboard
///	This class is the platform-specific implementation of the SIFLeaderboard class.
///	@author Mark Barrett
///

class SIFXboxOneLeaderboard
	:	public SIFLeaderboard,
		public MabObserver< XboxOneMabStatisticsManagerMessage >
{

	MABRUNTIMETYPE_HEADER( SIFXboxOneLeaderboard )

public:

	SIFXboxOneLeaderboard( MABMEM_HEAP _heap, DWORD _leaderboard_id, const MabString& _title, const MabVector<MabXBoxOneLeaderboardColumnDefinition >& _custom_columns, XboxOneMabStatisticsManager& _stats_manager, SIFLeaderboardPopulator* _populator, RULeaderboardFieldFormatter* _field_formatter );
	virtual ~SIFXboxOneLeaderboard();

	/// @name Accessor functions
	/// @{ 

#ifdef ENABLE_FIRST_PLACE_RANK_DISPLAYED
	/// \brief Use this accessor to get stats information about the top row.
	///	@param strings_out		A pointer to a vector of strings; strings will be added as by To1DVector
	/// @return					true if the TopRank has been found, false if not
	bool GetTopRankStrings( MabVector< MabString >* const strings_out = NULL ) const;
#endif 

	/// Use this accessor to get a list of only the columns that we've custom-defined. Note that this will not include the
	///	first two columns returned in the SIFLeaderboardPopulatorMessage--those are rank and gamertag, and don't have IDs
	///	associated with them. If you're looking for the full list, use GetFullColumnNameList().
	inline const MabVector< MabXBoxOneLeaderboardColumnDefinition >& GetCustomColumns() const { return custom_columns; }

	/// @}

	/// \brief Gets an XUID given an index on the present leaderboard
	///	Used by Leaderboard UI code when the player selects a given Gamertag
	MabString GetXUIDFromIndex( int index_in ) const;

	/// Update function handling request timeouts
	virtual void Update();

	/// Respond to XboxOneMabStatisticsManagerMessages
	virtual void Update( MabObservable< XboxOneMabStatisticsManagerMessage >* source, const XboxOneMabStatisticsManagerMessage& msg );

	/// TODO: Remove this test function once we have real leaderboards; ticket #8520
	//bool SendTestMessage(int min_index, int num_values);

	/// Implementation of virtual method inherited from SIFLeaderboard.
	bool HasDuplicateRequest( FILTER filter_in, int min_display_index ) const 
	{ return requested_rows.HasRequestInProgress( filter_in, min_display_index ); }
private:
	/// The constant column headings for Rank and Gamertag.
	/// This structure cannot be const--it needs initialisation--but it's const in theory
	static MabVector< MabString > INITIAL_COL_HEADINGS;

	/// Reference to the stats manager, used for data requests
	XboxOneMabStatisticsManager& stats_manager;

	/// @name main data fields
	/// @{

	/// \brief A container of unresolved stats requests
	///	A map from an overlapped ID--returned by the stats read call--to a triple that contains the filter, the min, and the max
	MabMap< UINT, RequestTuple > open_stats_requests;

	/// A list of all the ids of the custom columns; comes from custom_columns above
	MabVector< WORD > custom_col_ids;

	/// The range of already-requested rows for this leaderboard.
	RequestedRows<UINT> requested_rows;

	/// The controller_id associated with this leaderboard.
	int controller_id;

	/// @}

	/// \brief A helper function for OVERALL-filter leaderboard requests
	/// NOTE: This is NOT, in this case, some sort of helper function that everyone goes through; "overall" in
	///	this case refers to the filter.
	void OverallDataRequestHelper( int min_requested_index, const RequestTuple& req_tuple );

	/// \brief A helper function for FRIEND-filter leaderboard requests
	void FriendDataRequestHelper( int min_requested_index, const RequestTuple& req_tuple );

	/// \brief A helper function for MY_SCORE_filter leaderboard requests
	void MyScoreDataRequestHelper( const RequestTuple& req_tuple );

	/// \brief A helper function to keep Update from getting too indented
	///	NOTE: Only to be called once we've established that we care about this message and we want to remove it from
	///	the list of open_stats_requests. This function will NOT remove it, but it will be the next thing to happen after this function.
	void UpdateMessageEval( const XboxOneMabStatisticsManagerMessage& msg, SIFXboxOneLeaderboard::RequestTuple& tuple_in_out, MabVector< MabString >& strings_out );

	void ClearCache( FILTER filter_in = NUM_FILTERS );

	/// Clear the stats data from this leaderboard
	///	@param controller_id	Specifies the id of the controller to use. If set to -1 delete data for all users.
	bool ClearData( int controller_id = -1 );

	void SetControllerID( int _controller_id ) { controller_id = _controller_id; }

	/// Friending needed so JUST the Queue can have access to ClearData.
	friend class SIFXboxOneLeaderboardQueue;
};

/// \class SIFXboxOneLeaderboardQueue
/// The collection of all leaderboards associated with a given title.
///	@author Mark Barrett
///
class SIFXboxOneLeaderboardQueue
	:	public SIFLeaderboardQueue
{

	MABRUNTIMETYPE_HEADER( SIFXboxOneLeaderboardQueue )

	/// Friending needed for the sake of ResourceInitialise, which should only be called by the XLASTResource
	friend class SIFXboxOneXLASTResource;

public:
	SIFXboxOneLeaderboardQueue( MABMEM_HEAP _heap, XboxOneMabStatisticsManager* _stats_manager,  SIFLeaderboardPopulator* _populator = NULL );
	~SIFXboxOneLeaderboardQueue();

	/// @name Inherited from XboxOneMabSubsystem
	/// @{
	virtual bool Initialise();
	virtual void Update();
	/// @}

	XboxOneMabStatisticsManager* stats_manager;

	/// The list of all the leaderboard titles
	MabVector< MabString> leaderboard_title_queue;

	/// A mapping from leaderboard_id (#defined in GameConfig.spa.h) to a list of columns,
	///	where each column is represented as a triple: the name, the data type and the leaderboard_column_id (also from GameConfig.spa.h)
	MabMap< DWORD, MabVector< MabXBoxOneLeaderboardColumnDefinition > > column_heading_map;

	/// The Initialise function called from the XLAST Resource file
	void ResourceInitialise( SIFXboxOneXLASTResource* resource_in );

	/// Sets the controller_id for all of the leaderboards in this queue
	void SetControllerID( int controller_id );

// SIFCommonDebugSettings is apparently loaded in non-debug retail builds too.
//#ifdef ENABLE_DEBUG_KEYS
	/// This is a field that determines if we're allowed to clear the stats data. It's set through the debug menu.
	bool stats_data_clear_enabled;

	inline bool GetStatsDataClearEnabled() const { return stats_data_clear_enabled; };

	/// Set the stats data clear enabled function and, if it's true, log debug info about how to use it.
	void SetStatsDataClearEnabled( bool _stats_data_clear_enabled );

	/// Clear the stats data from all leaderboards in the queue
	///	@param controller_id	Specifies the id of the controller to use. If set to -1 delete data for all users.
	bool ClearDataFromAll( int controller_id = -1 );

	friend class SIFCommonDebugSettings;
//#endif

};

/// \class SIFXboxOneLeaderboardDataRow
///	An inner class meant to represent a single row of a leaderboard table.
class SIFXboxOneLeaderboardDataRow : public SIFLeaderboardDataRow
{
	MABRUNTIMETYPE_HEADER( SIFXboxOneLeaderboardDataRow )
	/// XUID is the user xuid associated with this row. Closely tied to gamertag, but used for some other system functions.
	///	This should never be displayed.
public:
	MabString xuid;

	SIFXboxOneLeaderboardDataRow( DWORD _rank = 0, MabString _gamertag = "", MabString _xuid = "", MabVector< MabString > _addl_strings = MabVector< MabString >() )
		:	SIFLeaderboardDataRow( static_cast<unsigned int>( _rank ), _gamertag, _addl_strings ), xuid( _xuid ) {};
	SIFXboxOneLeaderboardDataRow( SIFLeaderboardDataRow& parent_row, MabChar _xuid ) 
		:	SIFLeaderboardDataRow( parent_row ), xuid( _xuid ) {};
};


#endif // #define _SIF_XBOXONE_LEADERBOARD_H
