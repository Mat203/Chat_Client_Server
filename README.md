## Classes

### ChatServer

The `ChatServer` class is responsible for managing the server and handling client connections.

#### Attributes

- `rooms`: A map where the key is a room ID and the value is a vector of client sockets in that room.
- `consoleMutex`: A mutex used to synchronize access to the console.
- `fileTransfers`: A map where the key is a room ID and the value is a pair containing the sender's socket and the filename for each file transfer request.

#### Methods

- `broadcastMessage(message, senderSocket, roomId)`: Sends a message from the sender to all other clients in the same room.
- `broadcastFile(filename, senderSocket, roomId)`: Sends a file transfer request from the sender to all other clients in the same room.
- `handleClient(clientSocket)`: Handles incoming messages from a client. If a message is a `/sendfile` command, it calls `broadcastFile`. If a message is a `/receivefile` command, it sends the file to the client. If a message is a `/rejoin` command, it moves the client to a new room.
- `start()`: Starts the server and waits for client connections. For each client connection, it creates a new thread and calls `handleClient`.

### ChatClient

The `ChatClient` class is responsible for managing the client and handling messages from the server.

#### Attributes

- `clientSocket`: The client's socket.

#### Methods

- `receiveMessages()`: Receives messages from the server. If a message is a `/username` command, it updates the username. If a message is a `/receivefile` command, it receives the file from the server.
- `start()`: Connects to the server, sends the room ID, and starts a thread to receive messages from the server.

### FileHandler

The `FileHandler` class is responsible for sending and receiving files.

#### Methods

- `sendFile(clientSocket, senderDirectory, filename)`: Sends a file from the sender's directory to a client.
- `receiveFile(clientSocket, receiverDirectory)`: Receives a file from the server and saves it in the receiver's directory.

## Protocol

1. The client connects to the server and sends the room ID.
2. The server acknowledges the connection and adds the client to the room.
3. The client sends messages to the server. The server broadcasts these messages to all other clients in the same room.
4. If a client wants to send a file, it sends a `/sendfile` command followed by the filename to the server. The server broadcasts a file transfer request to all other clients in the same room.
5. If a client wants to receive the file, it sends a `/receivefile` command to the server. The server then sends the file to the client.
6. If a client wants to rejoin a room, it sends a `/rejoin` command followed by the new room ID to the server. The server moves the client to the new room.
