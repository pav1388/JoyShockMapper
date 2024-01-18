#pragma once
#include "InputHelpers.h"
#include "JslWrapper.h"


class CmdRegistry;


namespace JSM
{

class AutoConnect : public PollingThread
{
public:
	AutoConnect(CmdRegistry* commandRegistry, bool start);
	void linkJslWrapper(shared_ptr<JslWrapper>* joyshock);
	virtual ~AutoConnect() = default;

private:
	bool AutoConnectPoll(void* param);
	shared_ptr<JslWrapper> jsl;
	int lastSize = 0;
};

} //JSM