#include "webserv/Signal.hpp"
#include <signal.h>

namespace
{
	volatile sig_atomic_t gShutdownRequested = 0;
}

namespace webserv
{
	void handleShutdownSignal(int signalNumber)
	{
		(void)signalNumber;
		gShutdownRequested = 1;
	}

	bool shutdownRequested()
	{
		return (gShutdownRequested != 0);
	}
}
