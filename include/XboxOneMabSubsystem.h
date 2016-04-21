/*--------------------------------------------------------------
|        Copyright (C) 1997-2007 by Prodigy Design Ltd         |
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

#ifndef XBOXONE_MAB_SUBSYSTEM
#define XBOXONE_MAB_SUBSYSTEM


///
/// Class to encapsulate the xbox system notification message
///
class XboxOneMabSystemNotificationMessage;

/// XboxOneMabSubsystemUpdater is meant to be a repository for all application-specific operations that need to be
///	updated every frame. See the wiki page SIFPlatformSpecificApplicationProcessorProposal for a fuller description.
///
///	@author Mark Barrett

/// This is an abstract class that every platform-specific bundle of functionality should inherit from.
///	Inheritors from this class should be passed to XboxOneMabSubsystemUpdater::Register().
class XboxOneMabSubsystem
{
public:
	virtual bool Initialise() = 0;
	virtual void Update() = 0;
	virtual void CleanUp() = 0;

	virtual ~XboxOneMabSubsystem() {};
};

/// This class is the manager that processes all platform-specific pieces of functionality that need to
///	be updated once a frame.
class XboxOneMabSubsystemUpdater
{
public:

	///This class has a public constructor and destructor because it's not static. If you pass a MABMEM_HEAP in here,
	///	you don't have to pass it in to either Initialise or the Register methods. However, if a heap is sent to
	///	either of those methods, that heap will be used instead of this one.
	XboxOneMabSubsystemUpdater( MABMEM_HEAP _heap );
	virtual ~XboxOneMabSubsystemUpdater() {};

	/// Use this method to register SIFPlatformSpecificApplications for handling by this Manager--subsequent calls to this
	///	class's other methods will involve all Registered components.
	///	@param component_to_add			The component you wish to register.
	void Register( XboxOneMabSubsystem* const component_to_add );

	/// Use this method to unregister a given XboxOneMabSubsystem; it won't be associated with this class's methods anymore.
	/// Note that Cleanup of this class must then be left to the caller of this function.
	void Unregister( XboxOneMabSubsystem* const component_to_remove );

	/// Call this to call the Initialise methods on all Registered components.
	void Initialise();

	/// Call this to call the Update methods of all Registered components.
	void Update();

	/// Call this to call the Cleanup methods of all Registered components and then delete them.
	void CleanUp();

private:
	/// The list of registered components
	MabVector< XboxOneMabSubsystem* > registered_components;
	/// The default heap for this class
	MABMEM_HEAP heap;
};

#endif //XBOXONE_MAB_SUBSYSTEM