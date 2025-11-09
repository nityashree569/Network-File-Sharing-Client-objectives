# C++ Network File Sharing Server (Capstone Project)

This is a secure, multi-threaded file server and client built in C++ for Linux, fulfilling the requirements for Assignment 4 of the Capstone Project.

The server can handle multiple clients simultaneously and provides a command-line interface for listing, uploading ("PUT"), and downloading ("GET") files. All communications (except for the file data itself) are authenticated and encrypted.

## Features

* **Multi-threaded Server:** Can handle multiple clients at the same time using `std::thread`.
* **Authentication:** Server requires a valid username and password before accepting any commands.
* **Encryption:** All commands, responses, and filenames are encrypted using a simple XOR cipher to prevent basic network sniffing.
* **File Transfers:** Supports both uploading (`PUT`) and downloading (`GET`) of binary files.
* **File Listing:** Clients can request a list of all available files on the server (`LIST`).
* **Protocol:** Implements a simple text-based protocol (`AUTH`, `LIST`, `GET`, `PUT`, `QUIT`).

## How to Build

This project is built for a Linux environment and requires the `g++` compiler and `make` (or just `g++`).

### 1. Using the Makefile (May not work)
```bash
# This is the ideal way, but may fail due to spaces vs. tabs
make