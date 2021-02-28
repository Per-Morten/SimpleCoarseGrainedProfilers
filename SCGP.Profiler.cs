///
/// MIT License
/// 
/// Copyright (c) 2021 Per-Morten Straume
/// 
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.
/// 

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;

namespace SCGP
{
    /// <summary>
    /// Minimalist profiler made for course grained high-level manual profiling of applications.
    /// </summary>
    public class Profiler
    {
         /// <summary>
        /// Creates a new <see cref="Profiler"/> with a sample pool of the given <paramref name="initialCapacity"/>, with a minimum of 2 elements.
        /// </summary>
        public Profiler(int initialCapacity = 1 << 20)
        {
            Watch = Stopwatch.StartNew();
            CurrentSample = -1;
            SamplePool = new Sample[initialCapacity < 2 ? 2 : initialCapacity];
            for (int i = 0; i < SamplePool.Length; i++)
                SamplePool[i] = new Sample();
        }

        public void BeginSample(string name)
        {
            var sample = SamplePool[SampleCount];
            sample.Parent = CurrentSample;
            sample.BeginTicks = Watch.ElapsedTicks;
            sample.Name = name;
            CurrentSample = SampleCount++;

            if (SampleCount + 1 >= SamplePool.Length)
            {
                var beginTick = Watch.ElapsedTicks;
                var newArray = new Sample[SamplePool.Length * 2];
                for (int i = 0; i < SamplePool.Length; i++)
                    newArray[i] = SamplePool[i];

                for (int i = SamplePool.Length; i < newArray.Length; i++)
                    newArray[i] = new Sample();
                SamplePool = newArray;
                var endTick = Watch.ElapsedTicks;

                var s = SamplePool[SampleCount++];
                s.Parent = CurrentSample;
                s.BeginTicks = beginTick;
                s.EndTicks = endTick;
                s.Name = "SCGP.Profiler.ReallocateSamplePool";
            }
        }

        public void EndSample()
        {
            var endTicks = Watch.ElapsedTicks;
            var sample = SamplePool[CurrentSample];
            sample.EndTicks = endTicks;
            CurrentSample = sample.Parent;
        }

        /// <summary>
        /// Utility function for profiling a scope. 
        /// 
        /// <para>
        /// Example:
        /// <code>
        /// var p = new SCGP.Profiler();
        /// using (p.ScopedSample("Scope"))
        /// {
        ///     // scope to profile
        /// }
        /// </code>
        /// </para>
        /// </summary>
        public ScopedSampler ScopedSample(string name)
        {
            return new ScopedSampler(this, name);
        }

        /// <summary>
        /// <para>
        /// Formats all samples in <paramref name="profiler"/> according to the Chrome Tracing Trace Event Format and adds them to a List. One string per event.
        /// </para>
        /// 
        /// <para>
        /// See the example code for how to output them to file in a format that you can use in Chrome Tracing.
        /// </para>
        /// 
        /// <para>
        /// Overview of Chrome Tracing Trace Event Format: <a href="https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit?usp=sharing"></a>
        /// </para>
        /// 
        /// <example>
        /// <code>
        /// var sb = new StringBuilder();
        /// sb.Append("[");
        /// for (int i = 0; i &lt; events.Count; i++)
        /// {
        ///     sb.Append(events[i]);
        ///     if (i != events.Count - 1)
        ///         sb.Append(",");
        /// }
        /// sb.Append("]");
        /// System.IO.File.WriteAllText("Trace.json", sb.ToString());
        /// 
        /// </code>
        /// </example>
        /// </summary>
        public static List<string> ToChromeTracingEvents(Profiler profiler)
        {
            var list = new List<string>();
            ToChromeTracingEvents(profiler, list);
            return list;
        }

        /// <summary>
        /// Same as <see cref="ToChromeTracingEvents(Profiler)"/> except the events are appended to <paramref name="appendTo"/>.
        /// </summary>
        public static void ToChromeTracingEvents(Profiler profiler, List<string> appendTo)
        {
            for (int i = 0; i < profiler.SampleCount; i++)
            {
                var e = profiler.SamplePool[i];
                appendTo.Add($"{{ \"pid\":1, \"tid\":1, \"ts\":{e.BeginMicroseconds.ToString(CultureInfo.InvariantCulture)}, \"dur\":{e.ElapsedMicroseconds.ToString(CultureInfo.InvariantCulture)}, \"ph\":\"X\", \"name\":\"{e.Name}\", \"args\":{{ \"ms\":{e.ElapsedMilliseconds.ToString(CultureInfo.InvariantCulture)} }}}}");
            }
        }

        public class Sample
        {
            public int Parent;
            public long BeginTicks;
            public long EndTicks;
            public string Name;

            public static readonly double MicrosecondsPerTick = (1000.0 * 1000.0) / Stopwatch.Frequency;
            public double BeginMicroseconds => BeginTicks * MicrosecondsPerTick;
            public double EndMicroseconds => EndTicks * MicrosecondsPerTick;
            public double ElapsedMicroseconds => (EndTicks - BeginTicks) * MicrosecondsPerTick;

            public static readonly double MillisecondsPerTick = 1000.0 / Stopwatch.Frequency;
            public double BeginMilliseconds => BeginTicks * MillisecondsPerTick;
            public double EndMilliseconds => BeginTicks * MillisecondsPerTick;
            public double ElapsedMilliseconds => (EndTicks - BeginTicks) * MillisecondsPerTick;
        }

        public struct ScopedSampler : IDisposable
        {
            public Profiler Owner;
            public ScopedSampler(Profiler owner, string name)
            {
                Owner = owner;
                Owner.BeginSample(name);
            }

            public void Dispose()
            {
                Owner.EndSample();
            }
        }

        public int SampleCount;
        public int CurrentSample;
        public Sample[] SamplePool;
        public Stopwatch Watch;
    }
}
