#ifndef _XBOXONE_MAB_STATISTICS_ENTITY_H
#define _XBOXONE_MAB_STATISTICS_ENTITY_H

#include "XboxOneMabSingleton.h"
#include <collection.h>
/// Column pair contains the string key and the id for the column
class XboxOneLeaderboardColumnData
{
public:
	XboxOneLeaderboardColumnData( const MabString &_gametag, WORD _xboxuserid, const uint32 _rank,const MabVector<MabString>& _value )
		: gametag(_gametag), xboxuserid(_xboxuserid), rank(_rank)
	{
		value = _value;

	}
	const MabString& GetName() const { return gametag; }
	const MabString& GetID() const { return xboxuserid; }

private:
	MabString gametag;
	MabString xboxuserid;
	float percentile;
	uint32 rank;
	MabVector<MabString> value;

};

class MabXBoxOneLeaderboardColumnDefinition {
public:

	enum SORT_ORDER{
		SORT_DESCENDING,
		SORT_ASCENDING
	};

	MabXBoxOneLeaderboardColumnDefinition(
		const MabString& _displayName,
		const MabString& _statisticName,
		const MabString& _typeName = "Int64",
		int _ruGameScoreType = 0,
		SORT_ORDER _sort_order = SORT_DESCENDING) //< Should be set to "" if you don't want the column to be seen.
		:
	displayName(_displayName),
		statisticName(_statisticName),
		typeName(_typeName),
		sort_order(_sort_order),
		type_byte(_ruGameScoreType)
	{

		// Populate the Type byte given the type string
		/*
		const MabMap< MabString, BYTE >& tmp_string_data_map = MabXBoxOneLeaderboardColumnDefinition::GetStringToDataTypeMap();
		MabMap< MabString, BYTE >::const_iterator tmp_string_data_pair = tmp_string_data_map.find( typeName );
		MABASSERTMSG( tmp_string_data_pair != tmp_string_data_map.end(), "Type string passed is in unrecognised! See Xbox360MabStatisticsManager's Initialise method for supported strings!" );
		if ( tmp_string_data_pair != tmp_string_data_map.end() ) type_byte = tmp_string_data_pair->second;
		*/
	}

	inline const MabString& GetName() const {return displayName;}
	inline const int GetID() const {return type_byte;}


	/// Accessor for string_to_data_type_map--see comments for that member for more info
	static MabMap< MabString, BYTE > string_to_data_type_map;
	static const MabMap< MabString, BYTE >& GetStringToDataTypeMap() { return MabXBoxOneLeaderboardColumnDefinition::string_to_data_type_map; };
private:
	/// The key of the string table entry that has the name of this leaderboard column.
	/// Can be set to "" if the column name is never going to be seen.
	MabString displayName;
	MabString statisticName;
	SORT_ORDER sort_order;
	MabString typeName;
	int32 type_byte;
};

typedef MabVector < MabXBoxOneLeaderboardColumnDefinition > XboxOneLeaderboardColumnDefinitionVector;


class MabXBoxOneLeaderboardRowDefinition{
	public:
		MabXBoxOneLeaderboardRowDefinition(const MabString &_leaderboardName = "", uint32 _table_title_key = 0)
			:leaderboardName(_leaderboardName),table_title_key(_table_title_key)
		{
			table_columns.clear();
		}
		~MabXBoxOneLeaderboardRowDefinition(){

		};
		void SetTotalCount(uint32 _totalCount){totalCount = _totalCount;}
		void SetDisplayName(const MabString &_displayName){displayName = _displayName;}
		void addColumn( MabXBoxOneLeaderboardColumnDefinition column){table_columns.push_back(column);}
	public:
		MabString leaderboardName;
		uint32 table_title_key;
		MabString displayName;
		uint32 totalCount;
		XboxOneLeaderboardColumnDefinitionVector table_columns;
};

typedef MabMap < unsigned long, MabXBoxOneLeaderboardRowDefinition > XboxOneLeaderboardRowDefinitionMap;

#endif