using System;
using System.Threading;

class Program
{
    static readonly long[] data = new long[10_000_000];
    static long[] partialSums = Array.Empty<long>();
    static int numWorkers;

    static void SumSlice(int idx)
    {
        int sliceSize = data.Length / numWorkers;
        int start = idx * sliceSize;
        // The last worker takes any leftover elements when Length % numWorkers != 0.
        int end = (idx == numWorkers - 1) ? data.Length : start + sliceSize;

        long sum = 0;
        for (int i = start; i < end; i++)
        {
            sum += data[i];
        }
        partialSums[idx] = sum;
        Console.WriteLine($"  Worker {idx}: range [{start}, {end}) -> partial sum = {sum}");
    }

    static void Main()
    {
        for (int i = 0; i < data.Length; i++) data[i] = i + 1;

        numWorkers = Environment.ProcessorCount;
        partialSums = new long[numWorkers];
        Thread[] threads = new Thread[numWorkers];

        Console.WriteLine($"Array size: {data.Length:N0}, worker threads: {numWorkers}");

        for (int i = 0; i < numWorkers; i++)
        {
            int idx = i;
            threads[i] = new Thread(() => SumSlice(idx));
            threads[i].Start();
        }
        foreach (Thread t in threads)
        {
            t.Join();
        }

        long threadedTotal = 0;
        foreach (long partial in partialSums) threadedTotal += partial;

        long sequentialTotal = 0;
        foreach (long value in data) sequentialTotal += value;

        Console.WriteLine($"Threaded total:   {threadedTotal}");
        Console.WriteLine($"Sequential total: {sequentialTotal}");
        Console.WriteLine($"Match: {threadedTotal == sequentialTotal}");
    }
}
