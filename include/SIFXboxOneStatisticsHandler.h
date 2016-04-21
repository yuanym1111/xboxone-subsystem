#ifndef SIF_XBOXONE_STATISTICS_HANDLER_H
#define SIF_XBOXONE_STATISTICS_HANDLER_H

#include "SIFStatisticsHandler.h"

class XboxOneMabStatisticsManager;
class XboxOneMabStatisticsManagerMessage;

class RUStatsScorer;
class SIFXboxOneLeaderboardQueue;

class SIFXboxOneStasticsHandler: public SIFStatisticsHandler,
	public MabObserver<XboxOneMabStatisticsManagerMessage>
{
public:
	SIFXboxOneStasticsHandler(MABMEM_HEAP _heap,XboxOneMabStatisticsManager* _stats_mgr,SIFXboxOneLeaderboardQueue* _leaderboard_queue);
	~SIFXboxOneStasticsHandler();

	virtual void Update(MabObservable<SIFStatisticsHandlerMessage>* source, const SIFStatisticsHandlerMessage& msg);

	virtual void Update(MabObservable<XboxOneMabStatisticsManagerMessage>* source, const XboxOneMabStatisticsManagerMessage& msg);

	virtual void RegisterScorer(int , RUStatsScorer*);

	virtual void UnregisterScorer(int);

protected:
	

	/// A map from controller_id to stats_scorer
	MabMap< int, RUStatsScorer* > stats_scorer_map;


	SIFStatisticsHandlerMessage* most_recent_message;
	UINT next_message_id;

	XboxOneMabStatisticsManager* const stats_mgr; 

	SIFXboxOneLeaderboardQueue* leaderboard_queue;

	/// A helper function for performing stats writes.
	bool PerformStatsWrites( const SIFStatisticsHandlerMessage& msg );

	int pending_unregister;
};

#endif