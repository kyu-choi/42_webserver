#include "webserv/EventLoop.hpp"
#include "webserv/Server.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

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
		std::vector<unsigned long> parts;
		std::size_t start;

		start = 0;
		while (start <= host.size())
		{
			const std::size_t dot = host.find('.', start);
			const std::string part = host.substr(start,
				(dot == std::string::npos) ? std::string::npos : dot - start);
			char* end = 0;
			const unsigned long value = std::strtoul(part.c_str(), &end, 10);

			if (part.empty() || end == 0 || *end != '\0' || value > 255)
				throw std::runtime_error("invalid listen host: " + host);
			parts.push_back(value);
			if (dot == std::string::npos)
				break;
			start = dot + 1;
		}
		if (parts.size() != 4)
			throw std::runtime_error("invalid listen host: " + host);
		return (htonl((parts[0] << 24)
			| (parts[1] << 16)
			| (parts[2] << 8)
			| parts[3]));
	}

	std::string endpointKey(const webserv::ListenEndpoint& endpoint)
	{
		std::ostringstream stream;

		stream << endpoint.host << ":" << endpoint.port;
		return (stream.str());
	}
}

namespace webserv
{
	Server::ListenBinding::ListenBinding(
		int fdValue,
		std::size_t serverIndexValue,
		std::size_t listenIndexValue)
		: fd(fdValue),
		  serverIndex(serverIndexValue),
		  listenIndex(listenIndexValue)
	{
	}

	Server::Server(const std::string& host, unsigned short port)
		: _servers(), _listenBindings()
	{
		ServerConfig server = defaultServerConfig();
		ListenEndpoint endpoint;

		endpoint.host = host;
		endpoint.port = port;
		server.listen.clear();
		server.listen.push_back(endpoint);
		_servers.push_back(server);
		openListenSockets();
	}

	Server::Server(const ServerConfig& config)
		: _servers(), _listenBindings()
	{
		_servers.push_back(config);
		openListenSockets();
	}

	Server::Server(const std::vector<ServerConfig>& configs)
		: _servers(configs), _listenBindings()
	{
		openListenSockets();
	}

	Server::~Server()
	{
		closeListenSockets();
	}

	void Server::run()
	{
		std::vector<ListenSocketConfig> listeners;

		for (std::size_t i = 0; i < _listenBindings.size(); ++i)
		{
			listeners.push_back(ListenSocketConfig(
					_listenBindings[i].fd,
					_servers[_listenBindings[i].serverIndex]));
			std::cout << "webserv listening on "
					  << _servers[_listenBindings[i].serverIndex]
							.listen[_listenBindings[i].listenIndex].host
					  << ":" << _servers[_listenBindings[i].serverIndex]
							.listen[_listenBindings[i].listenIndex].port
					  << " (fd " << _listenBindings[i].fd << ")"
					  << std::endl;
		}
		EventLoop eventLoop(listeners);

		eventLoop.run();
	}

	const std::string& Server::host() const
	{
		return (_servers[0].listen[0].host);
	}

	unsigned short Server::port() const
	{
		return (_servers[0].listen[0].port);
	}

	std::size_t Server::listenSocketCount() const
	{
		return (_listenBindings.size());
	}

	void Server::openListenSockets()
	{
		std::vector<std::string> seenEndpoints;

		if (_servers.empty())
			throw std::runtime_error("config: no server block found");
		try
		{
			for (std::size_t serverIndex = 0;
				serverIndex < _servers.size(); ++serverIndex)
			{
				const ServerConfig& server = _servers[serverIndex];

				if (server.listen.empty())
					throw std::runtime_error("config: server has no listen endpoint");
				for (std::size_t listenIndex = 0;
					listenIndex < server.listen.size(); ++listenIndex)
				{
					const std::string key = endpointKey(server.listen[listenIndex]);

					for (std::size_t i = 0; i < seenEndpoints.size(); ++i)
					{
						if (seenEndpoints[i] == key)
							throw std::runtime_error(
								"config: duplicate listen endpoint: " + key);
					}
					seenEndpoints.push_back(key);
					_listenBindings.push_back(ListenBinding(
							createListenSocket(server.listen[listenIndex]),
							serverIndex,
							listenIndex));
				}
			}
		}
		catch (...)
		{
			closeListenSockets();
			throw;
		}
	}

	int Server::createListenSocket(const ListenEndpoint& endpoint)
	{
		struct sockaddr_in	address;
		int					reuseAddress;
		int					listenFd;

		listenFd = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFd < 0)
			throw std::runtime_error(systemError("socket"));
		try
		{
			setNonBlocking(listenFd);
		}
		catch (...)
		{
			closeFd(listenFd);
			throw;
		}
		reuseAddress = 1;
		if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR,
				&reuseAddress, sizeof(reuseAddress)) < 0)
		{
			closeFd(listenFd);
			throw std::runtime_error(systemError("setsockopt"));
		}
		std::memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_port = htons(endpoint.port);
		address.sin_addr.s_addr = addressForHost(endpoint.host);
		if (bind(listenFd,
				reinterpret_cast<struct sockaddr*>(&address),
				sizeof(address)) < 0)
		{
			closeFd(listenFd);
			throw std::runtime_error(systemError("bind"));
		}
		if (listen(listenFd, kListenBacklog) < 0)
		{
			closeFd(listenFd);
			throw std::runtime_error(systemError("listen"));
		}
		return (listenFd);
	}

	void Server::closeListenSockets()
	{
		for (std::size_t i = 0; i < _listenBindings.size(); ++i)
			closeFd(_listenBindings[i].fd);
		_listenBindings.clear();
	}
}
