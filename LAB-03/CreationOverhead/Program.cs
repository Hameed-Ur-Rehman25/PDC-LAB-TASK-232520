using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

class Program
{
    const int Iterations = 50;

    static void Main(string[] args)
    {
        string childPath = args.Length > 0 ? args[0] : DefaultChildPath();
        if (!File.Exists(childPath))
        {
            Console.WriteLine($"Child executable not found: {childPath}");
            Console.WriteLine("Build ProcessLab first (dotnet build ../ProcessLab -c Release) or pass its path as an argument.");
            return;
        }
        Console.WriteLine($"Child target: {childPath}");

        // Warm-up so first-run disk/JIT caching does not skew the measurements.
        RunChild(childPath);
        new Thread(() => { }).Start();

        var processStopwatch = Stopwatch.StartNew();
        for (int i = 0; i < Iterations; i++)
        {
            RunChild(childPath);
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

    static void RunChild(string path)
    {
        var startInfo = new ProcessStartInfo { FileName = path, UseShellExecute = false };
        startInfo.ArgumentList.Add("--noop");
        using Process p = Process.Start(startInfo)!;
        p.WaitForExit();
    }

    static string DefaultChildPath()
    {
        string exeName = OperatingSystem.IsWindows() ? "ProcessLab.exe" : "ProcessLab";
        string tfm = new DirectoryInfo(AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar)).Name;
        return Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,
            "..", "..", "..", "..", "ProcessLab", "bin", "Release", tfm, exeName));
    }
}
