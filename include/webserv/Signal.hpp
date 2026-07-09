#ifndef WEBSERV_SIGNAL_HPP
# define WEBSERV_SIGNAL_HPP

namespace webserv
{
	void	handleShutdownSignal(int signalNumber);
	bool	shutdownRequested();
}

#endif
