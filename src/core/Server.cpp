#include "webserv/EventLoop.hpp"
#include "webserv/Server.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	const int kListenBacklog = 128;

	std::string systemError(const std::string& context)
	{
		return (context + ": " + std::strerror(errno));
	}

	void setNonBlocking(int fd)
	{
		const int flags = fcntl(fd, F_GETFL, 0);

		if (flags < 0)
			throw std::runtime_error(systemError("fcntl(F_GETFL)"));
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
			throw std::runtime_error(systemError("fcntl(F_SETFL)"));
	}

	in_addr_t addressForHost(const std::string& host)
	{
		if (host == "127.0.0.1")
			return (htonl(INADDR_LOOPBACK));
		if (host == "0.0.0.0")
			return (htonl(INADDR_ANY));
		throw std::runtime_error("unsupported listen host for STEP03: " + host);
	}
}

namespace webserv
{
	Server::Server(const std::string& host, unsigned short port)
		: _host(host), _port(port), _listenFd(-1)
	{
		openListenSocket();
	}

	Server::~Server()
	{
		closeListenSocket();
	}

	void Server::run()
	{
		EventLoop eventLoop(_listenFd);

		std::cout << "webserv listening on " << _host << ":" << _port
				  << std::endl;
		eventLoop.run();
	}

	const std::string& Server::host() const
	{
		return (_host);
	}

	unsigned short Server::port() const
	{
		return (_port);
	}

	void Server::openListenSocket()
	{
		struct sockaddr_in	address;
		int					reuseAddress;

		_listenFd = socket(AF_INET, SOCK_STREAM, 0);
		if (_listenFd < 0)
			throw std::runtime_error(systemError("socket"));
		try
		{
			setNonBlocking(_listenFd);
		}
		catch (...)
		{
			closeListenSocket();
			throw;
		}
		reuseAddress = 1;
		if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR,
				&reuseAddress, sizeof(reuseAddress)) < 0)
		{
			closeListenSocket();
			throw std::runtime_error(systemError("setsockopt"));
		}
		std::memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_port = htons(_port);
		address.sin_addr.s_addr = addressForHost(_host);
		if (bind(_listenFd,
				reinterpret_cast<struct sockaddr*>(&address),
				sizeof(address)) < 0)
		{
			closeListenSocket();
			throw std::runtime_error(systemError("bind"));
		}
		if (listen(_listenFd, kListenBacklog) < 0)
		{
			closeListenSocket();
			throw std::runtime_error(systemError("listen"));
		}
	}

	void Server::closeListenSocket()
	{
		if (_listenFd >= 0)
		{
			close(_listenFd);
			_listenFd = -1;
		}
	}
}
