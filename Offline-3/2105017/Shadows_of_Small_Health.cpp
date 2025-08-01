/*
  This program simulates an intelligence operation with operatives recreating documents at typewriting stations
  and group leaders logging completed operations in a master logbook. It uses POSIX threads, mutexes, and semaphores
  for synchronization, ensuring no busy waiting and proper coordination.

  Key points:
    - N operatives are divided into groups of M, with leaders having the highest ID in each group.
    - 4 typewriting stations are available, assigned by ID % 4 + 1, with operatives waiting if occupied.
    - Leaders wait for all group members to finish document recreation before logging in the master logbook.
    - Two staff members periodically read the logbook, with readers having higher priority over writers.
    - Random delays use Poisson distribution, and timing is simulated with sleep functions.
    - All actions and synchronization events are printed for easy evaluation.

  Compilation:
    g++ -pthread Shadows_of_Small_Health.cpp -o a.out

  Usage:
    ./a.out <input_file> <output_file>

  Input:
    N M
    x y
    (N: operatives, M: group size,
    x: doc time, y: log time)
    Example:
        15 5
        10 3

  Output:
    Logs operative actions, group completions, and staff reviews with timestamps.

  Prepared by: Gourove Roy, Date: 27 June 2025
*/
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <semaphore.h>
#include <chrono>
#include <random>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace std;

int N, M, x, y;
int G;

pthread_mutex_t station_mutex[4];
pthread_cond_t station_cv[4];
bool station_available[4] = {true, true, true, true};

int *group_counter;
pthread_mutex_t *group_mutex;
pthread_cond_t *group_cv;

int completed_operations = 0;
int read_count = 0;
sem_t wrt;   // Semaphore for writers (binary semaphore)
sem_t mutex; // Semaphore for read_count protection

bool simulation_running = true;

pthread_mutex_t output_mutex;
auto start_time = chrono::high_resolution_clock::now();

long long get_time()
{
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    long long elapsed_time_ms = duration.count();
    return elapsed_time_ms;
}

void write_output(string message)
{
    pthread_mutex_lock(&output_mutex);
    cout << message << endl;
    pthread_mutex_unlock(&output_mutex);
}

int get_random_number()
{
    random_device rd;
    mt19937 generator(rd());

    // Lambda value for the Poisson distribution
    double lambda = 10000.234;
    poisson_distribution<int> poissonDist(lambda);
    return poissonDist(generator);
}

void *operative_function(void *arg)
{
    long id = (long)arg;
    int delay_arrival = get_random_number() % (x + 2) + 1;
    usleep(delay_arrival * 5000);
    int station_id = (id % 4) + 1;
    int station_index = station_id - 1;
    write_output("Operative " + to_string(id) + " has arrived at typewriting station " + to_string(station_id) + " at time " + to_string(get_time()));
    write_output("Operative " + to_string(id) + " is requesting station " + to_string(station_id) + ".");

    pthread_mutex_lock(&station_mutex[station_index]);
    while (!station_available[station_index])
    {
        write_output("Operative " + to_string(id) + " is waiting for station " + to_string(station_id) + ".");
        pthread_cond_wait(&station_cv[station_index], &station_mutex[station_index]);
    }
    station_available[station_index] = false;
    pthread_mutex_unlock(&station_mutex[station_index]);
    write_output("Operative " + to_string(id) + " has acquired station " + to_string(station_id) + ".");

    int typewriting_time = get_random_number() % (y + 2) + 1;
    usleep(typewriting_time * 5000);
    write_output("Operative " + to_string(id) + " has completed document recreation at station " + to_string(station_id) + " at time " + to_string(get_time()));

    pthread_mutex_lock(&station_mutex[station_index]);
    station_available[station_index] = true;
    pthread_cond_broadcast(&station_cv[station_index]);
    pthread_mutex_unlock(&station_mutex[station_index]);
    write_output("Operative " + to_string(id) + " has released station " + to_string(station_id) + ".");

    int group_id = (id - 1) / M;
    int leader_id = (group_id + 1) * M;

    if (id == leader_id)
    {
        write_output("Leader Operative " + to_string(id) + " is waiting for group members to finish.");
        pthread_mutex_lock(&group_mutex[group_id]);
        group_counter[group_id]++;
        if (group_counter[group_id] < M)
        {
            while (group_counter[group_id] < M)
            {
                pthread_cond_wait(&group_cv[group_id], &group_mutex[group_id]);
            }
        }
        pthread_mutex_unlock(&group_mutex[group_id]);
        write_output("Leader Operative " + to_string(id) + " detected all group members finished.");

        write_output("Unit " + to_string(group_id + 1) + " has completed document recreation phase at time " + to_string(get_time()));

        // Writer entry protocol
        sem_wait(&wrt);
        int writing_time = get_random_number() % (y + 2) + 1;
        usleep(writing_time * 5000);
        completed_operations++;
        write_output("Unit " + to_string(group_id + 1) + " has completed intelligence distribution at time " + to_string(get_time()));
        sem_post(&wrt);
    }
    else
    {
        pthread_mutex_lock(&group_mutex[group_id]);
        group_counter[group_id]++;
        write_output("Operative " + to_string(id) + " has finished and notified group leader.");
        if (group_counter[group_id] == M)
        {
            pthread_cond_broadcast(&group_cv[group_id]);
        }
        pthread_mutex_unlock(&group_mutex[group_id]);
    }

    return NULL;
}

void *staff_function(void *arg)
{
    long staff_id = (long)arg;
    while (simulation_running)
    {
        int sleep_interval = get_random_number() % (y + 2) + 1;
        usleep(sleep_interval * 5000);
        if (!simulation_running)
            break;

        // Reader entry protocol
        sem_wait(&mutex);
        read_count++;
        if (read_count == 1)
        {
            sem_wait(&wrt);
        }
        sem_post(&mutex);

        int current_completed = completed_operations;
        write_output("Intelligence Staff " + to_string(staff_id) + " began reviewing logbook at time " + to_string(get_time()) + ". Operations completed = " + to_string(current_completed));

        // Reader exit protocol
        sem_wait(&mutex);
        read_count--;
        if (read_count == 0)
        {
            sem_post(&wrt);
        }
        sem_post(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "Usage: ./a.out <input_file> <output_file>" << endl;
        return 0;
    }

    ifstream inputFile(argv[1]);
    ofstream outputFile(argv[2]);
    streambuf *cinBuffer = cin.rdbuf();
    streambuf *coutBuffer = cout.rdbuf();
    cin.rdbuf(inputFile.rdbuf());

    // Check if output file exists, otherwise create it
    ifstream testOutput(argv[2]);
    if (!testOutput.good())
    {
        ofstream createFile(argv[2]);
        createFile.close();
    }
    testOutput.close();

    cout.rdbuf(outputFile.rdbuf());

    cin >> N >> M >> x >> y;
    G = N / M;

    start_time = chrono::high_resolution_clock::now();

    // Initialize semaphores
    sem_init(&wrt, 0, 1);   // Binary semaphore for writers
    sem_init(&mutex, 0, 1); // Binary semaphore for read_count

    for (int i = 0; i < 4; i++)
    {
        pthread_mutex_init(&station_mutex[i], NULL);
        pthread_cond_init(&station_cv[i], NULL);
    }

    group_counter = new int[G]();
    group_mutex = new pthread_mutex_t[G];
    group_cv = new pthread_cond_t[G];
    for (int i = 0; i < G; i++)
    {
        pthread_mutex_init(&group_mutex[i], NULL);
        pthread_cond_init(&group_cv[i], NULL);
    }

    pthread_mutex_init(&output_mutex, NULL);

    pthread_t op_threads[N];
    for (long i = 0; i < N; i++)
    {
        pthread_create(&op_threads[i], NULL, operative_function, (void *)(i + 1));
    }

    pthread_t staff_threads[2];
    for (long i = 0; i < 2; i++)
    {
        pthread_create(&staff_threads[i], NULL, staff_function, (void *)(i + 1));
    }

    for (int i = 0; i < N; i++)
    {
        pthread_join(op_threads[i], NULL);
    }

    simulation_running = false;

    for (int i = 0; i < 2; i++)
    {
        pthread_join(staff_threads[i], NULL);
    }

    for (int i = 0; i < 4; i++)
    {
        pthread_mutex_destroy(&station_mutex[i]);
        pthread_cond_destroy(&station_cv[i]);
    }

    for (int i = 0; i < G; i++)
    {
        pthread_mutex_destroy(&group_mutex[i]);
        pthread_cond_destroy(&group_cv[i]);
    }

    sem_destroy(&wrt);
    sem_destroy(&mutex);
    pthread_mutex_destroy(&output_mutex);

    delete[] group_counter;
    delete[] group_mutex;
    delete[] group_cv;

    cin.rdbuf(cinBuffer);
    cout.rdbuf(coutBuffer);

    return 0;
}
