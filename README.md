# tt

`tt` is a terminal-based typing game.

## Features

- **Read ahead**: Hides the current texts and highlight the following.
- **Network Play**: Connect with friends over a network using sockets.

## Installation

```sh
git clone https://github.com/pablo-cardenas/tt.git
cd tt
make
sudo make install
```

## Usage

First, download the quotes file from monkeytype.

```sh
curl https://monkeytype.com/quotes/english.json -o quotes_english.json
```

Then, choose a port number (from 1024 to 65535) and run the server:

```sh
ttsrv <port> quotes_english.json
```

In another terminal, run the client with the server's host and port. If you are
running the server and client on the same machine, you can use `127.0.0.1` as
the host.

```sh
./ttcli <host> <port>
```

In the client, press `Enter` to start the game.

## License

This project is licensed under the MIT License. See the LICENSE file for
details.
