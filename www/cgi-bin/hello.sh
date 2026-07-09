printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'cgi-type: shell\n'
printf 'method: %s\n' "$REQUEST_METHOD"
printf 'query: %s\n' "$QUERY_STRING"
if [ "$REQUEST_METHOD" = "POST" ]; then
	body=$(cat)
	printf 'body: %s\n' "$body"
fi
