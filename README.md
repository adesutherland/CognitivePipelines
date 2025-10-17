# Cognitive Pipeline Application

## Description

This is a cross-platform desktop application designed for building and executing "cognitive pipelines." The application provides a user interface for chaining together different processing modules, including interfacing with Large Language Models (LLMs) and legacy C libraries. The goal is to create a flexible and extensible tool for complex, multi-step automated tasks.

This project is being developed with the assistance of an AI agent.

## Features

* **Cross-Platform:** Built with C++ and Qt to run on Windows, macOS, and Linux.
* **Modular Architecture:** A layered design separates the UI, core logic, and backend services.
* **LLM Integration:** Core functionality includes sending prompts to and receiving responses from LLM APIs.
* *(More features to be added)*

## Dependencies

This project relies on system package managers to provide all third-party libraries. Install the following before configuring with CMake:

- Qt 6 (Core, Gui, Widgets)
- Boost (1.70 or newer)
- cpr (C++ Requests)
- OpenSSL (development headers)
- Zlib (development headers)

macOS (Homebrew):

    brew update
    brew install qt boost cpr

Ubuntu (apt):

    sudo apt-get update
    sudo apt-get install -y build-essential cmake qt6-base-dev libboost-all-dev libcpr-dev libssl-dev zlib1g-dev

Windows (vcpkg, x64 triplet):

    # One-time clone/setup if needed
    git clone https://github.com/microsoft/vcpkg.git
    .\vcpkg\bootstrap-vcpkg.bat
    # Install deps
    vcpkg install boost:x64-windows cpr:x64-windows

When configuring with CMake on Windows, pass the vcpkg toolchain file:

    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\\scripts\\buildsystems\\vcpkg.cmake"

## Installation

Build with CMake as usual after installing dependencies. On macOS, ensure Qt is discoverable via CMAKE_PREFIX_PATH if needed (e.g., /opt/homebrew/opt/qt).

## Usage

*(Instructions on how to use the application will be added here.)*

## Configuration

Before running the application, create your local API key configuration file:

1. Copy the template file to a local, ignored file:
   
       cp accounts.json.example accounts.json

2. Open accounts.json and replace the placeholder value YOUR_API_KEY_HERE with your actual API key.

Notes:
- The accounts.json file is intentionally ignored by Git to protect your secrets.
- The JSON structure supports multiple accounts via the accounts array; you can add more entries later as needed.

## License

This project is licensed under the MIT License. See the [LICENSE](https://www.google.com/search?q=LICENSE) file for details.
