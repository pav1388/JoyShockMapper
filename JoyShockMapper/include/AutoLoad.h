#pragma once
#include "InputHelpers.h"

class CmdRegistry;

namespace patate
{
class AutoLoad : public PollingThread
{
public:
	AutoLoad(CmdRegistry* commandRegistry, bool start);

	virtual ~AutoLoad() = default;

private:
	bool AutoLoadPoll(void* param);
};
} // namespace patate