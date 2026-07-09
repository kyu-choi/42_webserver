#ifndef WEBSERV_REQUEST_PARSER_HPP
# define WEBSERV_REQUEST_PARSER_HPP

# include "webserv/HttpRequest.hpp"
# include "webserv/StateMachine.hpp"
# include <cstddef>
# include <string>

namespace webserv
{
	class RequestParser
	{
	public:
		enum Result
		{
			PARSE_INCOMPLETE,
			PARSE_COMPLETE,
			PARSE_ERROR
		};

		RequestParser();

		Result				parse(const std::string& input, HttpRequest& request);
		RequestParserState	state() const;
		std::size_t			consumedBytes() const;
		int					errorStatus() const;
		void				reset();

	private:
		RequestParserState	_state;
		std::size_t			_consumedBytes;
		int					_errorStatus;

		Result	parseRequestLine(
					const std::string& line,
					HttpRequest& request);
		Result	parseHeaders(
					const std::string& headerBlock,
					HttpRequest& request,
					std::size_t& expectedBodySize,
					bool& hasChunkedBody);
		Result	parseChunkedBody(
					const std::string& input,
					std::size_t bodyStart,
					HttpRequest& request);
		void	setError(int status);
	};
}

#endif
