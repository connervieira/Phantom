# Phantom

A modified variant of OpenALPR, designed to be the back-end to Predator ALPR.


## Description

[OpenALPR](https://github.com/openalpr/openalpr) is arguably the gold standard for automated license plate recognition. It's a highly customizable, multi-purpose utility designed to detect license plates in images, videos, and live streams.

Phantom is designed to be an extremely focused variant of OpenALPR. While the customizability of OpenALPR makes it a fantastic tool for all sorts of use cases, developers who know exactly what they need might find OpenALPR to be unnecessarily complex. Phantom is strictly intended to be a programming interface, not an end-user tool. There are limited command line options, and all outputs are displayed in computer-readable formats.


## Differences

Below is a list of the major differences between Phantom and vanilla OpenALPR.

### Computer Readable

Phantom returns all outputs, including errors, as JSON data. This means Phantom is a much more stable as a programming interface, since errors can be handled properly by external applications.

### Simplified Options

Phantom has much fewer command-line options. Instead of choosing what information to display on the command level, all information is directly fed to external applications, where it can be handled independently.

### Organized Code

The Phantom source code has been organized to make custom modifications and development even easier.

### Removed Daemon

Phantom isn't intended to operate as a daemon itself and instead focuses on being a utility for other applications. As such, the daemon functionality has been removed to increase compile speeds.

### Disabled Bindings

Vanilla OpenALPR has support for several different programming languages. While useful is many cases, these libraries add quite a bit of complexity, so Phantom removes them in favor of a more general support structure.


## Installation

The installation process for Phantom ALPR is extremely similar to OpenALPR. Step by step instructions can be found in the [DOCUMENTATION.md](DOCUMENTATION.md) document.
