#ifndef XBOX_ONE_MAB_SESSION_THREAD_H
#define XBOX_ONE_MAB_SESSION_THREAD_H

#include <synchapi.h>

class Clock
{
public:
	Clock() : m_heartbeats(0), m_wakeUps(0)
	{
		m_timerFrequency.QuadPart = 0;
		m_timerStart.QuadPart = 0;
		m_countPerPeriod.QuadPart = 0;
	}

	~Clock()
	{
	}

	void Initialize( UINT uPeriodMS )
	{
		if (!QueryPerformanceFrequency(&m_timerFrequency))
		{
			assert(false); 
		}

		SetInterval(uPeriodMS);
	}

	void SetInterval( UINT uPeriodMS )
	{
		m_countPerPeriod.QuadPart = ( m_timerFrequency.QuadPart * uPeriodMS ) / c_uOneSecondInMS;
	}

	void SleepUntilNextHeartbeat()
	{
		LARGE_INTEGER timeOfNextHeartbeat = GetNextHeartbeat();
		LARGE_INTEGER timeNow;

		m_heartbeats++; // so that the next one will be 20ms (or whatever) later...

		if (!QueryPerformanceCounter(&timeNow))
		{
			assert(false); 
		}

		LONGLONG timeDelta = timeOfNextHeartbeat.QuadPart - timeNow.QuadPart;

		if ( timeDelta > 0 )
		{
			LONGLONG numberOfMilliSecondsSinceLast = c_uOneSecondInMS * timeDelta / m_timerFrequency.QuadPart;
			DWORD dwSleepTime = static_cast<DWORD>(numberOfMilliSecondsSinceLast);
			Sleep( dwSleepTime );
		}
	}

	DWORD WaitForEventsOrHeartbeat(HANDLE hObject, HANDLE hObject2)
	{
		LARGE_INTEGER liExpected = GetNextHeartbeat();
		LARGE_INTEGER liNow;

		m_wakeUps = 0;
		m_heartbeats++; // so that the next one will be 20ms (or whatever) later...

		if (!QueryPerformanceCounter(&liNow))
		{
			assert(false); 
		}

		LONGLONG llDiff = liExpected.QuadPart - liNow.QuadPart;

		HANDLE aObjects[2];
		aObjects[0] = hObject;
		aObjects[1] = hObject2;

		if ( llDiff > 0 )
		{
			DWORD dwSleepTime = static_cast<DWORD>(c_uOneSecondInMS * llDiff / m_timerFrequency.QuadPart);
			m_wakeUps++;

			return ::WaitForMultipleObjectsEx( 2, aObjects, false, dwSleepTime, FALSE );
		}

		// we're already late, no need to wait, but we still need to test the object
		m_wakeUps++;
		return ::WaitForMultipleObjectsEx( 2, aObjects, false, 0, FALSE );
	}

	UINT m_wakeUps;
private:
	static const UINT c_uOneSecondInMS = 1000;
	UINT m_heartbeats;


	LARGE_INTEGER m_timerFrequency;
	LARGE_INTEGER m_timerStart;
	LARGE_INTEGER m_countPerPeriod;

	LARGE_INTEGER GetNextHeartbeat()
	{
		LARGE_INTEGER timeExpected;

		// Update Start
		if ( m_timerStart.QuadPart == 0 )
		{
			if (!QueryPerformanceCounter( &m_timerStart ))
			{
				assert(false); 
			}
			m_heartbeats = 1; // we are on the very first heartbeat
		}

		timeExpected.QuadPart = m_timerStart.QuadPart + (m_countPerPeriod.QuadPart * m_heartbeats);
		return timeExpected;
	}
};

namespace XboxOneMabLib {

public ref class MabXboxOneThreadsEventArgs sealed
{
public:

internal:
	MabXboxOneThreadsEventArgs(){};
};

public ref class XboxOneMabRefThread sealed
{
public:
	event Windows::Foundation::EventHandler<MabXboxOneThreadsEventArgs^>^ OnDoWork;
	XboxOneMabRefThread( UINT sendPeriodInMilliseconds, uint32 threadAffinityMask, int priorityClass );
	UINT GetSendPeriod( );
	void SetSendPeriod( UINT sendPeriodInMilliseconds);
	void WakeupThread();

	virtual ~XboxOneMabRefThread();

internal:
	void Shutdown();
	static DWORD WINAPI StaticThreadProc(XboxOneMabRefThread^ networkSendThread);
	DWORD ThreadProc();

private:

	uint32 m_threadAffinityMask;
	CRITICAL_SECTION m_threadManagementLock;
	HANDLE m_terminateThreadEvent;
	HANDLE m_threadHandle;
	int m_priorityClass;
	UINT m_sendEveryPeriodInMilliseconds;
	HANDLE m_wakeupEventHandle;
	Clock m_clock;
};

}

#endif