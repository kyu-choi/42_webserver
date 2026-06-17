#include "webserv/EventLoop.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	const std::size_t kBufferSize = 4096;

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

	std::string fixedHttpResponse()
	{
		return (std::string("HTTP/1.1 200 OK\r\n")
			+ "Content-Type: text/plain\r\n"
			+ "Content-Length: 12\r\n"
			+ "Connection: close\r\n"
			+ "\r\n"
			+ "Hello World!");
	}
}

namespace webserv
{
	EventLoop::Client::Client()
		: requestBuffer(), responseBuffer(), bytesSent(0), responseReady(false)
	{
	}

	EventLoop::EventLoop(int listenFd)
		: _listenFd(listenFd), _pollFds(), _clients()
	{
		addFd(_listenFd, POLLIN);
	}

	void EventLoop::run()
	{
		std::cout << "webserv poll event loop started" << std::endl;
		while (true)
		{
			const int ready = poll(&_pollFds[0], _pollFds.size(), -1);

			if (ready < 0)
			{
				if (errno == EINTR)
					continue;
				throw std::runtime_error(systemError("poll"));
			}
			std::vector<int> readyFds;
			std::vector<short> readyEvents;
			for (std::size_t i = 0; i < _pollFds.size(); ++i)
			{
				if (_pollFds[i].revents != 0)
				{
					readyFds.push_back(_pollFds[i].fd);
					readyEvents.push_back(_pollFds[i].revents);
				}
			}
			for (std::size_t i = 0; i < readyFds.size(); ++i)
			{
				const int fd = readyFds[i];
				const short events = readyEvents[i];

				if (isListenFd(fd))
				{
					if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0)
						throw std::runtime_error("listen socket poll error");
					if ((events & POLLIN) != 0)
						handleListenEvent();
				}
				else
				{
					if (_clients.find(fd) == _clients.end())
						continue;
					if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0)
					{
						closeClient(fd);
						continue;
					}
					if ((events & POLLIN) != 0)
						handleClientRead(fd);
					if (_clients.find(fd) != _clients.end()
						&& (events & POLLOUT) != 0)
						handleClientWrite(fd);
				}
			}
		}
	}

	void EventLoop::addFd(int fd, short events)
	{
		pollfd item;

		item.fd = fd;
		item.events = events;
		item.revents = 0;
		_pollFds.push_back(item);
	}

	void EventLoop::updateEvents(int fd, short events)
	{
		for (std::size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].fd == fd)
			{
				_pollFds[i].events = events;
				_pollFds[i].revents = 0;
				return;
			}
		}
	}

	void EventLoop::removeFd(int fd)
	{
		for (std::vector<pollfd>::iterator it = _pollFds.begin();
			it != _pollFds.end(); ++it)
		{
			if (it->fd == fd)
			{
				_pollFds.erase(it);
				return;
			}
		}
	}

	bool EventLoop::isListenFd(int fd) const
	{
		return (fd == _listenFd);
	}

	void EventLoop::closeClient(int fd)
	{
		removeFd(fd);
		_clients.erase(fd);
		closeFd(fd);
	}

	void EventLoop::handleListenEvent()
	{
		while (true)
		{
			int clientFd = accept(_listenFd, 0, 0);

			if (clientFd < 0)
			{
				if (errno == EINTR)
					continue;
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					return;
				throw std::runtime_error(systemError("accept"));
			}
			try
			{
				setNonBlocking(clientFd);
				_clients[clientFd] = Client();
				addFd(clientFd, POLLIN);
				std::cout << "accepted client fd " << clientFd << std::endl;
			}
			catch (...)
			{
				closeFd(clientFd);
				throw;
			}
		}
	}

	void EventLoop::handleClientRead(int fd)
	{
		char	buffer[kBufferSize];
		bool	receivedAny;

		receivedAny = false;
		while (true)
		{
			const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);

			if (received > 0)
			{
				_clients[fd].requestBuffer.append(buffer,
					static_cast<std::size_t>(received));
				receivedAny = true;
				continue;
			}
			if (received == 0)
			{
				closeClient(fd);
				return;
			}
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			closeClient(fd);
			return;
		}
		if (receivedAny)
		{
			_clients[fd].responseBuffer = fixedHttpResponse();
			_clients[fd].bytesSent = 0;
			_clients[fd].responseReady = true;
			updateEvents(fd, POLLOUT);
		}
	}

	void EventLoop::handleClientWrite(int fd)
	{
		Client& client = _clients[fd];

		if (!client.responseReady)
		{
			updateEvents(fd, POLLIN);
			return;
		}
		while (client.bytesSent < client.responseBuffer.size())
		{
			const ssize_t sent = send(fd,
					client.responseBuffer.c_str() + client.bytesSent,
					client.responseBuffer.size() - client.bytesSent,
					0);
			if (sent > 0)
			{
				client.bytesSent += static_cast<std::size_t>(sent);
				continue;
			}
			if (sent == 0)
				break;
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			closeClient(fd);
			return;
		}
		closeClient(fd);
	}
}
