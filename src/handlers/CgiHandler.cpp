#include "webserv/CgiHandler.hpp"
#include "webserv/Http.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
	void closeFd(int fd)
	{
		if (fd >= 0)
			close(fd);
	}

	bool setNonBlocking(int fd)
	{
		return (fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
	}

	bool setCloseOnExec(int fd)
	{
		return (fcntl(fd, F_SETFD, FD_CLOEXEC) == 0);
	}

	std::string lowerString(const std::string& value)
	{
		std::string result;

		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] >= 'A' && value[i] <= 'Z')
				result += static_cast<char>(value[i] - 'A' + 'a');
			else
				result += value[i];
		}
		return (result);
	}

	std::string trim(const std::string& value)
	{
		std::size_t begin;
		std::size_t end;

		begin = 0;
		while (begin < value.size()
			&& (value[begin] == ' ' || value[begin] == '\t'
				|| value[begin] == '\r' || value[begin] == '\n'))
			++begin;
		end = value.size();
		while (end > begin
			&& (value[end - 1] == ' ' || value[end - 1] == '\t'
				|| value[end - 1] == '\r' || value[end - 1] == '\n'))
			--end;
		return (value.substr(begin, end - begin));
	}

	std::string numberToString(std::size_t value)
	{
		std::ostringstream stream;

		stream << value;
		return (stream.str());
	}

	std::string extensionForPath(const std::string& path)
	{
		const std::size_t slash = path.find_last_of('/');
		const std::size_t dot = path.find_last_of('.');

		if (dot == std::string::npos)
			return ("");
		if (slash != std::string::npos && dot < slash)
			return ("");
		return (path.substr(dot));
	}

	std::string directoryName(const std::string& path)
	{
		const std::size_t slash = path.find_last_of('/');

		if (slash == std::string::npos)
			return (".");
		if (slash == 0)
			return ("/");
		return (path.substr(0, slash));
	}

	std::string baseName(const std::string& path)
	{
		const std::size_t slash = path.find_last_of('/');

		if (slash == std::string::npos)
			return (path);
		return (path.substr(slash + 1));
	}

	std::string headerEnvName(const std::string& headerName)
	{
		std::string result;

		result = "HTTP_";
		for (std::size_t i = 0; i < headerName.size(); ++i)
		{
			const char c = headerName[i];

			if (c >= 'a' && c <= 'z')
				result += static_cast<char>(c - 'a' + 'A');
			else if (c >= 'A' && c <= 'Z')
				result += c;
			else if (c >= '0' && c <= '9')
				result += c;
			else
				result += '_';
		}
		return (result);
	}

	void addEnv(std::vector<std::string>& env, const std::string& entry)
	{
		env.push_back(entry);
	}

	std::string hostWithoutPort(const std::string& hostHeader)
	{
		const std::string trimmed = trim(hostHeader);
		const std::size_t colon = trimmed.find(':');

		if (colon == std::string::npos)
			return (trimmed);
		return (trimmed.substr(0, colon));
	}

	std::vector<std::string> buildEnvironment(
		const webserv::HttpRequest& request,
		const webserv::RouteResult& route,
		const webserv::CgiNetworkInfo& network)
	{
		std::vector<std::string> env;
		const std::map<std::string, std::string>& headers = request.headers();
		std::string serverName = hostWithoutPort(request.header("Host"));

		if (serverName.empty())
			serverName = network.serverAddr;
		if (serverName.empty())
			serverName = "localhost";
		addEnv(env, "GATEWAY_INTERFACE=CGI/1.1");
		addEnv(env, "SERVER_SOFTWARE=webserv");
		addEnv(env, "SERVER_NAME=" + serverName);
		addEnv(env, "SERVER_PORT=" + network.serverPort);
		addEnv(env, "REMOTE_ADDR=" + network.remoteAddr);
		addEnv(env, "REQUEST_METHOD=" + std::string(
				webserv::httpMethodName(request.method())));
		addEnv(env, "SCRIPT_NAME=" + route.uriPath);
		addEnv(env, "SCRIPT_FILENAME=" + route.filesystemPath);
		addEnv(env, "QUERY_STRING=" + route.queryString);
		addEnv(env, "CONTENT_LENGTH=" + numberToString(request.body().size()));
		addEnv(env, "CONTENT_TYPE=" + request.header("Content-Type"));
		addEnv(env, "SERVER_PROTOCOL=" + request.version());
		addEnv(env, "PATH_INFO=" + route.uriPath);
		addEnv(env, "PATH_TRANSLATED=" + route.filesystemPath);
		addEnv(env, "DOCUMENT_ROOT=" + route.effective.root);
		addEnv(env, "REDIRECT_STATUS=200");
		for (std::map<std::string, std::string>::const_iterator it =
			headers.begin(); it != headers.end(); ++it)
		{
			if (it->first == "content-type" || it->first == "content-length")
				continue;
			addEnv(env, headerEnvName(it->first) + "=" + it->second);
		}
		return (env);
	}

	std::vector<char*> cStringArray(std::vector<std::string>& values)
	{
		std::vector<char*> result;

		for (std::size_t i = 0; i < values.size(); ++i)
			result.push_back(const_cast<char*>(values[i].c_str()));
		result.push_back(0);
		return (result);
	}

	void childExec(
		const std::string& interpreter,
		const std::string& scriptArgument,
		const std::string& scriptDirectory,
		std::vector<std::string>& envValues)
	{
		std::vector<std::string> argvValues;
		std::vector<char*> argv;
		std::vector<char*> envp;

		argvValues.push_back(interpreter);
		argvValues.push_back(scriptArgument);
		argv = cStringArray(argvValues);
		envp = cStringArray(envValues);
		if (chdir(scriptDirectory.c_str()) < 0)
			_exit(127);
		execve(interpreter.c_str(), &argv[0], &envp[0]);
		_exit(127);
	}

	void closePipePair(int pipeFds[2])
	{
		closeFd(pipeFds[0]);
		closeFd(pipeFds[1]);
		pipeFds[0] = -1;
		pipeFds[1] = -1;
	}

	bool splitCgiOutput(
		const std::string& output,
		std::string& headerBlock,
		std::string& body)
	{
		std::size_t separator;
		std::size_t separatorSize;

		separator = output.find("\r\n\r\n");
		separatorSize = 4;
		if (separator == std::string::npos)
		{
			separator = output.find("\n\n");
			separatorSize = 2;
		}
		if (separator == std::string::npos)
			return (false);
		headerBlock = output.substr(0, separator);
		body = output.substr(separator + separatorSize);
		return (true);
	}

	bool parseStatusHeader(
		const std::string& value,
		int& statusCode,
		std::string& statusReason)
	{
		std::istringstream stream(value);
		int parsed;
		std::string rest;

		if (!(stream >> parsed))
			return (false);
		if (std::string(webserv::reasonPhrase(parsed)) == "Unknown")
			return (false);
		std::getline(stream, rest);
		statusCode = parsed;
		statusReason = trim(rest);
		return (true);
	}

	bool parseCgiHeaders(
		const std::string& headerBlock,
		webserv::HttpResponse& response)
	{
		std::size_t start;
		bool hasContentType;
		bool hasLocation;

		start = 0;
		hasContentType = false;
		hasLocation = false;
		while (start <= headerBlock.size())
		{
			std::size_t end;
			std::string line;
			std::size_t colon;
			std::string name;
			std::string value;
			std::string lowered;

			end = headerBlock.find('\n', start);
			if (end == std::string::npos)
				end = headerBlock.size();
			line = headerBlock.substr(start, end - start);
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);
			if (!line.empty())
			{
				colon = line.find(':');
				if (colon == std::string::npos)
					return (false);
				name = trim(line.substr(0, colon));
				value = trim(line.substr(colon + 1));
				lowered = lowerString(name);
				if (name.empty())
					return (false);
				if (lowered == "status")
				{
					int statusCode;
					std::string statusReason;

					if (!parseStatusHeader(value, statusCode, statusReason))
						return (false);
					response.setStatusCode(statusCode);
					if (!statusReason.empty())
						response.setReasonPhrase(statusReason);
				}
				else if (lowered != "content-length"
					&& lowered != "connection"
					&& lowered != "transfer-encoding")
				{
					response.setHeader(name, value);
					if (lowered == "content-type")
						hasContentType = true;
					if (lowered == "location")
						hasLocation = true;
				}
			}
			if (end == headerBlock.size())
				break;
			start = end + 1;
		}
		return (hasContentType || hasLocation);
	}
}

namespace webserv
{
	CgiExecution::CgiExecution()
		: ok(false),
		  statusCode(HTTP_STATUS_INTERNAL_SERVER_ERROR),
		  pid(-1),
		  stdinFd(-1),
		  stdoutFd(-1),
		  requestBody()
	{
	}

	bool CgiHandler::isCgiRequest(const RouteResult& route)
	{
		const std::string extension = extensionForPath(route.filesystemPath);

		if (extension.empty())
			return (false);
		return (route.effective.cgiByExtension.find(extension)
			!= route.effective.cgiByExtension.end());
	}

	CgiExecution CgiHandler::start(
		const HttpRequest& request,
		const RouteResult& route,
		const CgiNetworkInfo& network)
	{
		CgiExecution execution;
		const std::string extension = extensionForPath(route.filesystemPath);
		std::map<std::string, std::string>::const_iterator interpreterIt;
		struct stat fileStat;
		int stdinPipe[2];
		int stdoutPipe[2];
		std::vector<std::string> envValues;

		stdinPipe[0] = -1;
		stdinPipe[1] = -1;
		stdoutPipe[0] = -1;
		stdoutPipe[1] = -1;
		interpreterIt = route.effective.cgiByExtension.find(extension);
		if (interpreterIt == route.effective.cgiByExtension.end())
		{
			execution.statusCode = HTTP_STATUS_NOT_FOUND;
			return (execution);
		}
		if (stat(route.filesystemPath.c_str(), &fileStat) < 0)
		{
			execution.statusCode = HTTP_STATUS_NOT_FOUND;
			return (execution);
		}
		if (!S_ISREG(fileStat.st_mode))
		{
			execution.statusCode = HTTP_STATUS_FORBIDDEN;
			return (execution);
		}
		if (pipe(stdinPipe) < 0)
			return (execution);
		if (pipe(stdoutPipe) < 0)
		{
			closePipePair(stdinPipe);
			return (execution);
		}
		if (!setCloseOnExec(stdinPipe[0]) || !setCloseOnExec(stdinPipe[1])
			|| !setCloseOnExec(stdoutPipe[0]) || !setCloseOnExec(stdoutPipe[1]))
		{
			closePipePair(stdinPipe);
			closePipePair(stdoutPipe);
			return (execution);
		}
		envValues = buildEnvironment(request, route, network);
		execution.pid = fork();
		if (execution.pid < 0)
		{
			closePipePair(stdinPipe);
			closePipePair(stdoutPipe);
			execution.statusCode = HTTP_STATUS_INTERNAL_SERVER_ERROR;
			return (execution);
		}
		if (execution.pid == 0)
		{
			if (dup2(stdinPipe[0], STDIN_FILENO) < 0
				|| dup2(stdoutPipe[1], STDOUT_FILENO) < 0)
				_exit(127);
			closePipePair(stdinPipe);
			closePipePair(stdoutPipe);
			childExec(
				interpreterIt->second,
				baseName(route.filesystemPath),
				directoryName(route.filesystemPath),
				envValues);
		}
		closeFd(stdinPipe[0]);
		closeFd(stdoutPipe[1]);
		stdinPipe[0] = -1;
		stdoutPipe[1] = -1;
		if (!setNonBlocking(stdinPipe[1]) || !setNonBlocking(stdoutPipe[0]))
		{
			closePipePair(stdinPipe);
			closePipePair(stdoutPipe);
			kill(execution.pid, SIGKILL);
			waitpid(execution.pid, 0, WNOHANG);
			execution.statusCode = HTTP_STATUS_INTERNAL_SERVER_ERROR;
			return (execution);
		}
		execution.ok = true;
		execution.statusCode = HTTP_STATUS_OK;
		execution.stdinFd = stdinPipe[1];
		execution.stdoutFd = stdoutPipe[0];
		execution.requestBody = request.body();
		return (execution);
	}

	bool CgiHandler::buildResponse(
		const std::string& output,
		HttpResponse& response)
	{
		std::string headerBlock;
		std::string body;

		if (!splitCgiOutput(output, headerBlock, body))
			return (false);
		response = HttpResponse();
		response.setStatusCode(HTTP_STATUS_OK);
		if (!parseCgiHeaders(headerBlock, response))
			return (false);
		response.setBody(body);
		response.setConnectionClose(true);
		return (true);
	}
}
