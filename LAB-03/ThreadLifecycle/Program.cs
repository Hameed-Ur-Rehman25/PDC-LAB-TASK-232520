using System;
using System.Threading;

class Program
{
    static void Worker()
    {
        Thread.Sleep(200);
    }

    static void Main()
    {
        Thread t = new Thread(Worker);
        Console.WriteLine($"After creation:             {t.ThreadState}"); // conceptually: New

        t.Start();
        Console.WriteLine($"Immediately after Start():  {t.ThreadState}"); // conceptually: Runnable/Ready or Running

        Thread.Sleep(50); // give the worker a moment to reach Thread.Sleep(200)
        Console.WriteLine($"While worker is sleeping:   {t.ThreadState}"); // conceptually: Blocked/Waiting

        t.Join();
        Console.WriteLine($"After Join() completes:     {t.ThreadState}"); // conceptually: Terminated
    }
}
