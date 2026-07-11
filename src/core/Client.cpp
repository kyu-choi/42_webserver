#include "webserv/Client.hpp"
#include <poll.h>

namespace webserv
{
	Client::Client(int fd, int listenFd, const std::string& remoteAddr)
		: _fd(fd),
		  _listenFd(listenFd),
		  _remoteAddr(remoteAddr),
		  _inputBuffer(),
		  _outputBuffer(),
		  _sendOffset(0),
		  _request(),
		  _parser(),
		  _state(CLIENT_READING_HEADERS),
		  _lastActivity(std::time(0)),
		  _closing(false),
		  _continueSent(false)
	{
	}

	int Client::fd() const
	{
		return (_fd);
	}

	int Client::listenFd() const
	{
		return (_listenFd);
	}

	const std::string& Client::remoteAddr() const
	{
		return (_remoteAddr);
	}

	ClientState Client::state() const
	{
		return (_state);
	}

	std::time_t Client::lastActivity() const
	{
		return (_lastActivity);
	}

	const std::string& Client::inputBuffer() const
	{
		return (_inputBuffer);
	}

	const std::string& Client::outputBuffer() const
	{
		return (_outputBuffer);
	}

	std::size_t Client::sendOffset() const
	{
		return (_sendOffset);
	}

	HttpRequest& Client::request()
	{
		return (_request);
	}

	const HttpRequest& Client::request() const
	{
		return (_request);
	}

	RequestParser& Client::parser()
	{
		return (_parser);
	}

	const RequestParser& Client::parser() const
	{
		return (_parser);
	}

	void Client::appendInput(const char* data, std::size_t size)
	{
		_inputBuffer.append(data, size);
		touch();
	}

	void Client::consumeInput(std::size_t size)
	{
		if (size >= _inputBuffer.size())
			_inputBuffer.clear();
		else
			_inputBuffer.erase(0, size);
		touch();
	}

	bool Client::hasPendingOutput() const
	{
		return (_sendOffset < _outputBuffer.size());
	}

	std::size_t Client::pendingOutputSize() const
	{
		return (_outputBuffer.size() - _sendOffset);
	}

	const char* Client::pendingOutputData() const
	{
		return (_outputBuffer.c_str() + _sendOffset);
	}

	void Client::setOutput(const std::string& response)
	{
		_outputBuffer = response;
		_sendOffset = 0;
		_state = CLIENT_WRITING_RESPONSE;
		touch();
	}

	void Client::setInterimOutput(const std::string& response)
	{
		_outputBuffer = response;
		_sendOffset = 0;
		touch();
	}

	void Client::clearOutput()
	{
		_outputBuffer.clear();
		_sendOffset = 0;
		touch();
	}

	void Client::advanceSendOffset(std::size_t sentBytes)
	{
		_sendOffset += sentBytes;
		if (_sendOffset > _outputBuffer.size())
			_sendOffset = _outputBuffer.size();
		touch();
	}

	bool Client::outputComplete() const
	{
		return (_sendOffset >= _outputBuffer.size());
	}

	void Client::setState(ClientState state)
	{
		_state = state;
		touch();
	}

	void Client::markClosing()
	{
		_closing = true;
		_state = CLIENT_CLOSING;
		touch();
	}

	void Client::touch()
	{
		_lastActivity = std::time(0);
	}

	bool Client::continueSent() const
	{
		return (_continueSent);
	}

	void Client::setContinueSent(bool sent)
	{
		_continueSent = sent;
	}

	short Client::desiredPollEvents() const
	{
		if (_closing || _state == CLIENT_CLOSING)
			return (0);
		if (hasPendingOutput())
			return (POLLOUT);
		if (clientStateCanRead(_state))
			return (POLLIN);
		return (0);
	}
}
