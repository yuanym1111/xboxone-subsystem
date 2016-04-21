//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <xdk.h>

#include <SIDCFramework.h>

#include <iostream>
//#include <ifaddrs.h>
#include <ws2tcpip.h>
#include <MabCoreLib.h>
#include <MabMemLib.h>
#include <xboxone/MabWindows.h>
#include "MabLog.h"


#include <map>
#include <mutex>

#include <ppltasks.h>
#include <Robuffer.h>

const int XBOX_ONE_MAX_USERS = 8;

#define MATCH_MAKING_DURATION   ((long long) 36000000000) /* 1 hour in 100-nanosecond units */
#define TICKS_PER_MILLISECOND   (10 * 1000)
#define TICKS_PER_SECOND		(10 * 1000 * 1000)

#define XboxOneMabNetGame	XboxOneMabMatchMaking::Get()
#define XboxOneSessionManager	XboxOneMabSessionManager::Get()
#define XboxOneStateManager	XboxOneMabStateManager::Get()
#define XboxOneGame	XboxOneMabGame::Get()
#define XboxOneUser	XboxOneMabUserInfo::Get()
#define XboxOneMessageController	XboxOneMabUISessionController::Get()
#define XboxOneStatistics			XboxOneMabStatisticsManager::Get()
#define XboxOneAchievement			XboxOneMabAchievementManager::Get()
#define XboxOneVoice				XboxOneMabVoiceManager::Get()