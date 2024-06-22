#!/bin/sh

mkdir -p data
curl https://monkeytype.com/quotes/english.json -o data/quotes_english.json
