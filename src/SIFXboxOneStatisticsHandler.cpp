#include <Precompiled.h>

#include "SIFXboxOneStatisticsHandler.h"
#include "SSGameTimer.h"

#include <XboxOneMabStatisticsManager.h>
#include "RUStatsScorer.h"
#include "SIFUnlockableManager.h"
#include "RUGameScore.h"
#include "XboxOneMabMatchMaking.h"
#include "RUGameSettings.h"

#define STATS_VIEW_ONLINE_MATCHES                   1

SIFXboxOneStasticsHandler::SIFXboxOneStasticsHandler(MABMEM_HEAP _heap, XboxOneMabStatisticsManager* _stats_mgr, SIFXboxOneLeaderboardQueue* _leaderboard_queue) : SIFStatisticsHandler( _heap) , stats_mgr(_stats_mgr) , leaderboard_queue(_leaderboard_queue)
{
	MabVector< RUGameScore::Type > game_score_list;
	game_score_list.push_back( RUGameScore::RANKING_SCORE );
	game_score_list.push_back( RUGameScore::WINS );
	game_score_list.push_back( RUGameScore::LOSSES );
	game_score_list.push_back( RUGameScore::DRAWS );
	game_score_list.push_back( RUGameScore::RELIABILITY );
	game_score_list.push_back( RUGameScore::VISIBLE_SCORE );

	fields_for_write.Add( STATS_VIEW_ONLINE_MATCHES, game_score_list );
}

SIFXboxOneStasticsHandler::~SIFXboxOneStasticsHandler()
{
	MABASSERT(!most_recent_message);
	MabMemDelete(most_recent_message);
}


void SIFXboxOneStasticsHandler::RegisterScorer(int controller_id, RUStatsScorer* scorer)
{
	MABASSERTMSG( stats_scorer_map.find( controller_id ) == stats_scorer_map.end(), "There is a scorer already registered with this controller id!" );
	if( stats_scorer_map.find( controller_id ) != stats_scorer_map.end() ) scorer->Detach( this );
	stats_scorer_map[ controller_id ] = scorer;
	scorer->Attach( this );
}

void SIFXboxOneStasticsHandler::UnregisterScorer(int controller_id)
{
	MabMap< int, RUStatsScorer* >::iterator iter = stats_scorer_map.find( controller_id );
	//MABASSERTMSG( iter != stats_scorer_map.end(), "There is no scorer registered with this controller_id!" );
	if( iter == stats_scorer_map.end() )
		return;

	// If we've got a message in-progress and we've not said otherwise, wait for the message to finish to
	//	unregister.
	/*
	if( most_recent_message != NULL )
	{
		pending_unregister = controller_id;
		return;
	}
	*/
	iter->second->Detach( this );
	if ( iter != stats_scorer_map.end() ) stats_scorer_map.erase( iter );

}

void SIFXboxOneStasticsHandler::Update(MabObservable<SIFStatisticsHandlerMessage>* source, const SIFStatisticsHandlerMessage& msg)
{
	// If it's in trial version, bail.
	if ( !SIFApplication::GetApplication()->GetUnlockableManager()->IsUnlocked( "FMU_UNLOCKED" ) ) return;

	if ( !SIFApplication::GetApplication()->GetMatchMakingManager()->HasSignedInUser( ) ) return;

	stats_mgr->Attach(this);

	if ( msg.perform_write )
	{
		PerformStatsWrites( msg );
	}

	// Either way detach--if we're writing, we won't get messages from the stats_mgr, we'll get
	//	messages from the write_helper.
	stats_mgr->Detach( this );
}

void SIFXboxOneStasticsHandler::Update(MabObservable<XboxOneMabStatisticsManagerMessage>* source, const XboxOneMabStatisticsManagerMessage& msg)
{

}

bool SIFXboxOneStasticsHandler::PerformStatsWrites(const SIFStatisticsHandlerMessage& msg)
{
	//Call statistic manager from the stored score map
	int controller_id = msg.player_controller;
	if( stats_scorer_map.count(msg.player_controller) == 0 ) return false;

	if( stats_scorer_map[msg.player_controller] == NULL ) return false;

	int score = stats_scorer_map[msg.player_controller]->GetScore(RUGameScore::RANKING_SCORE).ToInt64();
	int visibale_score = stats_scorer_map[msg.player_controller]->GetScore(RUGameScore::VISIBLE_SCORE).ToInt64();

	int reliable =  stats_scorer_map[msg.player_controller]->GetScore(RUGameScore::RELIABILITY).ToInt();
	int gameMode = SIFApplication::GetApplication()->GetGameSettings()->game_settings.gameMode;

	int win = stats_scorer_map[msg.player_controller]->GetScore(RUGameScore::WINS).ToInt();
	int loss = stats_scorer_map[msg.player_controller]->GetScore(RUGameScore::LOSSES).ToInt();
	int draw = stats_scorer_map[msg.player_controller]->GetScore(RUGameScore::DRAWS).ToInt();

	stats_mgr->sendMatchComplete(gameMode,win,loss,draw,score,reliable);
	return true;
}
