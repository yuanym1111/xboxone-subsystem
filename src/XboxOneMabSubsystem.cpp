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

#include "pch.h"

#include "XboxOneMabSubsystem.h"

static const unsigned int MAX_NUM_COMPONENTS_ESTIMATE = 20;

XboxOneMabSubsystemUpdater::XboxOneMabSubsystemUpdater( MABMEM_HEAP _heap )
	: heap( _heap )
{
	registered_components.reserve( MAX_NUM_COMPONENTS_ESTIMATE );
}

void XboxOneMabSubsystemUpdater::Register( XboxOneMabSubsystem* const component_to_register )
{
	registered_components.push_back( component_to_register );
}

void XboxOneMabSubsystemUpdater::Unregister( XboxOneMabSubsystem* const component_to_remove )
{
	for ( MabVector< XboxOneMabSubsystem* >::iterator iter = registered_components.begin(); iter != registered_components.end(); ++iter )
	{
		/// Note that--at the moment--this function is merely performing an address-wise comparison. Only if it's exactly the same
		///	component will the two compare as equal.
		if ( *iter == component_to_remove )
		{
			registered_components.erase( iter );
			return;
		}
	}

	MABBREAKMSG( "The component you're attempting to unregister hasn't been registered!" );
}

void XboxOneMabSubsystemUpdater::Initialise()
{
	for ( MabVector< XboxOneMabSubsystem* >::iterator iter = registered_components.begin(); iter != registered_components.end(); ++iter )
	{
		MABVERIFY( ( *iter )->Initialise() );
	}
}

void XboxOneMabSubsystemUpdater::Update()
{
	for ( MabVector< XboxOneMabSubsystem* >::iterator iter = registered_components.begin(); iter != registered_components.end(); ++iter )
	{
		( *iter )->Update();
	}
}

void XboxOneMabSubsystemUpdater::CleanUp()
{
	// Use a reverse iterator to ensure any dependancy chain is observed.
	MabVector< XboxOneMabSubsystem* >::reverse_iterator iter = registered_components.rbegin();
	while ( iter != registered_components.rend() )
	{
		// first call cleanup
		( *iter )->CleanUp();
		// then delete the object
		delete ( *iter );
		// Increment the iterator.
		++iter;
	}

	// Now clear the vector, it's contents are gone.
	registered_components.clear();
}