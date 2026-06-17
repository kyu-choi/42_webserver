#ifndef WEBSERV_SERVER_HPP
# define WEBSERV_SERVER_HPP

# include <string>

namespace webserv
{
	class Server
	{
	public:
		Server(const std::string& host, unsigned short port);
		~Server();

		void				run();
		const std::string&	host() const;
		unsigned short		port() const;

	private:
		Server(const Server& other);
		Server&	operator=(const Server& other);

		std::string		_host;
		unsigned short	_port;
		int				_listenFd;

		void	openListenSocket();
		void	closeListenSocket();
	};
}

#endif
