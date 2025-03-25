#!/bin/sh

CACHE="${XDG_CACHE_HOME:-${HOME}/.cache}/tt"

if [ ! -f "${CACHE}/monkeytype/quotes_english.json" ]; then
	mkdir -p ${CACHE}/monkeytype
	curl https://monkeytype.com/quotes/english.json \
		-o ${CACHE}/monkeytype/quotes_english.json
	echo "Downloaded ${CACHE}/monkeytype/quotes_english.json"
else
	echo "Existing ${CACHE}/monkeytype/quotes_english.json"
fi
