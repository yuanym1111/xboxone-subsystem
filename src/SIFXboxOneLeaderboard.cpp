#include "Precompiled.h"
#include "SIFXboxOneLeaderboard.h"
#include "SIFLeaderboard.h"

#include <MabLocaleInterface.h>
#include <MabLocaleStringFormatter.h>

#include "SIFLeaderboardPopulator.h"
#include "SIFLanguage.h"
#include <XboxOneMabStatisticsManager.h>
#include <XboxOneMabStatisticsEntity.h>

#include <MabControllerManager.h>
#include <MabController.h>
#include "xboxone/MabXboxOneController.h"
#include <MabUINode.h>
#include "SIFWindowSystem.h"
#include "SIFWindow.h"
#include "SIFCommonDebugSettings.h"

MABRUNTIMETYPE_IMP1( SIFXboxOneLeaderboard, SIFLeaderboard )
MABRUNTIMETYPE_IMP1( SIFXboxOneLeaderboardDataRow, SIFLeaderboardDataRow )
MABRUNTIMETYPE_IMP1( SIFXboxOneLeaderboardQueue, SIFLeaderboardQueue )


// String initialiser; junk data.
static const MabString REQUEST_INIT( "REQUESTINIT" );

// Application defined timeout for any leaderboard request
// This is intended to act more quickly than, say, a DNS lookup failure on uplink disconnect
// which might take up to 40 seconds or longer
static const MabTime REQUEST_TIMEOUT(15.0f);

MabVector< MabString > SIFXboxOneLeaderboard::INITIAL_COL_HEADINGS( MMHEAP_PERMANENT_DATA );

SIFXboxOneLeaderboard::SIFXboxOneLeaderboard(MABMEM_HEAP _heap, DWORD _leaderboard_id, const MabString& _title, const MabVector< MabXBoxOneLeaderboardColumnDefinition >& _custom_columns, XboxOneMabStatisticsManager& _stats_manager, SIFLeaderboardPopulator* _populator, RULeaderboardFieldFormatter* _field_formatter)
	:	SIFLeaderboard( _heap, static_cast< MabUInt64 >( _leaderboard_id ), _title, _custom_columns, _populator, NULL, _field_formatter ),
	stats_manager ( _stats_manager ), 
	custom_col_ids( _heap ),
	requested_rows( _heap ),
	controller_id( -1 )
{
	for ( MabVector< MabString >::iterator iter = INITIAL_COL_HEADINGS.begin(); iter != INITIAL_COL_HEADINGS.end(); ++iter )
	{
		full_col_name_list.push_back( *iter );
	}
	for ( MabVector< MabXBoxOneLeaderboardColumnDefinition>::const_iterator iter = custom_columns.begin(); iter != custom_columns.end(); ++iter )
	{
		MABLOGDEBUG( "SIFXboxOneLeaderboard custom column being added: %s", iter->GetName().c_str() );
		full_col_name_list.push_back( "[" + iter->GetName() + "]" );
		custom_col_ids.push_back( iter->GetID() );
	}

	stats_manager.Attach( this );
}

SIFXboxOneLeaderboard::~SIFXboxOneLeaderboard()
{
	full_col_name_list.clear();
}

void SIFXboxOneLeaderboard::Update()
{
	// Check all open stats requests and see if any have timed out
	#ifndef _DEBUG
	for ( MabMap< UINT, RequestTuple >::iterator msg_iter = open_stats_requests.begin(); msg_iter != open_stats_requests.end(); )
	{
		RequestTuple &tuple = msg_iter->second;

		// This request has been going for too long
		if ( MabTime::GetCurrentMabTime() - tuple.init_time > REQUEST_TIMEOUT )
		{
			// Compose message indicating load failure
			MabVector< MabString > strings_out;
			SIFLeaderboardPopulatorMessage message_out( leaderboard_id, tuple.min_display_index, tuple.max_display_index, tuple.filter, strings_out, tuple.ui_elem_name, false );

			// Erase the request
			// Note: might want to see notes in other places where requests are removed
			msg_iter = open_stats_requests.erase( msg_iter );

			// Send out notification
			Notify( message_out );
		}		
		// Continue normally
		else
		{
			++msg_iter;
		}
	}
	#endif
}

void SIFXboxOneLeaderboard::Update(MabObservable< XboxOneMabStatisticsManagerMessage >* source, const XboxOneMabStatisticsManagerMessage& msg)
{
	MabMap< UINT, RequestTuple >::iterator msg_iter = open_stats_requests.find( msg.GetID() );

	// If we don't know/care about this message, exit.
	if ( msg_iter == open_stats_requests.end() ) return;

	RequestTuple &tuple = msg_iter->second;

	//TODO:no idea about this logic
	if ( tuple.filter == OVERALL ||  tuple.filter == OVERALL_SEVEN )
	{
		int min_max_diff = tuple.max_display_index - tuple.min_display_index;
		tuple.min_display_index = requested_rows.ClearRequest( tuple.filter, msg.GetID() );
		tuple.max_display_index = tuple.min_display_index + min_max_diff;
	} else {
		requested_rows.ClearRequest( tuple.filter, msg.GetID() );
	}

	MabVector< MabString > strings_out;
	strings_out.reserve( ( tuple.max_display_index - tuple.min_display_index + 1 ) * ( full_col_name_list.size() ) );

	UpdateMessageEval( msg, tuple, strings_out );

	// Send out a message, even if it's an empty vector.
	SIFLeaderboardPopulatorMessage message_out( leaderboard_id, tuple.min_display_index, tuple.max_display_index, tuple.filter, strings_out, tuple.ui_elem_name );

	// Even if the operation wasn't successful, remove the id of the stats_request; it won't be coming back again
	// Note: we need to change this from the simple open_stats_requests.erase( msg_iter ); because there's the potential
	//	the UpdateMessageEval will have added another stats request to it
	open_stats_requests.erase( msg.GetID() );

	Notify( message_out );

}

void SIFXboxOneLeaderboard::UpdateMessageEval(const XboxOneMabStatisticsManagerMessage& msg, RequestTuple& tuple_in_out, MabVector< MabString >& strings_out)
{
	strings_out.clear();
    MabVector<XBoxOneStatsGetData> data = msg.GetStatsReadResults();

	if(data.size() <= 0)
		return;

#ifdef ENABLE_FIRST_PLACE_RANK_DISPLAYED
	int tmp_for_end;

	if ( tuple_in_out.filter == FRIEND || tuple_in_out.max_index )
	{ 
		tmp_for_end = (int) lbd_results.dwNumRows;
	} else {
		tmp_for_end = MabMath::Min( tuple_in_out.max_index + 1, (int) lbd_results.dwNumRows );
	}

	// Begin the for loop: for every row...
	for ( int row_ctr = 0; row_ctr < tmp_for_end; ++row_ctr )
#else

	MABLOGDEBUG( "SIFXboxOneLeaderboard::UpdateMessageEval: Number of rows: %u",  data.size() );

	int i = 1;
	for( MabVector< XBoxOneStatsGetData >::iterator iter = data.begin(); iter != data.end(); ++iter, ++i )		
#endif
      
	{
		SIFXboxOneLeaderboardDataRow this_internal_row = SIFXboxOneLeaderboardDataRow( iter->rank, iter->gametag, iter->xboxuserid, MabVector<MabString>() );

		/************************************************************************/
		/*       Iterate for all columns                                        */
		/************************************************************************/

		this_internal_row.addl_strings.clear();
		this_internal_row.addl_strings.push_back(MabString( 0, "%d", iter->won ));
		this_internal_row.addl_strings.push_back(MabString( 0, "%d", iter->lost ));
		this_internal_row.addl_strings.push_back(MabString( 0, "%d", iter->draw ));
		this_internal_row.addl_strings.push_back(MabString( 0, "%d", iter->reliable ));
		this_internal_row.addl_strings.push_back(MabString( 0, "%d", iter->score ));

		row_data[ tuple_in_out.filter ][ i ] = MabMemNew( heap ) SIFXboxOneLeaderboardDataRow( this_internal_row );

		if ( this_internal_row.addl_strings.size() > 0 )
		{
			field_formatter->Format( custom_columns, this_internal_row.addl_strings );
		}
	}

	if( (tuple_in_out.filter == MY_SCORE && tuple_in_out.secondary_filter == MY_SCORE) || (tuple_in_out.filter == MY_SCORE_SEVEN && tuple_in_out.secondary_filter == MY_SCORE_SEVEN) ){
		has_no_results[ tuple_in_out.filter ] = false;
		tuple_in_out.min_display_index = (int) data[0].rank - 1;
		tuple_in_out.max_display_index = tuple_in_out.min_display_index + ((int) data.size()) - 1;

	}else{
		tuple_in_out.max_display_index = MabMath::Min( tuple_in_out.max_display_index, (int) ( row_data[ tuple_in_out.filter ].size() - 1 ) );
	}
	// By this point, our data_index_mapping should have all of these fields.
	FilterToRowMap::const_iterator filter_find = row_data.find( tuple_in_out.filter );
	MABASSERTMSG(  data.size() <= 0 || filter_find != row_data.end(), "Filter unexpectedly does not exist for evaluation of leaderboard!" );
	if ( data.size() > 0 && filter_find != row_data.end() )
	{
		if(tuple_in_out.filter == MY_SCORE || tuple_in_out.filter == MY_SCORE_SEVEN){
			for( int rank_iter = 1; rank_iter <= row_data[ tuple_in_out.filter ].size(); ++rank_iter )
			{
				RankToRowMap::const_iterator rank_find = filter_find->second.find( rank_iter );
				MABASSERTMSG( rank_find != filter_find->second.end(), "We unexpectedly don't have this rank in the sorted vector!" );
				if ( rank_find != filter_find->second.end() ) rank_find->second->To1DVector( strings_out );
			}
		}else{
			for ( int i = tuple_in_out.min_display_index; i <= tuple_in_out.max_display_index; ++i )
			{
				RankToRowMap::const_iterator rank_find = row_data[ tuple_in_out.filter ].find( i + 1 );
				MABASSERTMSG( rank_find != row_data[ tuple_in_out.filter ].end(), "Could not find the specified rank!" );
				if( rank_find != row_data[ tuple_in_out.filter ].end() ) rank_find->second->To1DVector( strings_out );
			}
		}
	}


	//	All filters return the overall number of entries in this field
	num_overall_entries = data.size();
	if( tuple_in_out.filter != MY_SCORE && tuple_in_out.filter != MY_SCORE_SEVEN )
	{
		has_no_results[ tuple_in_out.filter ] = ( row_data[ tuple_in_out.filter ].size() == 0 );
	}


}



void SIFXboxOneLeaderboard::OverallDataRequestHelper(int min_requested_index, const RequestTuple& req_tuple)
{
	INT req_top_index;

	// Determine, then, if we already have the element one below the requested min_requested_index
	// NOTE: it's +2 b/c row_data is sorted by rank (off-by-one from index) and THEN we're looking for one below.
	if ( row_data.find( req_tuple.filter ) != row_data.end() &&
		row_data[ req_tuple.filter ].find( min_requested_index + 2 ) != row_data[ req_tuple.filter ].end() )
	{
		req_top_index = MabMath::Max( min_requested_index - (int)stats_manager.MAX_READ_ROWS + 1, 0 );
	} else {
		req_top_index = min_requested_index;
	}

	UINT overlapped_id;

	// make a call to the stats read -- off by one because ranks are one more than the requested index
	stats_manager.ReadLeaderboardGlobal(  leaderboard_id , custom_col_ids, req_top_index + 1, stats_manager.MAX_READ_ROWS, &overlapped_id ,((RequestTuple&)req_tuple).getLeaderboardID());

	// map the returned overlapped_id to a min-max pair so that we can know from the id what information to send back in the message
	MABASSERTMSG( open_stats_requests.find( overlapped_id ) == open_stats_requests.end(), "We're overwriting a stats write request because this is not a unique overlapped request!" );
	// NOTE: Normally this would be open_stats_requests[ overlapped_id ] = RequestTuple( etc ), but b/c RequestTuple has a const MabString
	// it doesn't have a default operator=, so we have to initialise the argument this way.
	MABVERIFY( open_stats_requests.insert( std::pair< UINT, RequestTuple >( overlapped_id, req_tuple ) ).second );

	// Record that we're making a request based on the current_filter
	requested_rows.NewRequest( current_filter, overlapped_id, req_top_index );
}

void SIFXboxOneLeaderboard::FriendDataRequestHelper(int min_requested_index, const RequestTuple& req_tuple)
{
	MABUNUSED( min_requested_index );
	MabVector<MabString> friends_xuids;
	//TODO: Need to call some function to add friend list or moved to the manager to get it

	INT req_top_index;

	MabMap< DWORD, MabVector< WORD > > lbrd_map = MabMap< DWORD, MabVector< WORD > >( heap );
	lbrd_map[ leaderboard_id ] = custom_col_ids;
	UINT overlapped_id;
	stats_manager.ReadLeaderboardForFriends(leaderboard_id , custom_col_ids, req_top_index + 1, stats_manager.MAX_READ_ROWS, &overlapped_id, ((RequestTuple&)req_tuple).getLeaderboardID());
//	stats_manager.ReadUserStats( friends_xuids, lbrd_map, &overlapped_id );

	// map the returned overlapped_id to a min-max pair so that we can know from the id what information to send back in the message
	MABASSERTMSG( open_stats_requests.find( overlapped_id ) == open_stats_requests.end(), "We're overwriting a stats write request because this is not a unique overlapped request!" );
	// NOTE: Normally this would be open_stats_requests[ overlapped_id ] = RequestTuple( etc ), but b/c RequestTuple has a const MabString
	// it doesn't have a default operator=, so we have to initialise the argument this way.
	MABVERIFY( open_stats_requests.insert( std::pair< UINT, RequestTuple >( overlapped_id, req_tuple ) ).second );

	// Record that we're making a Friends request
	requested_rows.NewRequest( req_tuple.filter );
}

void SIFXboxOneLeaderboard::MyScoreDataRequestHelper(const RequestTuple& req_tuple)
{
	
	UINT overlapped_id;
	// make a call to the stats read -- off by one because ranks are one more than the requested index
	stats_manager.ReadLeaderboardForMyself(  leaderboard_id , custom_col_ids, stats_manager.MAX_READ_ROWS, &overlapped_id, ((RequestTuple&)req_tuple).getLeaderboardID());

	// map the returned overlapped_id to a min-max pair so that we can know from the id what information to send back in the message
	MABASSERTMSG( open_stats_requests.find( overlapped_id ) == open_stats_requests.end(), "We're overwriting a stats write request because this is not a unique overlapped request!" );
	// NOTE: Normally this would be open_stats_requests[ overlapped_id ] = RequestTuple( etc ), but b/c RequestTuple has a const MabString
	// it doesn't have a default operator=, so we have to initialise the argument this way.
	MABVERIFY( open_stats_requests.insert( std::pair< UINT, RequestTuple >( overlapped_id, req_tuple ) ).second );

	// Record that we're making a request based on the current_filter
	requested_rows.NewRequest( req_tuple.filter);
}

void SIFXboxOneLeaderboard::ClearCache(FILTER filter_in /*= NUM_FILTERS */)
{
	SIFLeaderboard::ClearCache(filter_in);
}

bool SIFXboxOneLeaderboard::ClearData(int controller_id /*= -1 */)
{
	ClearCache();
	return true;
}

MabString SIFXboxOneLeaderboard::GetXUIDFromIndex(int index_in) const
{
	// Need to check if we know that there have been no results.
	MabMap< FILTER, bool >::const_iterator no_results = has_no_results.find( current_filter );
	if( no_results != has_no_results.end() && no_results->second ) return NULL;

	FilterToRowMap::const_iterator filter_find = row_data.find( current_filter );
	MABASSERTMSG( filter_find != row_data.end(), "We don't have the filter for this index!" );
	if( filter_find == row_data.end() ) return NULL;

	// Index is zero-based, rank is one-based
	RankToRowMap::const_iterator row_find = filter_find->second.find( index_in + 1 );
	MABASSERTMSG( row_find != filter_find->second.end(), "Could not find given index in Leaderboard data!" );
	if ( row_find == filter_find->second.end() ) return NULL;

	const SIFXboxOneLeaderboardDataRow* const tmp_xboxone_row_ptr = MabCast< SIFXboxOneLeaderboardDataRow >( row_find->second );
	return tmp_xboxone_row_ptr->xuid;
}



SIFXboxOneLeaderboardQueue::SIFXboxOneLeaderboardQueue(MABMEM_HEAP _heap, XboxOneMabStatisticsManager* _stats_manager, SIFLeaderboardPopulator* _populator /*= NULL */)
	:	SIFLeaderboardQueue( _heap, _populator ),
	stats_manager ( _stats_manager ),
	stats_data_clear_enabled( false )
{
	
}

bool SIFXboxOneLeaderboardQueue::Initialise()
{
	if ( !SIFLeaderboardQueue::Initialise() ) return false;

	// Static Leaderboard "const" data
	SIFXboxOneLeaderboard::INITIAL_COL_HEADINGS.reserve( 2 );
	SIFXboxOneLeaderboard::INITIAL_COL_HEADINGS.clear();
	SIFXboxOneLeaderboard::INITIAL_COL_HEADINGS.push_back( "[ID_X_STRINGID_RANKCOL]" );
	SIFXboxOneLeaderboard::INITIAL_COL_HEADINGS.push_back( "[ID_GAMERTAG]" );


	MABASSERT(stats_manager);

	//Check how to get leaderboard info from web api call, Can't find a solution
	const XboxOneLeaderboardRowDefinitionMap& tmp_lbrd_map = stats_manager->GetLeaderboardFields();

	for ( XboxOneLeaderboardRowDefinitionMap::const_iterator iter = tmp_lbrd_map.begin(); iter != tmp_lbrd_map.end(); ++iter )
	{
		leaderboard_queue.push_back( MabMemNew( heap ) SIFXboxOneLeaderboard(heap,iter->second.table_title_key,iter->second.leaderboardName,iter->second.table_columns,*stats_manager,populator,&field_formatter));
		field_formatter.SetLeaderboardID( iter->second.table_title_key );
	}

	if( !leaderboard_queue.empty() ) 
		current_leaderboard = leaderboard_queue.front();

	return true;

}

SIFXboxOneLeaderboardQueue::~SIFXboxOneLeaderboardQueue()
{

}

void SIFXboxOneLeaderboardQueue::Update()
{
	// Call base implementation
	SIFLeaderboardQueue::Update();

	// Call update function on each leaderboard
	for ( MabVector< SIFLeaderboard* >::iterator iter = leaderboard_queue.begin(); iter != leaderboard_queue.end(); ++iter )
	{
		SIFXboxOneLeaderboard* tmp_leaderboard = MabCast< SIFXboxOneLeaderboard >( *iter );
		tmp_leaderboard->Update();
	}	
}

void SIFXboxOneLeaderboardQueue::ResourceInitialise(SIFXboxOneXLASTResource* resource_in)
{

}

void SIFXboxOneLeaderboardQueue::SetControllerID(int controller_id)
{

}

void SIFXboxOneLeaderboardQueue::SetStatsDataClearEnabled(bool _stats_data_clear_enabled)
{

}

bool SIFXboxOneLeaderboardQueue::ClearDataFromAll(int controller_id /*= -1 */)
{
	return true;
}




