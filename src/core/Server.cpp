#include "webserv/Server.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
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

	void closeFd(int fd)
	{
		if (fd >= 0)
			close(fd);
	}

	in_addr_t addressForHost(const std::string& host)
	{
		if (host == "127.0.0.1")
			return (htonl(INADDR_LOOPBACK));
		if (host == "0.0.0.0")
			return (htonl(INADDR_ANY));
		throw std::runtime_error("unsupported listen host for STEP02: " + host);
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

	void Server::runOnce()
	{
		struct sockaddr_in	clientAddress;
		socklen_t			clientAddressLength;
		int					clientFd;

		std::memset(&clientAddress, 0, sizeof(clientAddress));
		clientAddressLength = sizeof(clientAddress);
		std::cout << "webserv listening on " << _host << ":" << _port
				  << std::endl;
		clientFd = accept(_listenFd,
				reinterpret_cast<struct sockaddr*>(&clientAddress),
				&clientAddressLength);
		if (clientFd < 0)
			throw std::runtime_error(systemError("accept"));
		try
		{
			sendFixedResponse(clientFd);
			closeFd(clientFd);
		}
		catch (...)
		{
			closeFd(clientFd);
			throw;
		}
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

	void Server::sendFixedResponse(int clientFd) const
	{
		const std::string	response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 12\r\n"
			"Connection: close\r\n"
			"\r\n"
			"Hello World!";
		std::size_t			totalSent;

		totalSent = 0;
		while (totalSent < response.size())
		{
			const ssize_t sent = send(clientFd,
					response.c_str() + totalSent,
					response.size() - totalSent,
					0);
			if (sent < 0)
			{
				if (errno == EINTR)
					continue;
				throw std::runtime_error(systemError("send"));
			}
			if (sent == 0)
				throw std::runtime_error("send: connection closed");
			totalSent += static_cast<std::size_t>(sent);
		}
	}
}
