///
/// MIT License
/// 
/// Copyright (c) 2022 Per-Morten Straume
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
#pragma once
#include <chrono>
#include <cstdint>
#include <vector>
#include <cstdarg>
#include <string>

namespace SCGP
{
    /// <summary>
    /// Minimalist profiler made for course grained high-level manual profiling of applications.
    /// </summary>
    class Profiler
    {
    public:
        struct Sample
        {
            std::int64_t Parent;
            std::uint64_t BeginNanoseconds;
            std::uint64_t EndNanoseconds;
            const char* Name;
        };

        struct ScopedSampler
        {
            Profiler* Owner;
            ScopedSampler(Profiler* owner, const char* name)
            {
                Owner = owner;
                Owner->BeginSample(name);
            }

            ~ScopedSampler()
            {
                Owner->EndSample();
            }
        };

        Sample* SamplePool;
        std::int64_t SampleCount;
        std::int64_t CurrentSample;
        std::int64_t SampleCapacity;
        std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> ClockStart;

        /// <summary>
        /// Creates a new <see cref="Profiler"/> with a sample pool of the given <paramref name="initialCapacity"/>, with a minimum of 2 elements.
        /// </summary>
        Profiler(int initialCapacity = 1 << 20)
        {
            SampleCount = 0;
            CurrentSample = -1;
            SampleCapacity = initialCapacity < 2 ? 2 : initialCapacity;
            SamplePool = new Sample[SampleCapacity];
            ClockStart = std::chrono::steady_clock::now();
        }

        /// <summary>
        /// Note: string pointed to by <paramref name="name"/> must outlive the calls to ToChromeTracingEvents
        /// </summary>
        void BeginSample(const char* name)
        {
            auto& sample = SamplePool[SampleCount];
            sample.Parent = CurrentSample;
            sample.BeginNanoseconds = ElapsedNanoseconds();
            sample.Name = name;
            CurrentSample = SampleCount++;

            if (SampleCount + 1 >= SampleCapacity)
            {
                auto begin = ElapsedNanoseconds();
                auto new_capacity = SampleCapacity * 2;
                auto new_sample_pool = new Sample[new_capacity];
                std::memcpy(new_sample_pool, SamplePool, SampleCapacity * sizeof(Sample));
                delete[] SamplePool;
                SamplePool = new_sample_pool;
                SampleCapacity = new_capacity;

                auto end = ElapsedNanoseconds();
                auto& sample = SamplePool[SampleCount++];
                sample.Parent = CurrentSample;
                sample.BeginNanoseconds = begin;
                sample.EndNanoseconds = end;
                sample.Name = "SCGP.Profiler.ReallocateSamplePool";
            }
        }

        void EndSample()
        {
            auto endTicks = ElapsedNanoseconds();
            auto& sample = SamplePool[CurrentSample];
            sample.EndNanoseconds = endTicks;
            CurrentSample = sample.Parent;
        }

        std::uint64_t ElapsedNanoseconds()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - ClockStart).count();
        }

        /// <summary>
        /// Utility function for profiling a scope. 
        /// 
        /// <para>
        /// Note: string pointed to by <paramref name="name"/> must outlive the calls to ToChromeTracingEvents
        /// </para>
        /// 
        /// <para>
        /// Example:
        /// <code>
        /// {
        ///     auto p = SCGP.Profiler();
        ///     {
        ///         auto _ = p.ScopedSample("Scope");
        ///         // scope to profile
        ///     }
        ///     
        /// }
        /// </code>
        /// </para>
        /// </summary>
        ScopedSampler ScopedSample(const char* name)
        {
            return ScopedSampler(this, name);
        }

        /// <summary>
        /// <para>
        /// Formats all samples in <paramref name="profiler"/> according to the Chrome Tracing Trace Event Format and adds them to a std::vector. One string per event.
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
        /// auto contents = std::string();
        /// contents += "[";
        /// for (int i = 0; i &lt; events.size(); i++)
        /// {
        ///     contents += events[i];
        ///     if (i != events.size() - 1)
        ///         contents += ",";
        /// }
        /// contents += "]";
        /// auto trace_file = std::fopen("trace.json", "w+");
        /// std::fprintf(trace_file, "%s", contents.c_str());
        /// std::fclose(trace_file);
        /// 
        /// </code>
        /// </example>
        /// </summary>
        static std::vector<std::string> ToChromeTracingEvents(const Profiler& profiler)
        {
            std::vector<std::string> list;
            ToChromeTracingEvents(profiler, list);
            return list;
        }

        /// <summary>
        /// Same as <see cref="ToChromeTracingEvents(Profiler)"/> except the events are appended to <paramref name="appendTo"/>.
        /// </summary>
        static void ToChromeTracingEvents(const Profiler& profiler, std::vector<std::string>& appendTo)
        {
            for (int i = 0; i < profiler.SampleCount; i++)
            {
                auto& e = profiler.SamplePool[i];
                appendTo.push_back(FmtString("{ \"pid\":1, \"tid\":1, \"ts\": %f, \"dur\": %f, \"ph\":\"X\", \"name\":\"%s\", \"args\":{ \"ms\":%f }}",
                                             e.BeginNanoseconds / 1000.0,
                                             (e.EndNanoseconds - e.BeginNanoseconds) / 1000.0,
                                             e.Name,
                                             (e.EndNanoseconds - e.BeginNanoseconds) / (1e+6)));
            }
        }

        static std::string FmtString(const char* format, ...)
        {
            std::va_list args1;
            va_start(args1, format);
            std::va_list args2;
            va_copy(args2, args1);
            const int size = std::vsnprintf(nullptr, 0, format, args1) + 1;
            va_end(args1);
            std::vector<char> tmp(size);
            std::vsnprintf(tmp.data(), tmp.size(), format, args2);
            va_end(args2);
            return{ tmp.data() };
        }
    };
}
