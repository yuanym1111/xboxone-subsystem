#include "pch.h"
#include "XboxOneMabEventProvider.h"
#include "XboxOneMabStateManager.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUtils.h"

using namespace XboxOneMabLib;

using namespace Concurrency;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services::Multiplayer;

XboxOneMabEventProvider::XboxOneMabEventProvider()
{

}

XboxOneMabEventProvider::~XboxOneMabEventProvider()
{

}


bool XboxOneMabEventProvider::Initialise()
{
	throw std::logic_error("The method or operation is not implemented.");
}

void XboxOneMabEventProvider::Update()
{
	throw std::logic_error("The method or operation is not implemented.");
}

void XboxOneMabEventProvider::CleanUp()
{
	throw std::logic_error("The method or operation is not implemented.");
}
