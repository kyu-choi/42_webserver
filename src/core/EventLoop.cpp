#include "webserv/EventLoop.hpp"
#include "webserv/ResponseBuilder.hpp"
#include "webserv/StaticFileHandler.hpp"
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
	const std::size_t kMaxInputBufferSize = 1024 * 1024;

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

}

namespace webserv
{
	EventLoop::EventLoop(int listenFd, const std::string& root)
		: _listenFd(listenFd), _root(root), _pollFds(), _clients()
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
				_clients.insert(std::make_pair(clientFd,
						Client(clientFd, _listenFd)));
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
		std::map<int, Client>::iterator	it;
		Client*							client;
		char							buffer[kBufferSize];
		bool							receivedAny;

		it = _clients.find(fd);
		if (it == _clients.end())
			return;
		client = &it->second;
		receivedAny = false;
		while (true)
		{
			const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);

			if (received > 0)
			{
				client->appendInput(buffer, static_cast<std::size_t>(received));
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
			if (client->inputBuffer().size() > kMaxInputBufferSize)
			{
				prepareErrorResponse(*client, HTTP_STATUS_PAYLOAD_TOO_LARGE);
			}
			else
			{
				processClientInput(*client);
			}
			updateEvents(fd, client->desiredPollEvents());
		}
	}

	void EventLoop::handleClientWrite(int fd)
	{
		std::map<int, Client>::iterator it = _clients.find(fd);

		if (it == _clients.end())
			return;
		Client& client = it->second;

		if (!client.hasPendingOutput())
		{
			updateEvents(fd, client.desiredPollEvents());
			return;
		}
		while (client.hasPendingOutput())
		{
			const ssize_t sent = send(fd,
					client.pendingOutputData(),
					client.pendingOutputSize(),
					0);
			if (sent > 0)
			{
				client.advanceSendOffset(static_cast<std::size_t>(sent));
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
		client.markClosing();
		closeClient(fd);
	}

	void EventLoop::processClientInput(Client& client)
	{
		RequestParser::Result result;

		result = client.parser().parse(client.inputBuffer(), client.request());
		if (result == RequestParser::PARSE_INCOMPLETE)
		{
			if (client.parser().state() == PARSER_BODY_BY_LENGTH)
				client.setState(CLIENT_READING_BODY);
			else
				client.setState(CLIENT_READING_HEADERS);
			return;
		}
		client.consumeInput(client.parser().consumedBytes());
		if (result == RequestParser::PARSE_ERROR)
		{
			const int status = client.parser().errorStatus();

			client.request().setErrorStatus(status);
			prepareErrorResponse(client, status);
			client.parser().reset();
			return;
		}
		client.setState(CLIENT_PROCESSING_REQUEST);
		prepareSuccessResponse(client);
		client.parser().reset();
	}

	void EventLoop::prepareSuccessResponse(Client& client)
	{
		if (!isImplementedMethod(client.request().method()))
			prepareErrorResponse(client, HTTP_STATUS_NOT_IMPLEMENTED);
		else if (client.request().method() != HTTP_METHOD_GET)
			prepareErrorResponse(client, HTTP_STATUS_METHOD_NOT_ALLOWED);
		else
		{
			const StaticFileHandler handler(_root);

			client.setOutput(handler.handleGet(client.request()).serialize());
		}
	}

	void EventLoop::prepareErrorResponse(Client& client, int statusCode)
	{
		if (statusCode == 0)
			statusCode = HTTP_STATUS_BAD_REQUEST;
		client.setOutput(ResponseBuilder::error(statusCode).serialize());
	}
}
