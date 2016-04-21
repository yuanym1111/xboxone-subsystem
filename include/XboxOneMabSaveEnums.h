/**
 * @file XboxOneMabSaveEnums.h
 * @Xbox One save manager enum class.
 * @author jaydens
 * @15/04/2015
 */

#ifndef XBOXONE_MAB_SAVE_ENUMS_H
#define XBOXONE_MAB_SAVE_ENUMS_H

namespace XBOXONE_MAB_SAVE_ENUMS
{
	/// A list of possible ways to write save games; makes it possible to specify that
	///	you only want to overwrite, or only want to save a new game, and throw an error
	///	and not save if this functionality is not met
	enum WRITE_MODE
	{
		WRITE_MODE_ANY = 0,		//< will write whether or not file exists
		CREATE_NEW_ONLY,		//< will fail if file already exists
		OVERWRITE_ONLY			//< will fail if the file does not exist
	};

	/// An enum describing when to throw an error message about the removal of the device that the player has selected
	enum DEVICE_REMOVAL_WARNING_MODE
	{
		DEVICE_REMOVAL_WARNING_NONE,		//< will never throw a warning, just return false on StartSave();
		DEVICE_REMOVAL_WARNING_IMMEDIATE,	//< will throw a warning immediately when the player-selected device is removed;
		DEVICE_REMOVAL_WARNING_ON_SAVE,		//< will throw a warning when you attempt to save a game on a device that's been removed;
		DEVICE_REMOVAL_WARNING_ALL			//< will throw a warning in *both* of the conditions listed above; default
	};

	/// This is the enumerated type representing the different ways that you can check for equality between two save games. Everything
	///	except ID can be |'d together. Note: This enum more properly belongs in Data, but if I include Data I get a circular dependency.
	enum EQUALITY_TYPE
	{
		DEVICE			= 0x0001,		//< Compare the two save files' device_ids
		DISPLAY_NAME	= 0x0002,		//< Compare the two save files' display names
		SYSTEM_NAME		= 0x0004,		//< Compare the two save files' system names
		BUFFER			= 0x0008,		//< Compare the two save files' buffers

		ID				= 0xF000		//< Compare the two save files' IDs (means we ignore all the other fields)
	};

	/// The default type of equality for the operator==; presently is device + system name
	/// We're getting rid of DISPLAY_NAME because we could theoretically have different display names
	///	for identical files if we're messing around with translations.
	const EQUALITY_TYPE EQUALITY_DEFAULT = ( EQUALITY_TYPE ) ( DEVICE | /*DISPLAY_NAME |*/ SYSTEM_NAME );
}

#endif // #ifndef XBOXONE_MAB_SAVE_ENUMS_H