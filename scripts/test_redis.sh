#!/bin/bash

HOST="127.0.0.1"
PORT="6379"

GREEN="\033[0;32m"
RED="\033[0;31m"
NC="\033[0m"

PASS="✅"
FAIL="❌"

pass_count=0
fail_count=0

send_redis_cmd() {
    echo -en "$1" | nc -N $HOST $PORT
}

escape_newlines() {
    echo "$1" | awk '{
        gsub("\r", "\\\\r");
        gsub("\n", "\\\\n");
        printf "%s", $0;
    }'
}

assert_response() {
    local name="$1"
    local cmd="$2"
    local expected="$3"

    print_test_header "$name"
    local response
    response=$(send_redis_cmd "$cmd")

    stripped_response=$(echo -en "$response" | tr -d '\r')
    stripped_expected=$(echo -en "$expected" | tr -d '\r')

    if [[ "$stripped_response" == "$stripped_expected" ]]; then
        echo -e "${GREEN}${PASS} Test passed${NC}"
        ((pass_count++))
    else
        echo -e "${RED}${FAIL} Test failed${NC}"
        echo -e "Expected: '$(escape_newlines "$expected")'"
        echo -e "Got:      '$(escape_newlines "$response")'"
        ((fail_count++))
    fi
}

print_test_header() {
    echo
    echo "======================================"
    echo "Test: $1"
    echo "======================================"
}

resp_ping="*1\r\n\$4\r\nPING\r\n"
resp_echo_hello="*2\r\n\$4\r\nECHO\r\n\$5\r\nhello\r\n"
resp_set_foo_bar="*3\r\n\$3\r\nSET\r\n\$3\r\nfoo\r\n\$3\r\nbar\r\n"
resp_set_bar_baz="*3\r\n\$3\r\nSET\r\n\$3\r\nbar\r\n\$3\r\nbaz\r\n"
resp_get_foo="*2\r\n\$3\r\nGET\r\n\$3\r\nfoo\r\n"
resp_exists_foo="*2\r\n\$6\r\nEXISTS\r\n\$3\r\nfoo\r\n"
resp_exists_foo_bar="*3\r\n\$6\r\nEXISTS\r\n\$3\r\nfoo\r\n\$3\r\nbar\r\n"
resp_exists_foo_exp="*3\r\n\$6\r\nEXISTS\r\n\$3\r\nfoo\r\n\$3\r\nexp\r\n"
resp_set_expiring="*5\r\n\$3\r\nSET\r\n\$3\r\nexp\r\n\$5\r\nvalue\r\n\$2\r\nPX\r\n\$3\r\n100\r\n"
resp_get_exp="*2\r\n\$3\r\nGET\r\n\$3\r\nexp\r\n"
resp_del="*3\r\n\$3\r\nDEL\r\n\$3\r\nfoo\r\n\$3\r\nbar\r\n"

# Ping
assert_response "PING" "$resp_ping" "+PONG"

# Echo
assert_response "ECHO hello" "$resp_echo_hello" "+hello"

# Set and Get
assert_response "SET foo bar" "$resp_set_foo_bar" "+OK"
assert_response "GET foo" "$resp_get_foo" "\$3\r\nbar"

# Set with expiration
assert_response "SET exp value PX 100" "$resp_set_expiring" "+OK"
assert_response "GET exp (immediate)" "$resp_get_exp" "\$5\r\nvalue"

# Get with expiration
sleep 0.2
assert_response "GET exp (after 200ms, should expire)" "$resp_get_exp" "\$-1"

# Delete
assert_response "SET foo bar" "$resp_set_foo_bar" "+OK"
assert_response "DEL foo" "$resp_del" "\$1\r\n"
assert_response "SET foo bar" "$resp_set_foo_bar" "+OK"
assert_response "SET bar baz" "$resp_set_bar_baz" "+OK"
assert_response "DEL foo bar" "$resp_del" "\$2\r\n"

# Exists
assert_response "SET foo bar" "$resp_set_foo_bar" "+OK"
assert_response "EXISTS foo" "$resp_exists_foo" "\$1\r\n"
assert_response "SET bar baz" "$resp_set_bar_baz" "+OK"
assert_response "EXISTS foo bar" "$resp_exists_foo_bar" "\$2\r\n"

# Exists with expiration
assert_response "SET foo bar" "$resp_set_foo_bar" "+OK"
assert_response "SET exp value PX 100" "$resp_set_expiring" "+OK"
sleep 0.2
assert_response "EXISTS foo exp" "$resp_exists_foo" "\$1\r\n"

echo
echo "======================================"
echo -e "Tests completed: ${GREEN}${pass_count} passed${NC}, ${RED}${fail_count} failed${NC}"
echo "======================================"
