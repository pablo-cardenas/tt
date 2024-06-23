#!/bin/sh

if [ ! -f data/monkeytype/quotes_english.json ]; then
	mkdir -p data/monkeytype
	curl https://monkeytype.com/quotes/english.json \
		-o data/monkeytype/quotes_english.json
	echo "Downloaded data/monkeytype/quotes_english.json"
else
	echo "Existing data/monkeytype/quotes_english.json"
fi
