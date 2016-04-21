#ifndef _XBOXONE_MAB_ACHIEVEMENT_MANAGER_H
#define _XBOXONE_MAB_ACHIEVEMENT_MANAGER_H

#include "MabString.h"
#include "XboxOneMabSingleton.h"
#include <collection.h>

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Achievements;
using namespace Windows::Xbox::System;

class XboxOneMabAchievementManager : public XboxOneMabSingleton<XboxOneMabAchievementManager>
{

public:

	XboxOneMabAchievementManager(int _num_achievements = 0);
	~XboxOneMabAchievementManager();

	void Initilize();
	bool SetUser(User^ _user);
	bool SetLiveContext(XboxLiveContext^ _liveContext);

	void RequestAchievementsForTitleId(int controller_index);

	void RequestAchievement( int controller_index, Platform::String^ achievementId);

	/// Reinitialise the static data and reallocate buffer memory--should generally be coupled with calls to PerformSystemAchievementDataLoad
	bool Reinitialise( int _num_achievements );

	/// Clears the achievement data for a given controller - should be called on Sign in. Ideally, this subsystem should actually just listen for
	/// these events, but given where we are in MK it's really too risky to introduce anything unpredictable - MK code will just call this directly
	/// when we need to clear the achievement data.
	void ClearAchievementDataForController( int controller_index );

	/// This function will specify whether the achievements have been loaded at least once--there
	///    is no guarantee the load is current, however. See PerformSystemAchievementDataLoad.
	bool HasSystemDataBeenLoaded( int controller_index ) { return have_achievements_been_loaded_once.at( controller_index ); }

	/// Determine whether a given achievement has been obtained
	bool IsAchievementObtained( int controller_index, Platform::String^ achievement_id) const;


	/// @name Accessor functions
	/// @{
	inline int GetNumAchievements() const { return m_num_achievements; }
	int GetAchievementGamerscore( Platform::String^ achievement_id ) const;
	void GetAchievementDisplayString( Platform::String^ achievement_id, MabString& string_out ) const;
	void GetAchievementDescriptionString( Platform::String^ achievement_id, MabString& string_out ) const;
	/// We're only able to retrieve the "Unachieved" string, which appears to be different that the HOWTO string,
	///	which may only be used for Rich Presence or on the Dashboard. In any case, we don't seem to need to worry
	///	about the How-To string; uncomment and rename if we ever want to get the Unachieved string.
	//void GetAchievementHowToString( unsigned int achievement_id, MabString& string_out ) const;
	/// @}

	/// Unlock the given achievement.
	///	NOTE: Obviously, this function cannot be called with multiple achievements. If multiple
	///	achievements should by simultaneously awarded, use the UnlockAchievements function--however,
	///	be aware that this will display the text "n Achievements Totalling X G[gamerpoints] awarded";
	///	multiple calls to this function will instead result in a queue of achievements so that the user 
	/// sees "Achievement A awarded - X G" immediately followed by "Achievement B awarded - X G".
	/// It's been informally decided by Jim Simmons that we prefer to use the latter rather than the former approach.
	void UnlockAchievement( int controller_index, Platform::String^ achievement_id );

	/// Unlock the given achievements. 
	///	NOTE: If submitted with this method, multiple achievements will yield the on-screen message
	///	"n Achievements Totalling x G[gamerpoints] awarded". See UnlockAchievement.
	void UnlockAchievements( int controller_index, MabVector<Platform::String^>& achievement_ids);

	void UserChanged();

	//Get all achievements for a user ID per title.
	void GetAchievementsCountForCurrentUser();
	void GetAchievementsProgressForCurrentUser(float& percent);
	unsigned int GetTotalAchievementsCount() { return m_total_achievements_count; }
	void RefreshAchievementProgress();

private:

	void LoadAchievementData(IVectorView<Achievement^>^ achievementList);
	void LoadAchievementData(Achievement^ achievement);

	int m_num_achievements;
	unsigned int m_total_achievements_count;
	unsigned int m_total_unlocked_achievements_count;
	unsigned int m_total_locked_achievements_count;
	XboxLiveContext^ m_xboxLiveContext;
	uint32 m_titleId;
	User^ m_user;
	Platform::String^ m_xboxUserId;
	Platform::String^ m_serviceConfigId;

	/// A controller_id-indexed list of whether achievements have been loaded
	MabVector< bool > have_achievements_been_loaded_once;

	/// A field that retains the controller id for the most recently-performed achievement data load.
	/// NOTE: Should only be used for the accessors which don't depend on controller_id, e.g. GetAchievementGamerscore,
	///	GetAchievementDisplayString, GetAchievementDescriptionString.
	int last_loaded_controller_id;

	/// A controller_id-indexed map from achievement_id to whether or not it's been awarded.
	/// Implemented as an array of bitmasks, shifted by the achievement id. This assumes we'll never be awarding more than 64 achievements!
	///	When we ship our AAA title with three different expansion packs, we'll have to change this.
	MabUInt64 awarded_map[XBOX_ONE_MAX_USERS] ;

	Concurrency::critical_section m_achievementDataLock;

	Vector<Achievement^>^ m_achievementList;

	MabVector< std::pair<Platform::String^, Platform::String^> > m_descriptionDataList;
	MabVector< std::pair<Platform::String^, Platform::String^> > m_displayDataList;
	MabVector<std::pair<Platform::String^, int>> m_scoreDataList;
	MabVector<Platform::String^> m_achievements_in_progress;  //ID is a string

//	IVectorView<Achievement^>^ m_achievementList;

	Platform::String^ m_achievementId;
};


#endif