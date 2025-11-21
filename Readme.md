# Minimal Sockets

![Status](https://img.shields.io/badge/status-development-orange) ![Language](https://img.shields.io/badge/language-C-blue)

**Minimal Sockets** is a lightweight TCP socket library written in C. It is designed to simplify the creation of basic `server <-> client` applications in the console.

> **Note:** This library is currently in active development as a hobby project. My primary goal is to learn the fundamentals of C programming and network architecture.

## Goals

The immediate roadmap for this library includes:
1.  Solidifying the basic TCP connection logic + include the support for other protocols.
2.  Transitioning from a simple **Echo Server** (current state) to a fully functional **Console Chat Application**.
3.  Improving error handling and memory management.
4.  Improving the overall structure and consistency of the library.

## Getting Started

### Prerequisites
* GCC (or any standard C compiler)

### Usage
* Just clone the repo, or copy paste the nsock.h header file in your project. This project may eventually stop being a stb style library.
* Then dont forget the add the `#define MSOCK_IMPLEMENTATION` in one of you project files.
* Take a look inside the examples folder on how to use the library.
* To build the examples just bootstrap the nob.c by compling it one time into nob.exe and just run. To include debug symbols run `.\nob.exe -d`

## Refrences
* Tsoding (Nobuild): This project makes use of the [Nobuild](https://github.com/tsoding/nobuild) concept by Tsoding. 
A huge shoutout to the [Tsoding Daily](https://www.youtube.com/@TsodingDaily) channel. his content inspired me to start 
learning C for fun!

## Note
* Please if you have any tips or input for the project, feel free to create an issue or a pull request!