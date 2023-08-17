# network_backend

`network_backend` is a versatile networking library designed with the vision of providing a unified interface for network communication. The core idea is to allow users to select the desired networking framework, library, and technology via a configuration, making the selection process completely transparent for the upper layers.

## Motivation

This project began as a proof of concept in my professional context. The primary goal was to abstract away the complexities of different networking technologies and present a single, consistent interface to the upper layers of software. By doing so, we can easily switch between different network backends without altering the core application logic.

## Features

- **Unified Networking Interface**: Regardless of the backend technology, the interface remains consistent.
  
- **Multiple Backends**: The library currently supports multiple backends including:
  - Vanilla TCP with sockets (Posix)
  - Libfabric (work in progress)
  - UCX (tbd)
  
- **Event Loop**: An in-built event loop mechanism for managing asynchronous network events.

- **Ping-Pong Example**: A simple demonstration of how to use the library for basic network communication tasks.

- **Unit Tests**: Ensuring the functionality and reliability of the library components.

## Future Work

While the initial work on the Libfabric and RDMA implementation has begun in this repository, the continuation of this work is being carried out in the following repository: [nn-backend at CERN GitLab](https://gitlab.cern.ch/atlas-tdaq-felix/nn-backend).

## Getting Started

### Prerequisites

- **Operating System**: Specify the supported operating systems (e.g., Linux, macOS, Windows).
- **Dependencies**: List any software or libraries that need to be installed beforehand. For instance:
  - CMake (Version 3.8 or higher)
  - Libfabric (if necessary for certain backends). Find the latest version at: https://github.com/ofiwg/libfabric

### Installation

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/fzhn/network_backend.git
   cd network_backend
   ```

2. **Configure with CMake**:
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

3. **Build the Project**:
   ```bash
   make
   ```

### Running the Examples

- **Ping-Pong Example**:
  ```bash
  ./ping_pong
  ```

### Configuration

- **Selecting a Network Backend**: Explain how users can select a specific network backend through configuration. If there's a configuration file or command-line arguments, provide details on how to use them.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.