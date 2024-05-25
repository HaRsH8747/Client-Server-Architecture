# Client-Server File Retrieval System

## Overview

This project implements a client-server file retrieval system using socket programming in C. The system allows clients to request files or directories from the server, which searches for the requested items in its file directory and returns them to the client. Multiple clients can connect to the server concurrently, and the server employs process forking and signal handling for efficient management of client requests.

## Features

- **Client Commands:** Clients can execute various commands to request files or directories from the server, including listing directories, retrieving files by name, size, type, or creation date, and terminating the connection.
- **Process Management:** The server uses process forking to handle multiple client requests concurrently, ensuring efficient resource utilization and responsiveness.
- **Signal Handling:** Signal handling mechanisms are employed to manage communication between parent and child processes, ensuring proper termination and error handling.
- **Alternating Servers:** Multiple instances of the server run on different machines/terminals, alternating between handling client connections based on a predefined pattern.

## Technologies Used

- **Socket Programming:** Implemented client-server communication using sockets to facilitate data exchange.
- **C Programming:** Developed server-side (serverw24, mirror1, mirror2) and client-side (clientw24) applications using the C language.
- **Linux Environment:** Executed the project within a Linux environment, utilizing Linux commands and utilities.
- **Process Control:** Utilized process forking for concurrent handling of client requests and signal handling for inter-process communication.
- **Plagiarism Detection Tool:** MOSS was used for ensuring code integrity and originality.

## Usage

1. Clone the repository to your local machine.
2. Compile the server and client programs using a C compiler (e.g., GCC).
3. Run the server program (`serverw24`) on the designated machine/terminal.
4. Run the client program (`clientw24`) on client machines/terminals and follow the command syntax for requesting files or directories from the server.

## Contributing

Contributions are welcome! Please fork the repository and create a pull request with your proposed changes.

## License

This project is licensed under the [MIT License](LICENSE).
