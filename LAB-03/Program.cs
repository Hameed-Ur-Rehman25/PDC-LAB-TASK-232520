using System;

class Program
{
    static void Main(string[] args)
    {
        string mode = args.Length > 0 ? args[0] : "all";

        switch (mode)
        {
            case "--child": ProcessLab.RunAsChild(); return;   // launched by Task 1
            case "--noop": return;                               // trivial child target for Task 3
            case "1": ProcessLab.Run(); break;
            case "2": ArraySumThreads.Run(); break;
            case "3": CreationOverhead.Run(); break;
            case "4": ThreadLifecycle.Run(); break;
            case "all":
                RunAll();
                break;
            default:
                Console.WriteLine("Usage: dotnet run -c Release -- [1|2|3|4|all]");
                break;
        }
    }

    static void RunAll()
    {
        Header("Task 1: Process Creation and Address-Space Separation");
        ProcessLab.Run();
        Header("Task 2: Summing Array Slices Across Worker Threads");
        ArraySumThreads.Run();
        Header("Task 3: Measuring Process- vs. Thread-Creation Overhead");
        CreationOverhead.Run();
        Header("Task 4: Thread Lifecycle States");
        ThreadLifecycle.Run();
    }

    static void Header(string title)
    {
        Console.WriteLine();
        Console.WriteLine($"===== {title} =====");
    }
}
