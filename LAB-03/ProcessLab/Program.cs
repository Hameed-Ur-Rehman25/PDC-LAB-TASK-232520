using System;
using System.Diagnostics;

class Program
{
    static void Main(string[] args)
    {
        if (args.Length > 0 && args[0] == "--child")
        {
            RunAsChild();
        }
        else if (args.Length > 0 && args[0] == "--noop")
        {
            // Trivial child target for Task 3 (CreationOverhead): exit immediately.
            return;
        }
        else
        {
            RunAsParent();
        }
    }

    static void RunAsChild()
    {
        Console.WriteLine($"[Child]  PID = {Environment.ProcessId}");
        int counter = 100;
        counter += 50;
        Console.WriteLine($"[Child]  final counter = {counter}");
    }

    static void RunAsParent()
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
}
