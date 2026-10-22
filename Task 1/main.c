#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h> // ловит ошибки
#include <time.h>
#include <pthread.h>   // работа с потоками
#include <stdatomic.h> //  атомарные операции

#include "timer.h" // замер скорости работы алгоритма

#define MAX_ITER 1000 // задаем макросами нашу область поиска и максимально количество итераций
#define X_MIN (-2.0)
#define X_MAX (0.5)
#define Y_MIN (-1.25)
#define Y_MAX (1.25)

typedef struct
{ // структура числа c
    double x;
    double y;
} Point;

typedef struct
{ // передаем номер потока и зерно каждому рабочему потоку
    size_t rank;
    unsigned int seed;
} ThreadArg;

static Point *points = NULL;    // массив для найденых точек
static size_t points_found = 0; // счетчики найденных и ненайденных точек
static size_t points_needed = 0;

static pthread_mutex_t points_mutex; // мьютекс для аккуратненькой записи в массив
static atomic_bool finished = false; // когда нужное количество найденных точек достигнуто

/* Проверяет, принадлежит ли точка множеству*/
static bool is_mandelbrot(double x, double y)
{
    double zx = 0.0;
    double zy = 0.0;

    for (int i = 0; i < MAX_ITER; i++)
    {
        double zx2 = zx * zx - zy * zy + x;
        double zy2 = 2.0 * zx * zy + y;

        zx = zx2;
        zy = zy2;

        if (zx * zx + zy * zy > 4.0)
        {
            return false;
        }
    }

    return true;
}

/* Генерирует случайное число в диапазоне [0, 1).
 Испольузем rand_r() и уникальное зерно, чтобы у каждого потока был свой генератор, не повторяющийся с другими потоками
*/
static double random_unit(unsigned int *seed)
{
    return (double)rand_r(seed) / ((double)RAND_MAX + 1.0);
}

/* Функция, выполняемая каждым потоком.*/
static void *worker(void *arg)
{
    ThreadArg *data = (ThreadArg *)arg; // получаем аргументы функции
    unsigned int seed = data->seed;

    while (!atomic_load(&finished))
    {
        double x = X_MIN + (X_MAX - X_MIN) * random_unit(&seed);
        double y = Y_MIN + (Y_MAX - Y_MIN) * random_unit(&seed);

        if (!is_mandelbrot(x, y))
        { // пока не достигнуто нужное количество точек, генерируем случайные точки и проверяем их
            continue;
        }

        pthread_mutex_lock(&points_mutex);

        if (points_found < points_needed)
        { // если точка подошла, при помощи мьютекса записываем точки в массив
            points[points_found].x = x;
            points[points_found].y = y;
            points_found++;

            if (points_found == points_needed)
            {
                atomic_store(&finished, true);
            }
        }

        pthread_mutex_unlock(&points_mutex);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr,
                "Write: %s nthreads npoints\n", // проверяем, что на вход получено 3 аргумента
                argv[0]);
        return 1;
    }

    char *endptr1; // для проверки введеных значений указатели на символы
    char *endptr2;

    errno = 0;
    long nthreads_input = strtol(argv[1], &endptr1, 10);

    if (errno != 0 || *endptr1 != '\0' ||
        nthreads_input <= 0 || nthreads_input > INT_MAX)
    { // проверка числа потоков
        fprintf(stderr, "Invalid number of threads\n");
        return 1;
    }

    errno = 0;
    long npoints_input = strtol(argv[2], &endptr2, 10);

    if (errno != 0 || *endptr2 != '\0' || // проверка числа точек
        npoints_input <= 0)
    {
        fprintf(stderr, "Invalid number of points\n");
        return 1;
    }

    size_t nthreads = (size_t)nthreads_input; // записываем число потоков и число точек
    points_needed = (size_t)npoints_input;

    if (nthreads > SIZE_MAX / sizeof(pthread_t) ||
        nthreads > SIZE_MAX / sizeof(ThreadArg) ||
        points_needed > SIZE_MAX / sizeof(Point))
    {
        fprintf(stderr, "Requested sizes are too large\n"); // проверка, для последующего выделения памяти для массива
        return 1;
    }

    points = malloc(points_needed * sizeof(*points));
    pthread_t *threads = malloc(nthreads * sizeof(*threads)); // выделяем память для массива точек, массива id потоков и массива аргументов
    ThreadArg *args = malloc(nthreads * sizeof(*args));

    if (points == NULL || threads == NULL || args == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n"); // обработка ошибки при выделении памяти
        free(points);
        free(threads);
        free(args);
        return 1;
    }

    if (pthread_mutex_init(&points_mutex, NULL) != 0)
    { // обработка ошибки при создании мьютекса
        fprintf(stderr, "Mutex initialization failed\n");
        free(points);
        free(threads);
        free(args);
        return 1;
    }

    unsigned int base_seed = (unsigned int)time(NULL); // начинаем замерять время

    size_t created = 0;
    int create_error = 0;

    double start, end;
    GET_TIME(start);

    for (size_t i = 0; i < nthreads; i++)
    {
        args[i].rank = i;
        args[i].seed = base_seed + (unsigned int)(i * 67u) // каждому потоку даем айди и зерно
                       + 1u;

        int err = pthread_create( // создаем потоки
            &threads[i],
            NULL,
            worker,
            &args[i]);

        if (err != 0)
        {
            fprintf(stderr,
                    "pthread_create failed: %d\n", // обрабатываем ошибки при создании потока
                    err);
            create_error = err;
            atomic_store(&finished, true);
            break;
        }

        created++;
    }

    for (size_t i = 0; i < created; i++)
    {
        int err = pthread_join(threads[i], NULL);

        if (err != 0)
        {
            fprintf(stderr, "pthread_join failed: %d\n", err); // ждем завершение всех потоков
            create_error = err;
        }
    }

    GET_TIME(end); // заканчиваем считать время

    if (create_error != 0 || points_found != points_needed)
    {
        fprintf(stderr, "Point generation did not complete\n"); // обработка ошибки, если нужное количество точек не создано
        pthread_mutex_destroy(&points_mutex);
        free(points);
        free(threads);
        free(args);
        return 1;
    }

    FILE *file = fopen("mandelbrot.csv", "w"); // открываем файл

    if (file == NULL)
    {
        perror("Cannot open mandelbrot.csv");
        pthread_mutex_destroy(&points_mutex); // обработка ошибки открытия файла
        free(points);
        free(threads);
        free(args);
        return 1;
    }

    fprintf(file, "x,y\n");

    for (size_t i = 0; i < points_found; i++)
    { // записываем найденные точки в файл
        fprintf(file, "%.10f,%.10f\n",
                points[i].x, points[i].y);
    }

    int write_error = ferror(file);
    if (fclose(file) != 0)
    {
        write_error = 1;
    }

    if (write_error)
    {
        fprintf(stderr, "Error writing CSV file\n"); // обработка ошибки записи в файл
        pthread_mutex_destroy(&points_mutex);
        free(points);
        free(threads);
        free(args);
        return 1;
    }

    printf("Threads: %zu\n", nthreads);
    printf("Points found: %zu\n", points_found);
    printf("Calculation time: %.6f seconds\n", end - start);
    printf("Output file: mandelbrot.csv\n");

    pthread_mutex_destroy(&points_mutex);

    free(points);
    free(threads); // освобождаем память
    free(args);

    return 0;
}
