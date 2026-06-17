#ifndef WEBSERV_STATE_MACHINE_HPP
# define WEBSERV_STATE_MACHINE_HPP

namespace webserv
{
	enum ClientState
	{
		CLIENT_READING_HEADERS,
		CLIENT_READING_BODY,
		CLIENT_PROCESSING_REQUEST,
		CLIENT_RUNNING_CGI,
		CLIENT_WRITING_RESPONSE,
		CLIENT_CLOSING
	};

	enum RequestParserState
	{
		PARSER_REQUEST_LINE,
		PARSER_HEADERS,
		PARSER_BODY_BY_LENGTH,
		PARSER_BODY_CHUNK_SIZE,
		PARSER_BODY_CHUNK_DATA,
		PARSER_BODY_CHUNK_CRLF,
		PARSER_COMPLETE,
		PARSER_ERROR
	};

	const char*	clientStateName(ClientState state);
	bool		clientStateCanRead(ClientState state);
	bool		clientStateCanWrite(ClientState state);
	const char*	parserStateName(RequestParserState state);
	bool		parserStateIsTerminal(RequestParserState state);
}

#endif
