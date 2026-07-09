#include "webserv/EventLoop.hpp"
#include "webserv/DeleteHandler.hpp"
#include "webserv/ErrorPageHandler.hpp"
#include "webserv/ResponseBuilder.hpp"
#include "webserv/Router.hpp"
#include "webserv/Signal.hpp"
#include "webserv/StaticFileHandler.hpp"
#include "webserv/UploadHandler.hpp"
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
	const std::size_t kBufferSize = 4096;
	const std::size_t kMaxClients = 1024;
	const std::size_t kMaxRawInputBufferSize = 64 * 1024 * 1024;
	const std::size_t kMaxCgiOutputSize = 10 * 1024 * 1024;
	const std::size_t kMaxSessions = 64;
	const int kPollTimeoutMs = 1000;
	const std::time_t kClientTimeoutSeconds = 30;
	const std::time_t kCgiTimeoutSeconds = 3;
	const std::time_t kSessionTtlSeconds = 3;
	const char* kSessionCookieName = "WSID";

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

	bool methodAllowed(
		webserv::HttpMethod method,
		const std::vector<webserv::HttpMethod>& allowedMethods)
	{
		for (std::size_t i = 0; i < allowedMethods.size(); ++i)
		{
			if (allowedMethods[i] == method)
				return (true);
		}
		return (false);
	}

	std::string methodNames(
		const std::vector<webserv::HttpMethod>& methods)
	{
		std::string names;

		for (std::size_t i = 0; i < methods.size(); ++i)
		{
			if (i != 0)
				names += ", ";
			names += webserv::httpMethodName(methods[i]);
		}
		return (names);
	}

	webserv::HttpResponse configuredErrorResponse(
		int statusCode,
		const std::map<int, std::string>& errorPages,
		const std::string& root)
	{
		std::string body;
		std::string contentType;

		if (!webserv::ErrorPageHandler::loadCustomErrorPage(
				statusCode,
				errorPages,
				root,
				body,
				contentType))
		{
			body = webserv::ErrorPageHandler::defaultErrorPage(statusCode);
			contentType = "text/html";
		}
		return (webserv::ResponseBuilder::error(
				statusCode,
				body,
				contentType));
	}

	bool bodyTooLarge(
		const webserv::HttpRequest& request,
		std::size_t clientMaxBodySize)
	{
		if (request.hasContentLength()
			&& request.contentLength() > clientMaxBodySize)
			return (true);
		return (request.body().size() > clientMaxBodySize);
	}

	bool parserStateReadsBody(webserv::RequestParserState state)
	{
		return (state == webserv::PARSER_BODY_BY_LENGTH
			|| state == webserv::PARSER_BODY_CHUNK_SIZE
			|| state == webserv::PARSER_BODY_CHUNK_DATA
			|| state == webserv::PARSER_BODY_CHUNK_CRLF
			|| state == webserv::PARSER_BODY_CHUNK_TRAILERS);
	}

	std::string trim(const std::string& value)
	{
		std::size_t begin;
		std::size_t end;

		begin = 0;
		while (begin < value.size()
			&& (value[begin] == ' ' || value[begin] == '\t'))
			++begin;
		end = value.size();
		while (end > begin
			&& (value[end - 1] == ' ' || value[end - 1] == '\t'))
			--end;
		return (value.substr(begin, end - begin));
	}

	std::string numberToString(unsigned long value)
	{
		std::ostringstream stream;

		stream << value;
		return (stream.str());
	}

	bool isValidSessionId(const std::string& value)
	{
		if (value.size() != 32)
			return (false);
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			const char c = value[i];

			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
				return (false);
		}
		return (true);
	}

	std::string cookieValue(
		const std::string& cookieHeader,
		const std::string& name)
	{
		std::size_t start;

		start = 0;
		while (start < cookieHeader.size())
		{
			std::size_t end;
			std::string token;
			std::size_t equal;

			end = cookieHeader.find(';', start);
			if (end == std::string::npos)
				end = cookieHeader.size();
			token = trim(cookieHeader.substr(start, end - start));
			equal = token.find('=');
			if (equal != std::string::npos)
			{
				const std::string tokenName = trim(token.substr(0, equal));
				const std::string tokenValue = trim(token.substr(equal + 1));

				if (tokenName == name)
					return (tokenValue);
			}
			start = end + 1;
		}
		return ("");
	}

	std::string hexEncode(const unsigned char* bytes, std::size_t length)
	{
		const char* digits = "0123456789abcdef";
		std::string result;

		for (std::size_t i = 0; i < length; ++i)
		{
			result += digits[(bytes[i] >> 4) & 0x0f];
			result += digits[bytes[i] & 0x0f];
		}
		return (result);
	}

	bool fillRandomBytes(unsigned char* bytes, std::size_t length)
	{
		const int fd = open("/dev/urandom", O_RDONLY);
		std::size_t filled;

		if (fd < 0)
			return (false);
		filled = 0;
		while (filled < length)
		{
			const ssize_t result = read(fd, bytes + filled, length - filled);

			if (result > 0)
			{
				filled += static_cast<std::size_t>(result);
				continue;
			}
			if (result < 0 && errno == EINTR)
				continue;
			close(fd);
			return (false);
		}
		close(fd);
		return (true);
	}

	void fillFallbackBytes(
		unsigned char* bytes,
		std::size_t length,
		unsigned long counter)
	{
		unsigned long seed;

		seed = static_cast<unsigned long>(std::time(0))
			^ static_cast<unsigned long>(getpid())
			^ (counter * 1103515245UL);
		for (std::size_t i = 0; i < length; ++i)
		{
			seed = seed * 1103515245UL + 12345UL;
			bytes[i] = static_cast<unsigned char>((seed >> 16) & 0xff);
		}
	}
}

namespace webserv
{
	EventLoop::CgiJob::CgiJob()
		: pid(-1),
		  clientFd(-1),
		  stdinFd(-1),
		  stdoutFd(-1),
		  requestBody(),
		  stdinOffset(0),
		  output(),
		  startTime(std::time(0)),
		  stdinClosed(true),
		  stdoutClosed(true),
		  childReaped(true),
		  childStatus(0),
		  errorPages(),
		  errorRoot()
	{
	}

	EventLoop::CgiJob::CgiJob(
		int clientFdValue,
		const CgiExecution& execution,
		const std::map<int, std::string>& errorPagesValue,
		const std::string& errorRootValue)
		: pid(execution.pid),
		  clientFd(clientFdValue),
		  stdinFd(execution.stdinFd),
		  stdoutFd(execution.stdoutFd),
		  requestBody(execution.requestBody),
		  stdinOffset(0),
		  output(),
		  startTime(std::time(0)),
		  stdinClosed(false),
		  stdoutClosed(false),
		  childReaped(false),
		  childStatus(0),
		  errorPages(errorPagesValue),
		  errorRoot(errorRootValue)
	{
	}

	EventLoop::Session::Session()
		: visits(0),
		  createdAt(0),
		  lastAccess(0)
	{
	}

	EventLoop::Session::Session(std::time_t now)
		: visits(0),
		  createdAt(now),
		  lastAccess(now)
	{
	}

	ListenSocketConfig::ListenSocketConfig(
		int fdValue,
		const ServerConfig& serverValue)
		: fd(fdValue), server(serverValue)
	{
	}

	EventLoop::EventLoop(const std::vector<ListenSocketConfig>& listeners)
		: _pollFds(),
		  _clients(),
		  _serversByListenFd(),
		  _cgiJobs(),
		  _cgiClientByFd(),
		  _orphanedCgiPids(),
		  _sessions(),
		  _sessionCounter(0)
	{
		if (listeners.empty())
			throw std::runtime_error("event loop: no listen sockets");
		for (std::size_t i = 0; i < listeners.size(); ++i)
		{
			_serversByListenFd[listeners[i].fd] = listeners[i].server;
			addFd(listeners[i].fd, POLLIN);
		}
	}

	EventLoop::~EventLoop()
	{
		std::vector<int> cgiClientFds;

		closeAllClients();
		for (std::map<int, CgiJob>::const_iterator it = _cgiJobs.begin();
			it != _cgiJobs.end(); ++it)
			cgiClientFds.push_back(it->first);
		for (std::size_t i = 0; i < cgiClientFds.size(); ++i)
			cleanupCgiForClient(cgiClientFds[i], true);
		reapOrphanedCgiPids();
	}

	void EventLoop::run()
	{
		std::cout << "webserv poll event loop started" << std::endl;
		while (!shutdownRequested())
		{
			const int ready = poll(
				&_pollFds[0],
				_pollFds.size(),
				kPollTimeoutMs);

			if (ready < 0)
			{
				if (errno == EINTR)
					continue;
				throw std::runtime_error(systemError("poll"));
			}
			closeTimedOutClients();
			checkCgiJobs();
			if (ready == 0)
				continue;
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
						handleListenEvent(fd);
				}
				else if (isCgiFd(fd))
				{
					handleCgiEvent(fd, events);
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
		std::cout << "webserv event loop stopping" << std::endl;
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
		return (_serversByListenFd.find(fd) != _serversByListenFd.end());
	}

	bool EventLoop::isCgiFd(int fd) const
	{
		return (_cgiClientByFd.find(fd) != _cgiClientByFd.end());
	}

	void EventLoop::closeClient(int fd)
	{
		if (_clients.find(fd) == _clients.end())
			return;
		cleanupCgiForClient(fd, true);
		removeFd(fd);
		_clients.erase(fd);
		closeFd(fd);
	}

	void EventLoop::closeAllClients()
	{
		std::vector<int> clientFds;

		for (std::map<int, Client>::const_iterator it = _clients.begin();
			it != _clients.end(); ++it)
			clientFds.push_back(it->first);
		for (std::size_t i = 0; i < clientFds.size(); ++i)
			closeClient(clientFds[i]);
	}

	void EventLoop::closeTimedOutClients()
	{
		std::vector<int> expired;
		const std::time_t now = std::time(0);

		for (std::map<int, Client>::const_iterator it = _clients.begin();
			it != _clients.end(); ++it)
		{
			if (now - it->second.lastActivity() >= kClientTimeoutSeconds)
				expired.push_back(it->first);
		}
		for (std::size_t i = 0; i < expired.size(); ++i)
			closeClient(expired[i]);
	}

	void EventLoop::handleListenEvent(int listenFd)
	{
		while (true)
		{
			int clientFd = accept(listenFd, 0, 0);

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
				if (_clients.size() >= kMaxClients)
				{
					closeFd(clientFd);
					continue;
				}
				setNonBlocking(clientFd);
				_clients.insert(std::make_pair(clientFd,
						Client(clientFd, listenFd)));
				addFd(clientFd, POLLIN);
				std::cout << "accepted client fd " << clientFd
						  << " from listen fd " << listenFd << std::endl;
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
			if (client->inputBuffer().size() > kMaxRawInputBufferSize)
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

	void EventLoop::handleCgiEvent(int fd, short events)
	{
		std::map<int, int>::iterator fdIt;
		std::map<int, CgiJob>::iterator jobIt;
		int clientFd;

		fdIt = _cgiClientByFd.find(fd);
		if (fdIt == _cgiClientByFd.end())
			return;
		clientFd = fdIt->second;
		jobIt = _cgiJobs.find(clientFd);
		if (jobIt == _cgiJobs.end())
		{
			_cgiClientByFd.erase(fdIt);
			removeFd(fd);
			closeFd(fd);
			return;
		}
		if ((events & POLLNVAL) != 0)
		{
			failCgiJob(clientFd, HTTP_STATUS_BAD_GATEWAY, true);
			return;
		}
		if (fd == jobIt->second.stdinFd)
		{
			if ((events & POLLOUT) != 0)
				handleCgiWrite(jobIt->second);
			if (_cgiJobs.find(clientFd) != _cgiJobs.end()
				&& (events & (POLLERR | POLLHUP)) != 0)
				closeCgiFd(jobIt->second, jobIt->second.stdinFd);
		}
		else if (fd == jobIt->second.stdoutFd)
		{
			if ((events & (POLLIN | POLLHUP)) != 0)
				handleCgiRead(jobIt->second);
			if (_cgiJobs.find(clientFd) != _cgiJobs.end()
				&& (events & POLLERR) != 0)
				failCgiJob(clientFd, HTTP_STATUS_BAD_GATEWAY, true);
		}
	}

	void EventLoop::handleCgiWrite(CgiJob& job)
	{
		while (job.stdinFd >= 0
			&& job.stdinOffset < job.requestBody.size())
		{
			const ssize_t written = write(
				job.stdinFd,
				job.requestBody.data() + job.stdinOffset,
				job.requestBody.size() - job.stdinOffset);

			if (written > 0)
			{
				job.stdinOffset += static_cast<std::size_t>(written);
				continue;
			}
			if (written == 0)
				break;
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			closeCgiFd(job, job.stdinFd);
			job.stdinClosed = true;
			return;
		}
		if (job.stdinOffset >= job.requestBody.size())
		{
			closeCgiFd(job, job.stdinFd);
			job.stdinClosed = true;
		}
	}

	void EventLoop::handleCgiRead(CgiJob& job)
	{
		char buffer[kBufferSize];

		while (job.stdoutFd >= 0)
		{
			const ssize_t bytesRead = read(job.stdoutFd, buffer, sizeof(buffer));

			if (bytesRead > 0)
			{
				job.output.append(buffer, static_cast<std::size_t>(bytesRead));
				if (job.output.size() > kMaxCgiOutputSize)
				{
					failCgiJob(
						job.clientFd,
						HTTP_STATUS_BAD_GATEWAY,
						true);
					return;
				}
				continue;
			}
			if (bytesRead == 0)
			{
				closeCgiFd(job, job.stdoutFd);
				job.stdoutClosed = true;
				return;
			}
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			failCgiJob(job.clientFd, HTTP_STATUS_BAD_GATEWAY, true);
			return;
		}
	}

	void EventLoop::processClientInput(Client& client)
	{
		RequestParser::Result result;

		result = client.parser().parse(client.inputBuffer(), client.request());
		if (result == RequestParser::PARSE_INCOMPLETE)
		{
			if (parserStateReadsBody(client.parser().state()))
			{
				client.setState(CLIENT_READING_BODY);
				prepareEarlyBodyLimitResponse(client);
			}
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

	bool EventLoop::prepareEarlyBodyLimitResponse(Client& client)
	{
		const ServerConfig* server;
		RouteResult route;

		if (client.request().bodyFraming() == HttpRequest::BODY_NONE)
			return (false);
		server = serverForClient(client);
		if (server == 0)
			return (false);
		if (!isImplementedMethod(client.request().method()))
		{
			prepareErrorResponse(
				client,
				HTTP_STATUS_NOT_IMPLEMENTED,
				server->errorPages,
				server->root);
			return (true);
		}
		route = Router::route(client.request(), *server);
		if (!route.ok)
		{
			prepareErrorResponse(
				client,
				route.statusCode,
				server->errorPages,
				server->root);
			return (true);
		}
		if (!methodAllowed(client.request().method(), route.effective.methods))
		{
			prepareMethodNotAllowedResponse(
				client,
				route.effective.methods,
				route.effective.errorPages,
				server->root);
			return (true);
		}
		if (bodyTooLarge(client.request(), route.effective.clientMaxBodySize))
		{
			prepareErrorResponse(
				client,
				HTTP_STATUS_PAYLOAD_TOO_LARGE,
				route.effective.errorPages,
				server->root);
			return (true);
		}
		return (false);
	}

	void EventLoop::prepareSuccessResponse(Client& client)
	{
		const ServerConfig* server;
		RouteResult route;
		HttpResponse response;

		server = serverForClient(client);
		if (server == 0)
		{
			prepareErrorResponse(client, HTTP_STATUS_INTERNAL_SERVER_ERROR);
			return;
		}
		if (!isImplementedMethod(client.request().method()))
		{
			prepareErrorResponse(
				client,
				HTTP_STATUS_NOT_IMPLEMENTED,
				server->errorPages,
				server->root);
			return;
		}
		route = Router::route(client.request(), *server);
		if (!route.ok)
		{
			prepareErrorResponse(
				client,
				route.statusCode,
				server->errorPages,
				server->root);
		}
		else if (route.effective.redirectCode != 0)
		{
			client.setOutput(ResponseBuilder::redirect(
					route.effective.redirectCode,
					route.effective.redirectTarget).serialize());
		}
		else if (!methodAllowed(
				client.request().method(),
				route.effective.methods))
		{
			prepareMethodNotAllowedResponse(
				client,
				route.effective.methods,
				route.effective.errorPages,
				server->root);
		}
		else if (bodyTooLarge(
				client.request(),
				route.effective.clientMaxBodySize))
		{
			prepareErrorResponse(
				client,
				HTTP_STATUS_PAYLOAD_TOO_LARGE,
				route.effective.errorPages,
				server->root);
		}
		else if (client.request().method() == HTTP_METHOD_GET
			&& route.uriPath == "/session")
		{
			prepareSessionResponse(client);
		}
		else if (CgiHandler::isCgiRequest(route))
		{
			startCgiResponse(client, route, *server);
		}
		else if (client.request().method() == HTTP_METHOD_POST
			&& route.uriPath == "/echo")
		{
			client.setOutput(ResponseBuilder::text(
					HTTP_STATUS_OK,
					client.request().body(),
					"application/octet-stream").serialize());
		}
		else if (client.request().method() == HTTP_METHOD_POST)
		{
			if (!route.effective.uploadEnabled)
			{
				prepareErrorResponse(
					client,
					HTTP_STATUS_FORBIDDEN,
					route.effective.errorPages,
					server->root);
			}
			else
			{
				const UploadHandler handler(route.effective.uploadStore);

				response = handler.handlePost(client.request());
				if (isErrorStatusCode(response.statusCode()))
				{
					prepareErrorResponse(
						client,
						response.statusCode(),
						route.effective.errorPages,
						server->root);
				}
				else
					client.setOutput(response.serialize());
			}
		}
		else if (client.request().method() == HTTP_METHOD_DELETE)
		{
			const DeleteHandler handler;

			if (route.uriPath.size() > 1
				&& route.uriPath[route.uriPath.size() - 1] == '/')
			{
				prepareErrorResponse(
					client,
					HTTP_STATUS_FORBIDDEN,
					route.effective.errorPages,
					server->root);
			}
			else
			{
				response = handler.handleDelete(
					route.filesystemPath,
					route.relativePath);
				if (isErrorStatusCode(response.statusCode()))
				{
					prepareErrorResponse(
						client,
						response.statusCode(),
						route.effective.errorPages,
						server->root);
				}
				else
					client.setOutput(response.serialize());
			}
		}
		else if (client.request().method() != HTTP_METHOD_GET)
		{
			prepareErrorResponse(
				client,
				HTTP_STATUS_NOT_IMPLEMENTED,
				route.effective.errorPages,
				server->root);
		}
		else
		{
			const StaticFileHandler handler(route.effective.root);

			response = handler.handlePath(
				route.filesystemPath,
				route.effective.indexes,
				route.effective.autoindex,
				route.uriPath);
			if (isErrorStatusCode(response.statusCode()))
			{
				prepareErrorResponse(
					client,
					response.statusCode(),
					route.effective.errorPages,
					server->root);
			}
			else
				client.setOutput(response.serialize());
		}
	}

	void EventLoop::prepareSessionResponse(Client& client)
	{
		const std::time_t now = std::time(0);
		std::string sessionId;
		std::string requestedId;
		std::map<std::string, Session>::iterator it;
		bool created;
		HttpResponse response;
		std::string body;

		purgeExpiredSessions(now);
		requestedId = cookieValue(
			client.request().header("Cookie"),
			kSessionCookieName);
		created = true;
		if (isValidSessionId(requestedId))
		{
			it = _sessions.find(requestedId);
			if (it != _sessions.end())
			{
				sessionId = requestedId;
				created = false;
			}
		}
		if (created)
		{
			enforceSessionLimit();
			sessionId = createSessionId();
			_sessions[sessionId] = Session(now);
		}
		Session& session = _sessions[sessionId];
		++session.visits;
		session.lastAccess = now;
		body = "session_id: " + sessionId + "\n";
		body += "new: ";
		body += created ? "yes\n" : "no\n";
		body += "visits: " + numberToString(session.visits) + "\n";
		body += "created_at: "
			+ numberToString(static_cast<unsigned long>(session.createdAt))
			+ "\n";
		body += "last_access: "
			+ numberToString(static_cast<unsigned long>(session.lastAccess))
			+ "\n";
		body += "ttl_seconds: "
			+ numberToString(static_cast<unsigned long>(kSessionTtlSeconds))
			+ "\n";
		body += "active_sessions: "
			+ numberToString(static_cast<unsigned long>(_sessions.size()))
			+ "\n";
		response = ResponseBuilder::text(
			HTTP_STATUS_OK,
			body,
			"text/plain");
		response.setHeader(
			"Set-Cookie",
			std::string(kSessionCookieName) + "=" + sessionId
				+ "; Path=/session; Max-Age="
				+ numberToString(static_cast<unsigned long>(
					kSessionTtlSeconds))
				+ "; HttpOnly; SameSite=Lax");
		client.setOutput(response.serialize());
	}

	void EventLoop::purgeExpiredSessions(std::time_t now)
	{
		std::vector<std::string> expired;

		for (std::map<std::string, Session>::const_iterator it =
			_sessions.begin(); it != _sessions.end(); ++it)
		{
			if (now - it->second.lastAccess >= kSessionTtlSeconds)
				expired.push_back(it->first);
		}
		for (std::size_t i = 0; i < expired.size(); ++i)
			_sessions.erase(expired[i]);
	}

	void EventLoop::enforceSessionLimit()
	{
		while (_sessions.size() >= kMaxSessions && !_sessions.empty())
		{
			std::map<std::string, Session>::iterator oldest =
				_sessions.begin();

			for (std::map<std::string, Session>::iterator it =
				_sessions.begin(); it != _sessions.end(); ++it)
			{
				if (it->second.lastAccess < oldest->second.lastAccess)
					oldest = it;
			}
			_sessions.erase(oldest);
		}
	}

	std::string EventLoop::createSessionId()
	{
		unsigned char bytes[16];

		for (int attempt = 0; attempt < 16; ++attempt)
		{
			std::string id;

			++_sessionCounter;
			if (!fillRandomBytes(bytes, sizeof(bytes)))
				fillFallbackBytes(bytes, sizeof(bytes), _sessionCounter);
			id = hexEncode(bytes, sizeof(bytes));
			if (_sessions.find(id) == _sessions.end())
				return (id);
		}
		fillFallbackBytes(bytes, sizeof(bytes), ++_sessionCounter);
		return (hexEncode(bytes, sizeof(bytes)));
	}

	void EventLoop::startCgiResponse(
		Client& client,
		const RouteResult& route,
		const ServerConfig& server)
	{
		const CgiExecution execution = CgiHandler::start(
			client.request(),
			route);

		if (!execution.ok)
		{
			prepareErrorResponse(
				client,
				execution.statusCode,
				route.effective.errorPages,
				server.root);
			return;
		}
		_cgiJobs[client.fd()] = CgiJob(
			client.fd(),
			execution,
			route.effective.errorPages,
			server.root);
		CgiJob& job = _cgiJobs[client.fd()];
		client.setState(CLIENT_RUNNING_CGI);
		_cgiClientByFd[job.stdoutFd] = client.fd();
		addFd(job.stdoutFd, POLLIN);
		if (job.requestBody.empty())
		{
			closeCgiFd(job, job.stdinFd);
			job.stdinClosed = true;
		}
		else
		{
			_cgiClientByFd[job.stdinFd] = client.fd();
			addFd(job.stdinFd, POLLOUT);
		}
	}

	void EventLoop::checkCgiJobs()
	{
		std::vector<int> clients;
		const std::time_t now = std::time(0);

		reapOrphanedCgiPids();
		for (std::map<int, CgiJob>::const_iterator it = _cgiJobs.begin();
			it != _cgiJobs.end(); ++it)
			clients.push_back(it->first);
		for (std::size_t i = 0; i < clients.size(); ++i)
		{
			std::map<int, CgiJob>::iterator it = _cgiJobs.find(clients[i]);

			if (it == _cgiJobs.end())
				continue;
			CgiJob& job = it->second;
			if (!job.childReaped)
			{
				int status;
				const pid_t result = waitpid(job.pid, &status, WNOHANG);

				if (result == job.pid)
				{
					job.childReaped = true;
					job.childStatus = status;
				}
				else if (result < 0)
				{
					job.childReaped = true;
					job.childStatus = 1;
				}
			}
			if (_cgiJobs.find(clients[i]) == _cgiJobs.end())
				continue;
			if (!job.childReaped && now - job.startTime >= kCgiTimeoutSeconds)
			{
				failCgiJob(clients[i], HTTP_STATUS_GATEWAY_TIMEOUT, true);
				continue;
			}
			if (job.stdoutClosed && job.childReaped)
				completeCgiJob(clients[i]);
		}
	}

	void EventLoop::reapOrphanedCgiPids()
	{
		std::vector<pid_t> stillRunning;

		for (std::size_t i = 0; i < _orphanedCgiPids.size(); ++i)
		{
			int status;
			const pid_t result = waitpid(_orphanedCgiPids[i], &status, WNOHANG);

			if (result == 0)
				stillRunning.push_back(_orphanedCgiPids[i]);
		}
		_orphanedCgiPids = stillRunning;
	}

	void EventLoop::closeCgiFd(CgiJob& job, int& fd)
	{
		if (fd < 0)
			return;
		_cgiClientByFd.erase(fd);
		removeFd(fd);
		closeFd(fd);
		fd = -1;
		(void)job;
	}

	void EventLoop::cleanupCgiForClient(int clientFd, bool terminateChild)
	{
		std::map<int, CgiJob>::iterator it = _cgiJobs.find(clientFd);

		if (it == _cgiJobs.end())
			return;
		CgiJob& job = it->second;
		closeCgiFd(job, job.stdinFd);
		closeCgiFd(job, job.stdoutFd);
		if (terminateChild && !job.childReaped && job.pid > 0)
			kill(job.pid, SIGKILL);
		if (!job.childReaped && job.pid > 0)
		{
			int status;
			const pid_t result = waitpid(job.pid, &status, WNOHANG);

			if (result == 0)
				_orphanedCgiPids.push_back(job.pid);
		}
		_cgiJobs.erase(it);
	}

	void EventLoop::failCgiJob(
		int clientFd,
		int statusCode,
		bool terminateChild)
	{
		std::map<int, Client>::iterator clientIt = _clients.find(clientFd);
		std::map<int, CgiJob>::iterator jobIt = _cgiJobs.find(clientFd);
		std::map<int, std::string> errorPages;
		std::string errorRoot;

		if (jobIt != _cgiJobs.end())
		{
			errorPages = jobIt->second.errorPages;
			errorRoot = jobIt->second.errorRoot;
		}
		cleanupCgiForClient(clientFd, terminateChild);
		if (clientIt == _clients.end())
			return;
		prepareErrorResponse(
			clientIt->second,
			statusCode,
			errorPages,
			errorRoot);
		updateEvents(clientFd, clientIt->second.desiredPollEvents());
	}

	void EventLoop::completeCgiJob(int clientFd)
	{
		std::map<int, Client>::iterator clientIt = _clients.find(clientFd);
		std::map<int, CgiJob>::iterator jobIt = _cgiJobs.find(clientFd);
		std::string output;
		int childStatus;
		std::map<int, std::string> errorPages;
		std::string errorRoot;
		HttpResponse response;
		bool responseOk;

		if (jobIt == _cgiJobs.end())
			return;
		output = jobIt->second.output;
		childStatus = jobIt->second.childStatus;
		errorPages = jobIt->second.errorPages;
		errorRoot = jobIt->second.errorRoot;
		cleanupCgiForClient(clientFd, false);
		if (clientIt == _clients.end())
			return;
		if (!WIFEXITED(childStatus) || WEXITSTATUS(childStatus) != 0)
		{
			prepareErrorResponse(
				clientIt->second,
				HTTP_STATUS_BAD_GATEWAY,
				errorPages,
				errorRoot);
		}
		else
		{
			responseOk = CgiHandler::buildResponse(output, response);
			if (!responseOk)
			{
				prepareErrorResponse(
					clientIt->second,
					HTTP_STATUS_BAD_GATEWAY,
					errorPages,
					errorRoot);
			}
			else
				clientIt->second.setOutput(response.serialize());
		}
		updateEvents(clientFd, clientIt->second.desiredPollEvents());
	}

	void EventLoop::prepareErrorResponse(Client& client, int statusCode)
	{
		const ServerConfig* server;

		if (statusCode == 0)
			statusCode = HTTP_STATUS_BAD_REQUEST;
		server = serverForClient(client);
		if (server == 0)
			client.setOutput(ResponseBuilder::error(statusCode).serialize());
		else
			prepareErrorResponse(
				client,
				statusCode,
				server->errorPages,
				server->root);
	}

	void EventLoop::prepareErrorResponse(
		Client& client,
		int statusCode,
		const std::map<int, std::string>& errorPages,
		const std::string& root)
	{
		if (statusCode == 0)
			statusCode = HTTP_STATUS_BAD_REQUEST;
		client.setOutput(configuredErrorResponse(
				statusCode,
				errorPages,
				root).serialize());
	}

	void EventLoop::prepareMethodNotAllowedResponse(
		Client& client,
		const std::vector<HttpMethod>& allowedMethods,
		const std::map<int, std::string>& errorPages,
		const std::string& root)
	{
		HttpResponse response;

		response = configuredErrorResponse(
			HTTP_STATUS_METHOD_NOT_ALLOWED,
			errorPages,
			root);
		response.setHeader("Allow", methodNames(allowedMethods));
		client.setOutput(response.serialize());
	}

	const ServerConfig* EventLoop::serverForClient(const Client& client) const
	{
		std::map<int, ServerConfig>::const_iterator it;

		it = _serversByListenFd.find(client.listenFd());
		if (it == _serversByListenFd.end())
			return (0);
		return (&it->second);
	}
}
