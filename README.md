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

First, run the server

```sh
./ttsrv <port> <json>
```

The, run the client:

```sh
./ttcli <host> <port>
```

## License

This project is licensed under the MIT License. See the LICENSE file for details.
