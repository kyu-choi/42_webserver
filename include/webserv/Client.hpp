#ifndef WEBSERV_CLIENT_HPP
# define WEBSERV_CLIENT_HPP

# include "webserv/HttpRequest.hpp"
# include "webserv/RequestParser.hpp"
# include "webserv/StateMachine.hpp"
# include <ctime>
# include <string>

namespace webserv
{
	class Client
	{
	public:
		Client(int fd, int listenFd);

		int					fd() const;
		int					listenFd() const;
		ClientState			state() const;
		std::time_t			lastActivity() const;
		const std::string&	inputBuffer() const;
		const std::string&	outputBuffer() const;
		std::size_t			sendOffset() const;
		HttpRequest&		request();
		const HttpRequest&	request() const;
		RequestParser&		parser();
		const RequestParser&	parser() const;

		void	appendInput(const char* data, std::size_t size);
		void	consumeInput(std::size_t size);
		bool	hasPendingOutput() const;
		std::size_t	pendingOutputSize() const;
		const char*	pendingOutputData() const;
		void	setOutput(const std::string& response);
		void	setInterimOutput(const std::string& response);
		void	clearOutput();
		void	advanceSendOffset(std::size_t sentBytes);
		bool	outputComplete() const;
		void	setState(ClientState state);
		void	markClosing();
		void	touch();
		bool	continueSent() const;
		void	setContinueSent(bool sent);
		short	desiredPollEvents() const;

	private:
		int				_fd;
		int				_listenFd;
		std::string		_inputBuffer;
		std::string		_outputBuffer;
		std::size_t		_sendOffset;
		HttpRequest		_request;
		RequestParser	_parser;
		ClientState		_state;
		std::time_t		_lastActivity;
		bool			_closing;
		bool			_continueSent;
	};
}

#endif
