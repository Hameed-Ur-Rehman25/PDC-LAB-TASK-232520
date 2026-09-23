using System;
using System.Diagnostics;
using System.Threading;

static class CreationOverhead
{
    const int Iterations = 50;

    public static void Run()
    {
        // Warm-up so first-run disk/JIT caching does not skew the measurements.
        RunChildProcess();
        new Thread(() => { }).Start();

        var processStopwatch = Stopwatch.StartNew();
        for (int i = 0; i < Iterations; i++)
        {
            RunChildProcess();
        }
        processStopwatch.Stop();

        var threadStopwatch = Stopwatch.StartNew();
        for (int i = 0; i < Iterations; i++)
        {
            Thread t = new Thread(() => { });
            t.Start();
            t.Join();
        }
        threadStopwatch.Stop();

        double avgProcessMs = processStopwatch.Elapsed.TotalMilliseconds / Iterations;
        double avgThreadMs = threadStopwatch.Elapsed.TotalMilliseconds / Iterations;

        Console.WriteLine($"Iterations: {Iterations}");
        Console.WriteLine($"Average process creation time: {avgProcessMs:F3} ms");
        Console.WriteLine($"Average thread creation time:  {avgThreadMs:F3} ms");
        Console.WriteLine($"Process creation was {(avgProcessMs / avgThreadMs):F1}x more expensive than thread creation.");
    }

    static void RunChildProcess()
    {
        var startInfo = new ProcessStartInfo { FileName = Environment.ProcessPath!, UseShellExecute = false };
        startInfo.ArgumentList.Add("--noop");
        using Process p = Process.Start(startInfo)!;
        p.WaitForExit();
    }
}
