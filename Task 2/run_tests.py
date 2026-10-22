import subprocess
import sys
import os
import matplotlib.pyplot as plt

# Настройки теста
PROGRAM_NAME = "./nbody_barrier"     
INPUT_FILE = "test.txt"        
TEND = "5.0"                  
N_VALUES = [100, 200, 500, 1000, 2000, 5000]    
THREADS_LIST = [1, 2, 4, 8, 10] 
RUNS = 5                      
colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']


def run_benchmark(n_threads, n_points):
    times = []
    input_filename = f"test_{n_points}.txt" 

    for i in range(RUNS):
        try:
            result = subprocess.run(
                [PROGRAM_NAME, TEND, input_filename, str(n_threads)], 
                capture_output=True, text=True, check=True
            )
            output = result.stdout.strip()

            for line in output.split('\n'):
                if "Simulation time:" in line:
                    time_val = float(line.split(":")[1].split()[0])
                    times.append(time_val)
                    break
                    
        except Exception as e:
            print(f"Ошибка при запуске: {e}")
            return None
    if times:
        return sum(times) / len(times)
    return None

def visualization(results):
    for i, thread in enumerate(THREADS_LIST):
        plt.plot(N_VALUES, results[thread], marker='o', color = colors[i], label = f"{thread} потоков")
    plt.xlabel('Количество входных точек (N)')
    plt.ylabel('Время выполнения (sec)')
    plt.title("График зависимости времени выполнения от N по разным потокам")
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(N_VALUES)
    plt.show()

def main():
    print(f"{'N':<10} | {'Threads':<10} | {'Avg Time (s)':<15}")
    print("-" * 50)
    results = {}
    for t in THREADS_LIST:
        results[t] = []

    for n in N_VALUES:
        for t in THREADS_LIST:            
            avg_time = run_benchmark(t, n)
            time_str = f"{avg_time:.6f}"
            
            print(f"{n:<10} | {t:<10} | {time_str:<15}")
            results[t].append(avg_time)
            
    visualization(results)
    

if __name__ == "__main__":
    main()