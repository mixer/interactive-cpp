# Mixer Interactivity C++ SDK

The C++ Interactivity SDK supports client-side development with Mixer Interactivity.
If you have not read through our [Interactive Reference Documentation](https://dev.mixer.com/reference/interactive/) now is a great time to do that. 

## Quick Start

Clone the `interactive-cpp` repository.

```
$ git clone https://github.com/mixer/interactive-cpp.git interactive-cpp
```

Copy the `source` directory into your C++ game project (you will likely want to rename it). Add `interactivity.cpp` to your compilation list and include `interactivity.h` to get started.

See the [InteractiveSample]() for an example of how you might handle authorization and connect to an interactive session.

### Authorization
If you don't plan on handling authorization yourself you can use the provided authorization helper functions. To do so you will need an OAuth client ID which you can obtain here: https://mixer.com/lab/oauth

## Design choices
This library is designed to be easily consumed by any game project. Some of major design decisions include:
* A single header and [unity build](https://en.wikipedia.org/wiki/Single_Compilation_Unit) style cpp file.
* An extern "C" interface.
* No dependencies exposed in the header, all usage of STL and other libraries are quarantined from your game.
* Callbacks for easy memory management and managed language interop.

## Release Notes

The `interactive-cpp` repository is currently in a pre-release state. Please refer
to the [release notes](https://github.com/mixer/interactive-cpp/releases) for more information.

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
*   [Issue Tracker](https://github.com/mixer/interactive-cpp/issues)

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

