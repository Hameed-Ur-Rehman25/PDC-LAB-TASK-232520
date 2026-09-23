using System;
using System.Diagnostics;

static class ProcessLab
{
    public static void Run()
    {
        Console.WriteLine($"[Parent] PID = {Environment.ProcessId}");
        int counter = 100;
        counter += 1;
        Console.WriteLine($"[Parent] counter after increment = {counter}, launching child...");

        var startInfo = new ProcessStartInfo
        {
            FileName = Environment.ProcessPath!,
            UseShellExecute = false
        };
        startInfo.ArgumentList.Add("--child");

        using Process child = Process.Start(startInfo)!;
        Console.WriteLine($"[Parent] started child with PID = {child.Id}");
        child.WaitForExit();
        Console.WriteLine($"[Parent] child exited with code {child.ExitCode}");

        Console.WriteLine($"[Parent] final counter = {counter}");
        Console.WriteLine("[Parent] Parent and child counters were modified independently (separate address spaces).");
    }

    public static void RunAsChild()
    {
        Console.WriteLine($"[Child]  PID = {Environment.ProcessId}");
        int counter = 100;
        counter += 50;
        Console.WriteLine($"[Child]  final counter = {counter}");
    }
}
