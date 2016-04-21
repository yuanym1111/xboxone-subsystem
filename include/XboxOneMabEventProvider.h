#ifndef _XBOXONE_MAB_SESSION_SEARCH_MANAGER_H
#define _XBOXONE_MAB_SESSION_SEARCH_MANAGER_H

#include "MabString.h"
#include "XboxOneMabSingleton.h"
#include "XboxOneMabSubsystem.h"

const float FriendSessionUpdateInterval = 10.0f;


class XboxOneMabSessionMessage
{
public:
	enum EVENT_TYPE
	{
		SESSION_SEARCH_SEARCH_COMPLETED,
		SESSION_SEARCH_QOS_POLLING_COMPLETED
	};

public:
	XboxOneMabSessionMessage(bool _succeeded, EVENT_TYPE type, const char* data = NULL)
		: results(data), event_type(type), succeeded(_succeeded)
	{}

	XboxOneMabSessionMessage(DWORD _succeeded, EVENT_TYPE type, const char* data = NULL)
		: results(data), event_type(type), succeeded(SUCCEEDED(_succeeded))
	{}

	const char* GetSearchResults() const { return results; }

	const bool GetResult() const { return succeeded; }

	const EVENT_TYPE GetEventType() const { return event_type; }

private:
	const char* results;
	EVENT_TYPE event_type;
	bool succeeded;
};



class XboxOneMabEventProvider : public XboxOneMabSingleton<XboxOneMabEventProvider>, 
								public XboxOneMabSubsystem,
							public MabObservable< XboxOneMabSessionMessage >
{

public:
	XboxOneMabEventProvider();
	~XboxOneMabEventProvider();

	void Update(MabObservable<XboxOneMabSessionMessage> *source, const XboxOneMabSessionMessage &msg);

private:

	virtual bool Initialise();

	virtual void Update();

	virtual void CleanUp();

};

#endif