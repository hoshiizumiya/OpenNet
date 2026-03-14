# 🎉Welcome to the OpenNet project
<p align="center">

</p>
﻿<p align="center">
  <img src="https://github.com/hoshiizumiya/OpenNet/blob/master/OpenNet/Assets/AppIcons/StoreLogo.scale-400.png" alt="OpenNet Banner" width="600"/>
</p>

[简体中文](README_zh-CN.md)

## Overview

Our current framework is WinUI3 with C++/WinRT. Using WinUI3 allows us to create modern Windows applications with a contemporary look and feel, while C++/WinRT provides improved user experience and performance.  
Currently, we plan to run only on Windows. Cross-platform support may be considered in the future (for cross-platform native apps using the MAUI build system and cross-platform calls via P/Invoke).  
The main open-source library we currently use are `libtorrent` and `aria2`.

## How to build the debug project

1. Development environment requirements

* Windows 11 25H2 or later — we recommend using the latest version of Windows 11.
* Visual Studio 2026.
* Workloads: C++ Desktop Development, WinUI Desktop Development, and C++ WinUI app tools.
* Windows 11 SDK (10.0.26100.0 or later)
* MSVC v145(or open solution and follow vs install prompts)
* `vcpkg` package manager (the VS-integrated version is acceptable)
* You may need to have [vcpkg](https://vcpkg.io/) installed and integrate setup for msbuild. See [this documentation for guide](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-msbuild?pivots=shell-powershell).

2. Build the project

* Open Visual Studio and choose to clone the repository.
* Enter the project's Git repository URL and choose a local path with more than 30GB free space. Avoid spaces and non-ASCII characters in the path, as some older dependencies may fail. Then click "Clone". If your network is unreliable, consider using SSH or a TUN proxy for Git.
* Open OpenNet.slnx in Visual Studio.
* In Solution Explorer, ensure the bolded project is `OpenNet` and configured as the startup project.
* Click the green Start button to begin debugging and launch the application.
* On first run, the application will automatically download required NuGet and `vcpkg` dependencies and build them. This may take some time depending on configuration (around 30–60 minutes). Since `vcpkg` is hosted on GitHub, use a proxy if your network is restricted.

## Features

* Support for downloading via BitTorrent using `.torrent` files/magnet links and http/https download.
* RSS subscription and automatic downloading.
* Nat tools to check network status.

## Preview
**NOT THE FINAL PRODUCT**

![TasksDownloadSpeedGraph](docs/assets/TasksDownloadSpeedGraph.png)
![TasksFiles](docs/assets/TasksFiles.png)
![TasksPagePeers](docs/assets/TasksPagePeers.png)

## Roadmap

* Support for HTTP/HTTPS/FTP file downloads.
* Support for automatic NAT traversal.
* Support for distributed network features such as DHT, PEX, and LSD.
* Support for NAT detection.
* Support for remote control.
* Support for user login and multi-user management.

## Learn about the project
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/hoshiizumiya/OpenNet)

## Contribute

MVVM framework: @https://github.com/AlexAdasCca