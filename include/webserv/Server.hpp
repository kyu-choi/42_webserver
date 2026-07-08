#ifndef WEBSERV_SERVER_HPP
# define WEBSERV_SERVER_HPP

# include "webserv/Config.hpp"
# include <string>
# include <vector>

namespace webserv
{
	class Server
	{
	public:
		Server(const std::string& host, unsigned short port);
		explicit Server(const ServerConfig& config);
		explicit Server(const std::vector<ServerConfig>& configs);
		~Server();

		void				run();
		const std::string&	host() const;
		unsigned short		port() const;
		std::size_t			listenSocketCount() const;

	private:
		struct ListenBinding
		{
			int			fd;
			std::size_t	serverIndex;
			std::size_t	listenIndex;

			ListenBinding(
				int fdValue,
				std::size_t serverIndexValue,
				std::size_t listenIndexValue);
		};

		Server(const Server& other);
		Server&	operator=(const Server& other);

		std::vector<ServerConfig>	_servers;
		std::vector<ListenBinding>	_listenBindings;

		void	openListenSockets();
		int		createListenSocket(const ListenEndpoint& endpoint);
		void	closeListenSockets();
	};
}

#endif
