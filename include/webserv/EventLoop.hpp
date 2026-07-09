#ifndef WEBSERV_EVENT_LOOP_HPP
# define WEBSERV_EVENT_LOOP_HPP

# include "webserv/Client.hpp"
# include "webserv/Config.hpp"
# include <map>
# include <poll.h>
# include <vector>

namespace webserv
{
	struct ListenSocketConfig
	{
		int				fd;
		ServerConfig	server;

		ListenSocketConfig(int fdValue, const ServerConfig& serverValue);
	};

	class EventLoop
	{
	public:
		explicit EventLoop(const std::vector<ListenSocketConfig>& listeners);

		void	run();

	private:
		std::vector<pollfd>				_pollFds;
		std::map<int, Client>			_clients;
		std::map<int, ServerConfig>		_serversByListenFd;

		EventLoop(const EventLoop& other);
		EventLoop&	operator=(const EventLoop& other);

		void	addFd(int fd, short events);
		void	updateEvents(int fd, short events);
		void	removeFd(int fd);
		bool	isListenFd(int fd) const;
		void	closeClient(int fd);
		void	closeTimedOutClients();

		void	handleListenEvent(int listenFd);
		void	handleClientRead(int fd);
		void	handleClientWrite(int fd);
		void	processClientInput(Client& client);
		bool	prepareEarlyBodyLimitResponse(Client& client);
		void	prepareSuccessResponse(Client& client);
		void	prepareErrorResponse(Client& client, int statusCode);
		void	prepareErrorResponse(
					Client& client,
					int statusCode,
					const std::map<int, std::string>& errorPages,
					const std::string& root);
		void	prepareMethodNotAllowedResponse(
					Client& client,
					const std::vector<HttpMethod>& allowedMethods,
					const std::map<int, std::string>& errorPages,
					const std::string& root);
		const ServerConfig*	serverForClient(const Client& client) const;
	};
}

#endif
