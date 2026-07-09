#ifndef WEBSERV_EVENT_LOOP_HPP
# define WEBSERV_EVENT_LOOP_HPP

# include "webserv/CgiHandler.hpp"
# include "webserv/Client.hpp"
# include "webserv/Config.hpp"
# include <ctime>
# include <map>
# include <poll.h>
# include <string>
# include <sys/types.h>
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
		~EventLoop();

		void	run();

	private:
		struct CgiJob
		{
			pid_t							pid;
			int								clientFd;
			int								stdinFd;
			int								stdoutFd;
			std::string						requestBody;
			std::size_t						stdinOffset;
			std::string						output;
			std::time_t						startTime;
			bool							stdinClosed;
			bool							stdoutClosed;
			bool							childReaped;
			int								childStatus;
			std::map<int, std::string>		errorPages;
			std::string						errorRoot;

			CgiJob();
			CgiJob(
				int clientFdValue,
				const CgiExecution& execution,
				const std::map<int, std::string>& errorPagesValue,
				const std::string& errorRootValue);
		};

		struct Session
		{
			unsigned int	visits;
			std::time_t		createdAt;
			std::time_t		lastAccess;

			Session();
			explicit Session(std::time_t now);
		};

		std::vector<pollfd>				_pollFds;
		std::map<int, Client>			_clients;
		std::map<int, ServerConfig>		_serversByListenFd;
		std::map<int, CgiJob>			_cgiJobs;
		std::map<int, int>				_cgiClientByFd;
		std::vector<pid_t>				_orphanedCgiPids;
		std::map<std::string, Session>	_sessions;
		unsigned long					_sessionCounter;

		EventLoop(const EventLoop& other);
		EventLoop&	operator=(const EventLoop& other);

		void	addFd(int fd, short events);
		void	updateEvents(int fd, short events);
		void	removeFd(int fd);
		bool	isListenFd(int fd) const;
		bool	isCgiFd(int fd) const;
		void	closeClient(int fd);
		void	closeAllClients();
		void	closeTimedOutClients();

		void	handleListenEvent(int listenFd);
		void	handleClientRead(int fd);
		void	handleClientWrite(int fd);
		void	handleCgiEvent(int fd, short events);
		void	handleCgiWrite(CgiJob& job);
		void	handleCgiRead(CgiJob& job);
		void	processClientInput(Client& client);
		bool	prepareEarlyBodyLimitResponse(Client& client);
		void	prepareSuccessResponse(Client& client);
		void	prepareSessionResponse(Client& client);
		void	purgeExpiredSessions(std::time_t now);
		void	enforceSessionLimit();
		std::string	createSessionId();
		void	startCgiResponse(
					Client& client,
					const RouteResult& route,
					const ServerConfig& server);
		void	checkCgiJobs();
		void	reapOrphanedCgiPids();
		void	closeCgiFd(CgiJob& job, int& fd);
		void	cleanupCgiForClient(int clientFd, bool terminateChild);
		void	failCgiJob(int clientFd, int statusCode, bool terminateChild);
		void	completeCgiJob(int clientFd);
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
