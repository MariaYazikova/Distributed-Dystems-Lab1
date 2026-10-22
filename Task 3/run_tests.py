import subprocess
import re
import statistics

#configuration
INITIAL_KEYS = 1000
TOTAL_OPS = 50000
REPETITIONS = 10

THREADS = [1, 8, 16]

#operation ratios 
WORKLOADS = [
    #("90% searches, 5% inserts, 5% deletes", 0.90, 0.05),
    #("10% searches, 45% inserts, 45% deletes", 0.10, 0.45),
    #("50% searches, 25% inserts, 25% deletes", 0.50, 0.25),
    ("100% searches, 0% inserts, 0% deletes", 1.00, 0.00),
]

#lock implementations under test
PROGRAMS = [
    ("pthread_rwlock", "pthread_rwlock"),
    ("my_rwlock_writers", "WRITERS_PRIORITY"),
    ("my_rwlock_readers", "READERS_PRIORITY"),
]

#compilation of programs
subprocess.run([
    "gcc", "-pthread", "pth_ll_rwl.c", "my_rand.c",
    "-o", "pthread_rwlock"
], check=True)

subprocess.run([
    "gcc", "-pthread", "-DMY_LOCK", "pth_ll_rwl.c", "my_rand.c", 
    "read_write_lock.c","-o", "my_rwlock_writers"
], check=True)

subprocess.run([
    "gcc", "-pthread", "-DMY_LOCK", "-DREADERS_FIRST",
    "pth_ll_rwl.c", "my_rand.c", "read_write_lock.c",
    "-o", "my_rwlock_readers"
], check=True)

print("Compilation successful")

#results file 
with open("results_search_100.txt", "w", encoding="utf-8") as file:
    file.write(f"Initial keys: {INITIAL_KEYS}\n")
    file.write(f"Total operations: {TOTAL_OPS}\n")
    file.write(f"Repetitions: {REPETITIONS}\n\n")

    #tests for each lock implementation, thread count and workload
    for program, lock_name in PROGRAMS:
        for threads in THREADS:
            for workload, searches, inserts in WORKLOADS:
                times = []

                #current test configuration
                file.write(
                    f"Lock: {lock_name}\n"
                    f"Threads: {threads}\n"
                    f"Workload: {workload}\n"
                )
                
                #repeated execution
                for run in range(1, REPETITIONS + 1):
                    input_data = (
                        f"{INITIAL_KEYS}\n"
                        f"{TOTAL_OPS}\n"
                        f"{searches}\n"
                        f"{inserts}\n"
                    )

                    result = subprocess.run(
                        [f"./{program}", str(threads)],
                        input=input_data,
                        text=True,
                        capture_output=True
                    )
                    
                    #time extraction
                    match = re.search(
                        r"Elapsed time\s*=\s*"
                        r"([0-9.eE+-]+)",
                        result.stdout
                    )
                    elapsed = float(match.group(1))
                    times.append(elapsed)

                    #progress output in the terminal
                    print(
                        f"{program}, threads={threads}, "
                        f"{workload}, run {run}/10: "
                        f"{elapsed:.6f} s"
                    )
                    
                #execution times and average
                file.write(
                    f"All times: {times}\n"
                )
                file.write(
                    f"Mean: {statistics.mean(times):.9f} seconds\n\n"
                )

print("\nResults saved to results.txt")