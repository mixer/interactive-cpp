# Mixer Interactivity C++ SDK

The C++ Interactivity SDK supports client-side development with Mixer Interactivity.
If you'd like to checkout the [API documentation](https://dev.mixer.com/reference/interactive/cplusplus/index.html), feel free to
take a look around. You can also take a peek at the [Xbox console sample](https://github.com/mixer/interactive-cpp/tree/master/Samples) for a
simple example of an end-to-end integration.

## Release Notes

The `interactive-cpp` repository is currently in a pre-release state. Please refer
to the [release notes](https://github.com/mixer/interactive-cpp/releases) for more information.

## Setup

This repository contains a [git submodule](https://git-scm.com/docs/git-submodule)
called `cpprestsdk`. This library is utilized for http, websocket and JSON support.

To clone the `interactive-cpp` repository and initialize the `cpprestsdk`
submodule, run the following command:

```
$ git clone --recursive https://github.com/mixer/interactive-cpp.git interactive-cpp
```

Alternatively, the submodule may be initialized independently from the clone
by running the following command:

```
$ git submodule update --init --recursive
```

The submodule points to the tip of the branch of the `cpprestsdk` repository
specified in `interactive-cpp`'s `.gitmodules` file. Note that this submodule
is a fork of the mainline cpprestsdk repo, maintained by the Xbox Live team to
support the Xbox platform.

## Contribute Back!

Is there a feature missing that you'd like to see, or have you found a bug that you
have a fix for? Do you have an idea or just interest in helping out in building the
library? Let us know and we'd love to work with you. For a good starting point on where
we are headed and feature ideas, take a look at our [requested features and bugs](https://github.com/mixer/interactive-cpp/issues) or [backlog](https://github.com/mixer/interactive-cpp/blob/master/backlog.md).

Big or small we'd like to take your contributions to help improve the Mixer Interactivity
API for everyone. 

## Having Trouble?

We'd love to get your review score, whether good or bad, but even more than that, we want
to fix your problem. If you submit your issue as a Review, we won't be able to respond to
your problem and ask any follow-up questions that may be necessary. The most efficient way
to do that is to open an issue in our [issue tracker](https://github.com/mixer/interactive-cpp/issues).  

### Quick Links

*   [Mixer](https://mixer.com/)
*   [Developer Site](https://dev.mixer.com/)
*   [C++ API Documentation](https://dev.mixer.com/reference/interactive/cplusplus/index.html)
*   [Issue Tracker](https://github.com/mixer/interactive-cpp/issues)
*   [Backlog](https://github.com/mixer/interactive-cpp/blob/master/backlog.md)

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

