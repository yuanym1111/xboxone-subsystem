/**
 * @file XboxOneMabUISocialService.h
 * @Xbox One social service class.
 * @author jaydens
 * @20/03/2015
 */

#include <string>

enum XBOX_ONE_ACCOUNT_TIER_ENUM {
	UNKNOWN = 0,
	XBOX_ONE_ACCOUNT_TIER_SILVER,
	XBOX_ONE_ACCOUNT_TIER_NEWUSER,
	XBOX_ONE_ACCOUNT_TIER_GOLD,
	XBOX_ONE_ACCOUNT_TIER_FAMILY_GOLD
};

class XboxOneMabUISocialService {
public:
	static XBOX_ONE_ACCOUNT_TIER_ENUM GetXboxOneAccountTier(const std::string& xbox_user_id);
	static XBOX_ONE_ACCOUNT_TIER_ENUM GetXboxOneAccountTier(Platform::String^ xbox_user_id);
	static std::string GetXboxOneGameDisplayName(const std::string& xbox_user_id);
	static MabString GetFirstActiveXboxOneUserId();
	static void DecideActiveUser();
	static bool XboxOneStringValidation(const MabString &str, MabString &reason);
	static bool XboxOneStringsValidation(const MabVector<const char*> &strings, MabVector<bool>& results);

};