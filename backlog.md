# Backlog for the C++ Interactivity SDK

This document lists the current work being tracked for the C++ interactivity SDK. We're always
investigating, listening to feedback and making improvements, so check back in as we develop!

The tasks below are not listed in priority order - take a peek at our release notes for updates
as we work through these work items.


## Features
* UWP build support (with authentication via MSA and XToken)
* Win32 build support (with authentication via OAuth)
* iOS build support (with authentication via OAuth)
* Android build support (with authentication via OAuth)
* Service-side aggregation (save bandwidth)
* Retrieve count of button presses
* Get input by participant
* Disable buttons
* Set button progress

## Quality Improvements
* Better message handling

### Performance
* Integrate new batching functionality exposed by the service
* More robust retry logic
* Improved performance of json message parsing
* Improved thread safety, multi-threading and parallelization.
