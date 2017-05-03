# Backlog for the C++ Interactivity SDK

This document lists the current work being tracked for the C++ interactivity SDK. We're always investigating, listening to feedback and making improvements, so check back in as we develop!

The tasks below are not listed in priority order - take a peek at our release notes for updates as we work through these work items.


## Features

* UWP build support (with authentication via MSA and XToken)
* Win32 build support (with authentication via OAuth)
* Dynamic control and scene creation
* Client side spark transactions
* Client side configuration of throttling parameters
* Aggregation of control events
* Service endpoint migration

## Quality Improvements

### Performance

* Better message handling
  * Integrate new batching functionality exposed by the service
  * More robust retry logic
  * Improved performance of json message parsing
* Re-incorporate Unit Tests on a platform-agnostic framework
* Improved thread safety - refactor already underway!

### Bugs