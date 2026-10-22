import random
import sys

def generate_nbody_file(filename, n_points):
    with open(filename, 'w') as f:
        f.write(f"{n_points}\n")
        
        for _ in range(n_points):
            mass = random.uniform(1.0, 100.0)
            
            # Координаты x, y, z от -10.0 до 10.0
            x = random.uniform(-10.0, 10.0)
            y = random.uniform(-10.0, 10.0)
            z = random.uniform(-10.0, 10.0)
            
            # Начальные скорости vx, vy, vz 
            vx = random.uniform(-1.0, 1.0)
            vy = random.uniform(-1.0, 1.0)
            vz = random.uniform(-1.0, 1.0)
            
            # Записываем в формате: m x y z vx vy vz
            f.write(f"{mass:.6f} {x:.6f} {y:.6f} {z:.6f} {vx:.6f} {vy:.6f} {vz:.6f}\n")
    
    print(f"Файл '{filename}' успешно создан с {n_points} точками.")

if __name__ == "__main__":
    # Список размеров задач для тестов
    test_sizes = [100, 200, 500, 1000, 2000, 5000]
    
    for n in test_sizes:
        filename = f"test_{n}.txt"
        generate_nbody_file(filename, n)