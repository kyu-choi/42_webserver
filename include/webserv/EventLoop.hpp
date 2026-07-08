#ifndef WEBSERV_EVENT_LOOP_HPP
# define WEBSERV_EVENT_LOOP_HPP

# include "webserv/Client.hpp"
# include <map>
# include <poll.h>
# include <vector>

namespace webserv
{
	class EventLoop
	{
	public:
		explicit EventLoop(int listenFd);

		void	run();

	private:
		int						_listenFd;
		std::vector<pollfd>		_pollFds;
		std::map<int, Client>	_clients;

		EventLoop(const EventLoop& other);
		EventLoop&	operator=(const EventLoop& other);

		void	addFd(int fd, short events);
		void	updateEvents(int fd, short events);
		void	removeFd(int fd);
		bool	isListenFd(int fd) const;
		void	closeClient(int fd);

		void	handleListenEvent();
		void	handleClientRead(int fd);
		void	handleClientWrite(int fd);
		void	prepareFixedResponse(Client& client);
	};
}

#endif
