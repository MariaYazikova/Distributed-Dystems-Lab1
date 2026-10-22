#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "timer.h"
#include <math.h>

// Объявление констант
#define G 6.67430e-11
#define delta_t 0.01

// Глобальные массивы для суммарных сил
double *global_fx = NULL;
double *global_fy = NULL;
double *global_fz = NULL;

// Глобальные переменные
int num_steps;
int N;
int NTHREADS;
int count;
FILE *outfile;
pthread_mutex_t mutex;
pthread_cond_t cond;

// Структура точки
typedef struct {
    double m;
    double x;
    double y;
    double z;
    double vx;
    double vy;
    double vz;
} Point;

// Структура потока
typedef struct {
    int rank;
    int start_idx;
    int end_idx;

    double *local_fx;
    double *local_fy;
    double *local_fz;
    Point *points;

} ThreadData;
// Барьер
void *my_barrier(){
    pthread_mutex_lock(&mutex);
    count++;
    if (count == NTHREADS){
        count = 0;
        pthread_cond_broadcast (&cond);
    }
    else while (pthread_cond_wait(&cond , &mutex) != 0);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

// Подсчёт суммарных сил точек
void *compute_forces(void *arg){
    ThreadData *data = (ThreadData *)arg; // Приведение типа

    // Обнуление между итерациями
    for(int i=0; i<N; i++) {
        data->local_fx[i] = 0.0;
        data->local_fy[i] = 0.0;
        data->local_fz[i] = 0.0;
    }

    // Проход по массиву потока
    for (int q = data->start_idx; q < data->end_idx; q++){
        // Берём только последующие элементы
        for (int k = q+1; k < N; k++){
            // Векторы
            double dx = data->points[k].x - data->points[q].x;
            double dy = data->points[k].y - data->points[q].y;
            double dz = data->points[k].z - data->points[q].z;
            
            // Расстояние между точками
            double r = sqrt(dx*dx + dy*dy + dz*dz);
            double factor = G * data->points[k].m * data->points[q].m / (r * r * r);
            // Расчёт сил
            data->local_fx[k] += factor * dx;
            data->local_fy[k] += factor * dy;
            data->local_fz[k] += factor * dz;
            // Третий закон НЬютона
            data->local_fx[q] -= factor * dx;
            data->local_fy[q] -= factor * dy;
            data->local_fz[q] -= factor * dz; 
        }
    }
    // Захват мьютекса для записи в глобальный массив 
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < N; i++) {
        global_fx[i] += data->local_fx[i];
        global_fy[i] += data->local_fy[i];
        global_fz[i] += data->local_fz[i];
    }
    // Освобождение мьтекса
    pthread_mutex_unlock(&mutex);
    my_barrier();
    return NULL;
}

// Обновление координат и скорости
void *update_coord(void *arg){
    ThreadData *data = (ThreadData *)arg;
    // Проход по элементам потока
    for (int i=data->start_idx; i<data->end_idx; i++){
        data->points[i].x = data->points[i].x + data->points[i].vx*delta_t;
        data->points[i].vx = data->points[i].vx + global_fx[i]*delta_t/data->points[i].m;

        data->points[i].y = data->points[i].y + data->points[i].vy*delta_t;
        data->points[i].vy = data->points[i].vy + global_fy[i]*delta_t/data->points[i].m;
        
        data->points[i].z = data->points[i].z + data->points[i].vz*delta_t;
        data->points[i].vz = data->points[i].vz + global_fz[i]*delta_t/data->points[i].m;
    }
    my_barrier();
    return NULL;
}

void *thread_main(void *arg){
    ThreadData *data = (ThreadData *)arg;
    for (int step = 0; step < num_steps; step++) {
        if (data->rank == 0) {
            for (int i = 0; i < N; i++) {
                global_fx[i] = 0.0;
                global_fy[i] = 0.0;
                global_fz[i] = 0.0;
            }
        }
        my_barrier();
        double t = (step + 1) * delta_t;
        compute_forces(data);
        update_coord(data);
        // Вывод в CSV
        if (data->rank == 0) {
            fprintf(outfile, "%.6f", t);
            for (int i = 0; i < N; i++) fprintf(outfile, " %.6f %.6f", data->points[i].x, data->points[i].y);
            fprintf(outfile, "\n");
        }
        my_barrier();
    }
    return NULL;
}

int main(int argc, char* argv[]){
    pthread_cond_init (&cond, NULL );
    pthread_mutex_init(&mutex, NULL );

    double start, finish; 
    // Запуск таймера
    GET_TIME(start);    

    // Чтение аргументов командной строки: tend, filename  
    double tend = atof(argv[1]);
    char* filename = argv[2];
    NTHREADS = atoi(argv[3]); // Добавь это, и тогда не придется перекомпилировать!

    // Открытие входного файла с данными
    FILE *infile = fopen(filename, "r");

    // Чтение первой строки файла и запись в N
    fscanf(infile, "%d", &N);

    // Глобальные массивы, динамическая память
    global_fx = (double*)calloc(N, sizeof(double));
    global_fy = (double*)calloc(N, sizeof(double));
    global_fz = (double*)calloc(N, sizeof(double));

    // Массив структур для хранения состояния всех точек
    Point *points = malloc(N * sizeof(Point));
    // Чтение всех точек
    for(int i=0; i<N; i++) {
        fscanf(infile, "%lf %lf %lf %lf %lf %lf %lf", 
            &points[i].m, 
            &points[i].x, &points[i].y, &points[i].z,
            &points[i].vx, &points[i].vy, &points[i].vz);
    }
    fclose(infile);

    // Массивы для идентификаторов поток и их данных
    pthread_t thread_handles[NTHREADS];
    ThreadData thread_data[NTHREADS]; 

    // Количество шагов 
    num_steps = (int)(tend / delta_t);

    // Инициализация точек и выделение памяти под локальные массивы
    for(int i=0; i<NTHREADS; i++) {
        thread_data[i].local_fx = (double*)calloc(N, sizeof(double));
        thread_data[i].local_fy = (double*)calloc(N, sizeof(double));
        thread_data[i].local_fz = (double*)calloc(N, sizeof(double));
        
        thread_data[i].points = points;
        
        // Распределение расчётов по потокам
        int h = N/NTHREADS;
        thread_data[i].start_idx = i * h;
        // Если это последний поток, он забирает все оставшиеся точки
        if (i == NTHREADS - 1) {
            thread_data[i].end_idx = N;
        } else {
            thread_data[i].end_idx = thread_data[i].start_idx + h;
        }
        thread_data[i].rank = i;
    }
    // Открываем файл для записи результатов
    outfile = fopen("output.csv", "w");
    
    // Запись начального состояния
    fprintf(outfile, "%.6f", 0.0);
    for(int i=0; i<N; i++) {
        fprintf(outfile, " %.6f %.6f", points[i].x, points[i].y);
    }
    fprintf(outfile, "\n");

    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&thread_handles[i], NULL, thread_main, &thread_data[i]);
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(thread_handles[i], NULL);
    // Остановка таймера
    GET_TIME(finish);
    
    printf("Simulation time: %.6f seconds\n", finish - start);
    
    fclose(outfile);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    // Освобождение памяти
    for(int i=0; i<NTHREADS; i++) {
        free(thread_data[i].local_fx);
        free(thread_data[i].local_fy);
        free(thread_data[i].local_fz);
    }
    free(global_fx);
    free(global_fy);
    free(global_fz);

    return 0;
}