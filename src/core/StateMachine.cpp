#include "webserv/StateMachine.hpp"

namespace webserv
{
	const char* clientStateName(ClientState state)
	{
		switch (state)
		{
			case CLIENT_READING_HEADERS:
				return ("READING_HEADERS");
			case CLIENT_READING_BODY:
				return ("READING_BODY");
			case CLIENT_PROCESSING_REQUEST:
				return ("PROCESSING_REQUEST");
			case CLIENT_RUNNING_CGI:
				return ("RUNNING_CGI");
			case CLIENT_WRITING_RESPONSE:
				return ("WRITING_RESPONSE");
			case CLIENT_CLOSING:
				return ("CLOSING");
		}
		return ("CLOSING");
	}

	bool clientStateCanRead(ClientState state)
	{
		return (state == CLIENT_READING_HEADERS
			|| state == CLIENT_READING_BODY);
	}

	bool clientStateCanWrite(ClientState state)
	{
		return (state == CLIENT_WRITING_RESPONSE);
	}

	const char* parserStateName(RequestParserState state)
	{
		switch (state)
		{
			case PARSER_REQUEST_LINE:
				return ("REQUEST_LINE");
			case PARSER_HEADERS:
				return ("HEADERS");
			case PARSER_BODY_BY_LENGTH:
				return ("BODY_BY_LENGTH");
			case PARSER_BODY_CHUNK_SIZE:
				return ("BODY_CHUNK_SIZE");
			case PARSER_BODY_CHUNK_DATA:
				return ("BODY_CHUNK_DATA");
			case PARSER_BODY_CHUNK_CRLF:
				return ("BODY_CHUNK_CRLF");
			case PARSER_COMPLETE:
				return ("COMPLETE");
			case PARSER_ERROR:
				return ("ERROR");
		}
		return ("ERROR");
	}

	bool parserStateIsTerminal(RequestParserState state)
	{
		return (state == PARSER_COMPLETE || state == PARSER_ERROR);
	}
}
