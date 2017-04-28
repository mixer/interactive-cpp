# Beam Interactivity C++ SDK

The C++ Interactivity SDK supports client-side development with Beam Interactivity.

## Release Notes

The `interactive-sdk-cpp` repository is currently in a pre-alpha state.

## Setup

This repository contains a [git submodule](https://git-scm.com/docs/git-submodule)
called `cpprestsdk`. This library is utilized for http, websocket and JSON support.

To clone the `interactive-sdk-cpp` repository and initialize the `cpprestsdk`
submodule, run the following command:

```
$ git clone --recursive https://github.com/WatchBeam/interactive-sdk-cpp.git interactive-sdk-cpp
```

Alternatively, the submodule may be initialized independently from the clone
by running the following command:

```
$ git submodule update --init --recursive
```

The submodule points to the tip of the branch of the `cpprestsdk` repository
specified in `interactive-sdk-cpp`'s `.gitmodules` file. Note that this submodule
is a fork of the mainline cpprestsdk repo, maintained by the Xbox Live team.

## Building

TBD

## Contribute Back!

Is there a feature missing that you'd like to see, or found a bug that you have a fix for? Do you have an idea or just interest in helping out in building the library? Let us know and we'd love to work with you. For a good starting point on where we are headed and feature ideas, take a look at our [requested features and bugs](https://github.com/Microsoft/xbox-live-beam-api/issues).

Big or small we'd like to take your contributions to help improve the Beam Interactivity API for everyone. 

## Having Trouble?

We'd love to get your review score, whether good or bad, but even more than that, we want to fix your problem. If you submit your issue as a Review, we won't be able to respond to your problem and ask any follow-up questions that may be necessary. The most efficient way to do that is to open a an issue in our [issue tracker](https://github.com/WatchBeam/interactive-sdk-cpp/issues).  

### Quick Links

*   [Beam](https://beam.pro/)
*   [Developer Site](https://dev.beam.pro/)
*   [Getting Started Guide](https://dev.beam.pro/reference/interactive/index.html#cpp_getting-started)
*   [Interactivity SDK C# Repo](https://github.com/WatchBeam/interactive-sdk-cpp)
*   [Issue Tracker](https://github.com/WatchBeam/interactive-sdk-cpp/issues)

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

