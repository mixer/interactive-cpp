# Xbox Sample

## Interactive Project

We don't have generic project import support in the [Interactive Studio](https://beam.pro/i/studio), so you'll have to
create your own project and incorporate it into the sample.

These json documents and screenshots represent the example interactive project used
in the Xbox sample. Please create a project similar in structure, or edit the sample
to reflect the interactive project you author.

Once you have a project created, replace the value of the the global variable 

```
s_interactiveVersion
```

with your own interactive project version id in Interactivity.cpp. You can get
your project id in the Interactive Studio ([see example screenshot here](https://github.com/WatchBeam/interactive-sdk-cpp/blob/master/Samples/Xbox/ExampleScenes/Screenshots/interactive_project_id.png)).

## Building and Running

Make sure you've run 

```
$ git submodule update --init --recursive
```

from the root of the repo before building the project. Once you've initialized the submodules,
open Interactivity.sln, set your target platform to Durango, then hit build! You should be good to go.

When you're ready to run the sample, make sure you have a signed-in Xbox LIVE account that has
a linked Beam account, with permissions to the interactive project (we recommend using a
dedicated account for development, whether that's your personal account or a test account). When
you run the sample, have a browser pulled up viewing the Beam channel associated with that Xbox
LIVE account.

When you've hit "initialize interactivity" and then "go interactive" in the sample, your channel
should now have interactive buttons displayed and ready for participant input.


## Questions, Concerns, Issues

If you run into any issues, feel free to reach out via our [issues tracker](https://github.com/WatchBeam/interactive-sdk-cpp/issues), or check out our
[C++ API Documentation](https://dev.beam.pro/reference/interactive/cplusplus/index.html).


### Quick Links

*   [Beam](https://beam.pro/)
*   [Developer Site](https://dev.beam.pro/)
*   [C++ API Documentation](https://dev.beam.pro/reference/interactive/cplusplus/index.html)
*   [Issue Tracker](https://github.com/WatchBeam/interactive-sdk-cpp/issues)
*   [Backlog](https://github.com/WatchBeam/interactive-sdk-cpp/blob/master/backlog.md)