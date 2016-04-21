#include "pch.h"
#include "XboxOneMabThread.h"

namespace XboxOneMabLib {

	XboxOneMabRefThread::XboxOneMabRefThread( 
		UINT sendPeriodInMilliseconds, 
		uint32 threadAffinityMask,
		int priorityClass ) :
		m_sendEveryPeriodInMilliseconds(sendPeriodInMilliseconds),
		m_threadAffinityMask(threadAffinityMask),
		m_priorityClass(priorityClass),
		m_terminateThreadEvent(nullptr),
		m_threadHandle(nullptr),
		m_wakeupEventHandle(nullptr)
	{
		m_wakeupEventHandle = CreateEvent( NULL, false, false, NULL );
		InitializeCriticalSection(&m_threadManagementLock);
		m_terminateThreadEvent = CreateEvent( NULL, false, false, NULL );
		if ( !m_terminateThreadEvent )
		{
			throw E_UNEXPECTED;
		}

		m_threadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)XboxOneMabRefThread::StaticThreadProc, (LPVOID)this, CREATE_SUSPENDED, nullptr);
		SetThreadPriority(m_threadHandle, m_priorityClass );
#ifdef _TITLE
		SetThreadAffinityMask(m_threadHandle, m_threadAffinityMask);
#endif
		ResumeThread(m_threadHandle);
	}

	XboxOneMabRefThread::~XboxOneMabRefThread()
	{
		Shutdown();
		DeleteCriticalSection(&m_threadManagementLock);
	}

	UINT XboxOneMabRefThread::GetSendPeriod()
	{
		return m_sendEveryPeriodInMilliseconds;
	}
	void XboxOneMabRefThread::SetSendPeriod( UINT sendPeriodInMilliseconds)
	{
		m_sendEveryPeriodInMilliseconds = sendPeriodInMilliseconds;
		m_clock.SetInterval(m_sendEveryPeriodInMilliseconds);
	}

	void XboxOneMabRefThread::Shutdown()
	{
		EnterCriticalSection(&m_threadManagementLock);
		if( m_threadHandle != nullptr )
		{
			SetEvent(m_terminateThreadEvent);
			WaitForSingleObject(m_threadHandle, INFINITE);

			CloseHandle(m_threadHandle);
			m_threadHandle = nullptr;
			CloseHandle(m_threadHandle);
			m_threadHandle = NULL;
		}
		LeaveCriticalSection(&m_threadManagementLock);
	}

	DWORD WINAPI XboxOneMabRefThread::StaticThreadProc( XboxOneMabRefThread^ mySendThread )
	{
		return mySendThread->ThreadProc();
	}

	DWORD WINAPI XboxOneMabRefThread::ThreadProc()
	{
		m_clock.Initialize( m_sendEveryPeriodInMilliseconds );

		static const UINT c_uOneSecondInMS = 1000;
		LARGE_INTEGER m_timerFrequency;
		QueryPerformanceFrequency(&m_timerFrequency);

		while( m_clock.WaitForEventsOrHeartbeat( m_terminateThreadEvent, m_wakeupEventHandle ) != WAIT_OBJECT_0 )
		{
			auto args = ref new MabXboxOneThreadsEventArgs();
			OnDoWork(this, args);
		}

		return 0;
	}

	void XboxOneMabRefThread::WakeupThread()
	{
		SetEvent( m_wakeupEventHandle );
	}



}