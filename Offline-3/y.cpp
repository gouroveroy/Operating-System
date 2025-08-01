/*
  This program simulates an intelligence operation where operatives recreate documents at typewriting stations
  and group leaders log completed operations in a master logbook. It uses POSIX threads with mutexes and semaphores
  for synchronization, ensuring no busy waiting and proper coordination.

  Key points:
    - N operatives are divided into groups of M, with leaders having the highest ID in each group.
    - 4 typewriting stations are available, assigned by ID % 4 + 1, with operatives waiting if occupied.
    - Leaders wait for all group members to finish document recreation before logging in the master logbook.
    - Two staff members periodically read the logbook, with readers having higher priority over writers.
    - Random delays use Poisson distribution, and timing is simulated with sleep functions.

  Compilation:
    g++ -pthread student_report_printing.cpp -o a.out

  Usage:
    ./a.out <input_file> <output_file>

  Input:
    N M
    x y
    Example: 15 5\n10 3

  Output:
    Logs operative actions, group completions, and staff reviews with timestamps.

  Modified by: [Your Name], Date: [Current Date]
*/

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <semaphore.h>
#include <unistd.h>
#include <vector>

using namespace std;

// Constants
#define NUM_STATIONS 4
#define MAX_DELAY 2           // Maximum initial delay in seconds
#define TIME_UNIT 100         // Time unit in milliseconds
#define SLEEP_MULTIPLIER 1000 // Convert milliseconds to microseconds

int N, M, x, y;               // Input variables: operatives, group size, document recreation time, logbook entry time
int operations_completed = 0; // Shared variable for completed operations

// Synchronization primitives
pthread_mutex_t output_lock;                // Mutex for output synchronization
pthread_mutex_t reader_mutex;               // Mutex for reader count in logbook access
sem_t writer_sem;                           // Semaphore for writer exclusion in logbook
int reader_count = 0;                       // Number of active readers
sem_t station_sem[NUM_STATIONS];            // Semaphores for typewriting stations
vector<sem_t> group_sem;                    // Semaphores for group synchronization
std::atomic<bool> staff_cancel_flag(false); // Atomic flag to signal staff threads to cancel

// Timing functions
auto start_time = chrono::high_resolution_clock::now();

/**
 * Get elapsed time in milliseconds since simulation start.
 * @return Elapsed time in milliseconds.
 */
long long get_time()
{
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    long long elapsed_time_ms = duration.count();
    return elapsed_time_ms;
}

/**
 * Generate a Poisson-distributed random number for delays.
 * @return Random integer based on Poisson distribution with lambda=5.
 */
int get_random_number()
{
    std::random_device rd;
    std::mt19937 generator(rd());

    // Lambda value for the Poisson distribution
    double lambda = 5.0;
    std::poisson_distribution<int> poissonDist(lambda);
    return poissonDist(generator);
}

/**
 * Struct representing an operative.
 */
struct Operative
{
    int id;         // Unique ID (1 to N)
    int group_id;   // Group number (1 to N/M)
    bool is_leader; // True if operative is the group leader
};

/**
 * Write output with mutex lock to prevent interleaving.
 * @param output String to write.
 */
void write_output(const string &output)
{
    pthread_mutex_lock(&output_lock);
    cout << output;
    pthread_mutex_unlock(&output_lock);
}

/**
 * Thread function for operatives.
 * @param arg Pointer to Operative struct.
 */
void *operative_function(void *arg)
{
    Operative *op = (Operative *)arg;

    // Random initial delay
    int delay = get_random_number() % MAX_DELAY + 1;
    usleep(delay * SLEEP_MULTIPLIER * 1000);

    // Document Recreation Phase
    int station = (op->id % 4) + 1;
    write_output("Operative " + to_string(op->id) + " has arrived at typewriting station " +
                 to_string(station) + " at time " + to_string(get_time()) + "\n");
    sem_wait(&station_sem[station - 1]);
    usleep(x * TIME_UNIT * SLEEP_MULTIPLIER); // Simulate document recreation
    write_output("Operative " + to_string(op->id) + " has completed document recreation at time " +
                 to_string(get_time()) + "\n");
    sem_post(&station_sem[station - 1]);

    // Signal group completion
    sem_post(&group_sem[op->group_id - 1]);

    // Leader handles Logbook Entry Phase
    if (op->is_leader)
    {
        // Wait for all M members to finish
        for (int i = 0; i < M; i++)
        {
            sem_wait(&group_sem[op->group_id - 1]);
        }
        write_output("Unit " + to_string(op->group_id) + " has completed document recreation phase at time " +
                     to_string(get_time()) + "\n");

        // Logbook entry with writer access
        sem_wait(&writer_sem);
        usleep(y * TIME_UNIT * SLEEP_MULTIPLIER); // Simulate logbook entry
        operations_completed++;
        write_output("Unit " + to_string(op->group_id) + " has completed intelligence distribution at time " +
                     to_string(get_time()) + "\n");
        sem_post(&writer_sem);
    }

    return NULL;
}

/**
 * Thread function for intelligence staff.
 * @param arg Pointer to staff ID.
 */
void *staff_function(void *arg)
{
    int staff_id = *(int *)arg;
    while (true)
    {
        int sleep_time = get_random_number() % 10 + 1; // Random interval 1-10 seconds
        usleep(sleep_time * SLEEP_MULTIPLIER * 1000);

        // Reader access to logbook
        pthread_mutex_lock(&reader_mutex);
        reader_count++;
        if (reader_count == 1)
        {
            sem_wait(&writer_sem); // First reader blocks writers
        }
        pthread_mutex_unlock(&reader_mutex);

        // Read logbook
        int ops = operations_completed;
        write_output("Intelligence Staff " + to_string(staff_id) + " began reviewing logbook at time " +
                     to_string(get_time()) + ". Operations completed = " + to_string(ops) + "\n");

        // Release reader access
        pthread_mutex_lock(&reader_mutex);
        reader_count--;
        if (reader_count == 0)
        {
            sem_post(&writer_sem); // Last reader unblocks writers
        }
        pthread_mutex_unlock(&reader_mutex);

        if (staff_cancel_flag)
            break; // Exit loop if cancel flag is set
    }
    return NULL;
}

/**
 * Main function to initialize and run the simulation.
 */
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "Usage: ./a.out <input_file> <output_file>" << endl;
        return 0;
    }

    // Redirect input/output
    ifstream inputFile(argv[1]);
    streambuf *cinBuffer = cin.rdbuf();
    cin.rdbuf(inputFile.rdbuf());
    ofstream outputFile(argv[2]);
    streambuf *coutBuffer = cout.rdbuf();
    cout.rdbuf(outputFile.rdbuf());

    // Read input
    cin >> N >> M >> x >> y;

    // Initialize synchronization primitives
    pthread_mutex_init(&output_lock, NULL);
    pthread_mutex_init(&reader_mutex, NULL);
    sem_init(&writer_sem, 0, 1);
    for (int i = 0; i < NUM_STATIONS; i++)
    {
        sem_init(&station_sem[i], 0, 1);
    }
    int num_groups = N / M;
    group_sem.resize(num_groups);
    for (int i = 0; i < num_groups; i++)
    {
        sem_init(&group_sem[i], 0, 0);
    }

    // Create operatives
    vector<Operative> operatives;
    for (int i = 1; i <= N; i++)
    {
        int group_id = ((i - 1) / M) + 1;
        bool is_leader = (i % M == 0);
        operatives.push_back({i, group_id, is_leader});
    }

    // Create threads
    pthread_t operative_threads[N];
    pthread_t staff_threads[2];
    int staff_ids[2] = {1, 2};

    // Start staff threads
    for (int i = 0; i < 2; i++)
    {
        pthread_create(&staff_threads[i], NULL, staff_function, &staff_ids[i]);
    }

    // Start operative threads
    for (int i = 0; i < N; i++)
    {
        pthread_create(&operative_threads[i], NULL, operative_function, &operatives[i]);
    }

    // Wait for operative threads to finish
    for (int i = 0; i < N; i++)
    {
        pthread_join(operative_threads[i], NULL);
    }

    // Wait until all units have completed intelligence distribution
    while (operations_completed < num_groups)
    {
        usleep(100 * 1000); // Sleep 100ms to avoid busy waiting
    }

    staff_cancel_flag = true;
    for (int i = 0; i < 2; i++)
    {
        pthread_join(staff_threads[i], NULL);
    }

    // Clean up
    pthread_mutex_destroy(&output_lock);
    pthread_mutex_destroy(&reader_mutex);
    sem_destroy(&writer_sem);
    for (int i = 0; i < NUM_STATIONS; i++)
    {
        sem_destroy(&station_sem[i]);
    }
    for (int i = 0; i < num_groups; i++)
    {
        sem_destroy(&group_sem[i]);
    }

    // Restore cin and cout
    cin.rdbuf(cinBuffer);
    cout.rdbuf(coutBuffer);

    return 0;
}
