#include "AutoConnect.h"
#include "JslWrapper.h"
#include "InputHelpers.h"


namespace JSM
{

AutoConnect::AutoConnect(CmdRegistry* commandRegistry, bool start)
  : PollingThread("AutoConnect thread", std::bind(&AutoConnect::AutoConnectPoll, this, std::placeholders::_1), (void*)commandRegistry, 1000, start)
{
}

bool AutoConnect::AutoConnectPoll(void* param)
{
	//connectDevices();
	//int deviceHandleArray[100];
	//int size = 100;
	//int real_size = jsl->GetConnectedDeviceHandles(deviceHandleArray, size);
	int realSize = jsl->GetDeviceCount();
	if(lastSize != realSize)
	{
		COUT_INFO << "Found " << realSize << " devices, expected " << lastSize << "; reloading" << endl;
		lastSize = realSize;
		WriteToConsole("RECONNECT_CONTROLLERS");
	}
	return true;
}

void AutoConnect::linkJslWrapper(shared_ptr<JslWrapper>* joyshock)
{
	jsl = *joyshock;
	COUT_INFO << "Linked Joyshock to AutoConnect" << endl;
}

} // namespace JSM
