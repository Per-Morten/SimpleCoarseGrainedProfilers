## What is Simple Coarse Grained Profilers?
Simple Coarse Grained Profilers is supposed to be a collection of minimalist "plug and play" high level profilers.

## Why is Simple Coarse Grained Profilers?
The profilers are written for the situation where you're working in a language that either don't have any good profiling options built in, 
or where there only exists third party options with large up front integartion costs, such as dependencies, 
configuration, custom applications to view the profiling data, or other types of integration complexity.

The idea is that if you come to a new language where the profiling tooling is lacking, you can copy and paste one of the profilers here, 
make some minor language adjustments, then you're ready to go.

## What do you use Simple Coarse Grained Profilers for?
Since these profilers are written to be easy to integrate and port to other languages, you should only use them for high level profiling. 
None of the profilers make use of language specific mechanisms that could lead to better performance, 
so the overhead can affect measurements if the execution time of the code you're executing is very short.
Additionally, there is a considerable amount of overhead from the profiler when it needs to reallocate its sample pool. 
However, this will be shown as an event in the profiling data, allowing you to detect it. To avoid these reallocations, 
ensure that the profiler is given enough capacity when it's created.

## How do you use the Profiler?
The API should be relatively straight forward. To profile some code you call BeginSample with the name of the sample before the code you want to profile, 
and EndSample at the end of the code you want to profile. 
If the language supports destructors (C++), "try with resources" (Java), or "scoped using statements" (C#), 
then you can use the ScopedSample function instead of having to call BeginSample and EndSample. 
Both the BeginSample/EndSample and ScopedSample is shown in the example below.
For each BeginSample you must have a matching EndSample. 
Currently there's no future proof mechanism that reports if you do this incorrectly, any exceptions or errors thrown should not be relied upon for the future.

```csharp
var profiler = new SCGP.Profiler();
for (int i = 0; i < 100; i++)
{
    profiler.BeginSample($"Sample A"); // Start of "Sample A"
    for (int j = 0; j < 100; j++)
    {
        using (profiler.ScopedSample($"Sample A::A")) // Start of "Sample A::A"
            for (int k = 0; k < 100; k++)
            {

            }
    } // End of "Sample A::A"
    profiler.EndSample(); // End of "Sample A"
}
```


## Where can you see the Profiling Data?
Currently, the profilers support translating the data to Chrome Tracing events. 
These can be viewed in any Google Chrome browser by going to chrome://tracing and loading the file containing the trace events.
See the following example on how to generate such a file in somewhat language independent C#.

```csharp
var profiler = new SCGP.Profiler();

// Go and profile some stuff

// After you've finished profiling:
List<string> events = SCGP.Profiler.ToChromeTracingEvents(profiler);
string chromeTracableJSONFile = "[";
for (int i = 0; i < events.Count; i++)
{
    chromeTracableJSONFile += events[i];
    if (i < events.Count - 1)
        chromeTracableJSONFile += ",";
}
chromeTracableJSONFile += "]";
System.IO.File.WriteAllText("TraceGrowData.json", chromeTracableJSONFile);
```
