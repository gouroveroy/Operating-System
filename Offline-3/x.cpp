/*
  This program simulates a covert operation where operatives recreate documents at typewriting stations
  and intelligence staff review a logbook. Operatives are divided into groups, each with a leader.
  Each operative has a random arrival time, uses a station, and the group leader logs the completion
  in a logbook. The program reads the number of operatives, group size, and timing parameters from an
  input file, and writes all events to an output file.

  Key points:
    - Each operative has a unique ID and random arrival time (exponential distribution).
    - Operatives use typewriting stations (limited resources, mutex-protected).
    - Group leaders wait for all group members, then log completion in a logbook (reader-writer lock).
    - Intelligence staff periodically review the logbook (reader-priority synchronization).
    - All actions are handled using pthreads, and output is thread-safe.

  Compilation:
    g++ -pthread Shadows_of_Small_Health.cpp -o Shadows_of_Small_Health.out

  Usage:
    ./Shadows_of_Small_Health.out <input_file> <output_file>

  Input:
    The input file should contain:
    n m
    writing_time walking_time

    Example of input file (input.txt):
    15 5
    10 3

  Output:
    All events (arrivals, completions, logbook reviews) are logged in the output file with timestamps.

  Prepared by: Gourove Roy (2105017), Date: 25 June 2025
*/

#include <chrono>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <unistd.h>
#include <vector>
using namespace std;

// Constants
#define TYPEWRITING_STATIONS_COUNT 4
#define INTELLIGENCE_STAFF_COUNT 2

// Number of operatives, group size, and timing parameters
int n, m, writing_time, walking_time;
int num_groups;

// Mutex lock for output to file for avoiding interleaving
pthread_mutex_t output_lock;

// Timing functions
auto start_time = chrono::high_resolution_clock::now();

// Get the elapsed time in milliseconds since the start of the simulation.
/**
 * The function `get_time` calculates the elapsed time in milliseconds since a specified start time.
 *
 * @return The function `get_time()` returns the elapsed time in milliseconds since the `start_time`
 * variable was set.
 */
long long get_time()
{
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    long long elapsed_time_ms = duration.count();
    return elapsed_time_ms;
}

// uses mutex lock to write output to avoid interleaving
/**
 * The function `write_output` writes the given string to the standard output while ensuring thread
 * safety using a mutex lock.
 *
 * @param output The `output` parameter is a constant reference to a `string` object. This means that
 * the function `write_output` takes a `string` as input, but it does not modify the input string
 * within the function.
 */
void write_output(const string &output)
{
    pthread_mutex_lock(&output_lock);
    cout << output;
    pthread_mutex_unlock(&output_lock);
}

// Function to generate a Poisson-distributed random number
/**
 * The function `get_random_number` generates a random number following a Poisson distribution with a
 * specified lambda value, using a random number generator.
 *
 * @return The function `get_random_number` returns an integer that is a Poisson-distributed random
 * number.
 */
int get_random_number()
{
    random_device rd;
    mt19937 generator(rd());

    // Lambda value for the Poisson distribution
    double lambda = 10000.234;
    poisson_distribution<int> poissonDist(lambda);
    return poissonDist(generator);
}

// Class for Typewriting Station
/* The Station class in C++ provides methods to acquire and release a mutex for thread synchronization. */
class Station
{
public:
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    bool in_use;

    /**
     * The Station constructor initializes a mutex and condition variable using pthread in C++.
     */
    Station() : in_use(false)
    {
        pthread_mutex_init(&mtx, NULL);
        pthread_cond_init(&cv, NULL);
    }

    /**
     * The function `acquire()` locks a mutex using pthread library in C++, waiting if the station is
     * already in use.
     */
    void acquire()
    {
        pthread_mutex_lock(&mtx);
        if (in_use)
        {
            write_output("Station is busy, operative waiting at time " + to_string(get_time()) + "\n");
        }
        while (in_use)
        {
            pthread_cond_wait(&cv, &mtx);
        }
        in_use = true;
        write_output("Station acquired at time " + to_string(get_time()) + "\n");
        pthread_mutex_unlock(&mtx);
    }

    /**
     * The function `release` unlocks a pthread mutex and broadcasts to all waiting operatives that the
     * station is now available.
     */
    void release()
    {
        pthread_mutex_lock(&mtx);
        in_use = false;
        pthread_cond_broadcast(&cv); // Alert all waiting operatives
        write_output("Station released, notifying all waiting operatives at time " + to_string(get_time()) + "\n");
        pthread_mutex_unlock(&mtx);
    }
};

// Class for Group
/* The `Group` class in C++ provides thread synchronization using mutex and condition variables for
coordinating the completion of multiple threads. */
class Group
{
public:
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    int counter;

    /**
     * The Group constructor initializes the counter variable and initializes the mutex and condition
     * variable for thread synchronization.
     */
    Group() : counter(0)
    {
        pthread_mutex_init(&mtx, NULL);
        pthread_cond_init(&cv, NULL);
    }

    /**
     * The function `non_leader_completed` increments a counter and signals a condition variable.
     */
    void non_leader_completed()
    {
        pthread_mutex_lock(&mtx);
        counter++;
        write_output("Group member completed, counter now " + to_string(counter) + " at time " + to_string(get_time()) + "\n");
        pthread_cond_signal(&cv);
        pthread_mutex_unlock(&mtx);
    }

    /**
     * The function `leader_completed_and_wait` increments a counter and waits until the counter
     * reaches a specified value before unlocking a mutex.
     *
     * @param m The parameter `m` in the `leader_completed_and_wait` function represents the total
     * number of threads that need to complete before the leader can proceed.
     */
    void leader_completed_and_wait(int m)
    {
        pthread_mutex_lock(&mtx);
        counter++;
        write_output("Group leader waiting for members, counter now " + to_string(counter) + " at time " + to_string(get_time()) + "\n");
        while (counter < m)
        {
            pthread_cond_wait(&cv, &mtx);
        }
        pthread_mutex_unlock(&mtx);
    }
};

// Class for Logbook (Reader-Writer Lock with Reader Priority)
/* The `Logbook` class in C++ provides synchronization mechanisms for multiple readers and exclusive
writers accessing a shared resource. */
class Logbook
{
public:
    pthread_mutex_t mtx;
    pthread_cond_t reader_cv, writer_cv;
    int reader_count;
    bool writing;
    int completed_operations;
    int waiting_writers; // Track waiting writers

    /**
     * The Logbook constructor initializes variables for tracking reader count, writing status, and
     * completed operations, as well as initializes mutex and condition variables for synchronization.
     */
    Logbook() : reader_count(0), writing(false), completed_operations(0), waiting_writers(0)
    {
        pthread_mutex_init(&mtx, NULL);
        pthread_cond_init(&reader_cv, NULL);
        pthread_cond_init(&writer_cv, NULL);
    }

    /**
     * The function `start_reading` uses mutex and condition variables to allow multiple readers to
     * access a shared resource while preventing writers from writing concurrently.
     */
    void start_reading()
    {
        pthread_mutex_lock(&mtx);
        // Block new readers if writers are waiting (writer progress guarantee)
        while (writing || waiting_writers > 0)
        {
            if (writing)
                write_output("Staff waiting to read logbook (writer active) at time " + to_string(get_time()) + "\n");
            else if (waiting_writers > 0)
                write_output("Staff waiting to read logbook (writer waiting) at time " + to_string(get_time()) + "\n");
            pthread_cond_wait(&reader_cv, &mtx);
        }
        reader_count++;
        write_output("Staff started reading logbook, readers now " + to_string(reader_count) + " at time " + to_string(get_time()) + "\n");
        pthread_mutex_unlock(&mtx);
    }

    /**
     * The function `stop_reading` decreases the reader count and signals the writer condition variable
     * if the reader count becomes zero.
     */
    void stop_reading()
    {
        pthread_mutex_lock(&mtx);
        reader_count--;
        write_output("Staff finished reading logbook, readers now " + to_string(reader_count) + " at time " + to_string(get_time()) + "\n");
        if (reader_count == 0)
        {
            pthread_cond_signal(&writer_cv);
        }
        pthread_mutex_unlock(&mtx);
    }

    /**
     * The function `start_writing` uses mutex and condition variables to ensure exclusive writing
     * access when there are no active readers or writers.
     */
    void start_writing()
    {
        pthread_mutex_lock(&mtx);
        waiting_writers++;
        if (reader_count > 0 || writing)
        {
            write_output("Writer waiting to write logbook (readers/writer active) at time " + to_string(get_time()) + "\n");
        }
        while (reader_count > 0 || writing)
        {
            pthread_cond_wait(&writer_cv, &mtx);
        }
        waiting_writers--;
        writing = true;
        write_output("Writer started writing logbook at time " + to_string(get_time()) + "\n");
        pthread_mutex_unlock(&mtx);
    }

    /**
     * The function `stop_writing` updates a shared variable, signals waiting threads, and releases a
     * mutex lock.
     */
    void stop_writing()
    {
        pthread_mutex_lock(&mtx);
        writing = false;
        completed_operations++;
        write_output("Writer finished writing logbook, completed_operations now " + to_string(completed_operations) + " at time " + to_string(get_time()) + "\n");
        // Give priority to waiting writers
        if (waiting_writers > 0)
            pthread_cond_signal(&writer_cv);
        else
            pthread_cond_broadcast(&reader_cv);
        pthread_mutex_unlock(&mtx);
    }

    /**
     * This function returns the number of completed operations.
     *
     * @return The function `get_completed_operations` is returning the value of the variable
     * `completed_operations`.
     */
    int get_completed_operations() const
    {
        return completed_operations;
    }
};

vector<Station> stations;
vector<Group> groups;
Logbook logbook;

/**
 * The struct `StaffArgs` contains an integer `id`.
 * @property {int} id - The `id` property in the `StaffArgs` struct represents the unique identifier
 * for a staff member.
 */
struct StaffArgs
{
    int id;
};

/**
 * The struct OperativeArgs contains an integer id.
 * @property {int} id - The `id` property in the `OperativeArgs` struct represents the unique
 * identifier for an operative.
 */
struct OperativeArgs
{
    int id;
};

/**
 * The function `staff_thread` simulates a staff member reviewing a logbook with a specified lambda
 * value for exponential distribution.
 *
 * @param arg The `arg` parameter in the `staff_thread` function is a void pointer that is later cast
 * to a `StaffArgs` pointer. The `StaffArgs` structure likely contains information needed by the thread
 * to perform its tasks, such as the staff ID and the lambda value for the exponential distribution
 * used
 *
 * @return The `staff_thread` function is returning a `NULL` pointer.
 */
void *staff_thread(void *arg)
{
    StaffArgs *args = (StaffArgs *)arg;
    int id = args->id;
    for (int i = 0; i < 50; i++)
    {
        usleep((get_random_number() % 100 + 1) * 1000);
        write_output("Intelligence Staff " + to_string(id) + " attempting to read logbook at time " + to_string(get_time()) + "\n");
        logbook.start_reading();
        write_output("Intelligence Staff " + to_string(id) + " began reviewing logbook at time " + to_string(get_time()) + ". Operations completed = " + to_string(logbook.get_completed_operations()) + "\n");
        logbook.stop_reading();
        write_output("Intelligence Staff " + to_string(id) + " finished reviewing logbook at time " + to_string(get_time()) + "\n");
    }
    return NULL;
}

/**
 * The function `operative_thread` simulates operatives arriving at a typewriting station, completing
 * document recreation, and handling intelligence distribution tasks within a group-based system.
 *
 * @param arg The `arg` parameter in the `operative_thread` function is a void pointer that is cast to
 * a pointer of type `OperativeArgs`. It contains the arguments passed to the thread function,
 * specifically the `id` and `lambda` values for the operative.
 *
 * @return The `operative_thread` function is returning a `NULL` pointer.
 */
void *operative_thread(void *arg)
{
    OperativeArgs *args = (OperativeArgs *)arg;
    int id = args->id;
    int station_index = id % TYPEWRITING_STATIONS_COUNT;
    int group_index = (id - 1) / m;
    int leader_id = group_index * m + m;

    usleep((get_random_number() % 100 + 1) * 1000);
    write_output("Operative " + to_string(id) + " has arrived at typewriting station at time " + to_string(get_time()) + "\n");

    write_output("Operative " + to_string(id) + " attempting to acquire station " + to_string(station_index + 1) + " at time " + to_string(get_time()) + "\n");
    stations[station_index].acquire();
    write_output("Operative " + to_string(id) + " acquired station " + to_string(station_index + 1) + " at time " + to_string(get_time()) + "\n");
    usleep(writing_time * 1000);
    write_output("Operative " + to_string(id) + " has completed document recreation at time " + to_string(get_time()) + "\n");
    stations[station_index].release();
    write_output("Operative " + to_string(id) + " released station " + to_string(station_index + 1) + " at time " + to_string(get_time()) + "\n");

    if (id == leader_id)
    {
        write_output("Group leader (Operative " + to_string(id) + ") waiting for group completion at time " + to_string(get_time()) + "\n");
        groups[group_index].leader_completed_and_wait(m);
        write_output("Unit " + to_string(group_index + 1) + " has completed document recreation phase at time " + to_string(get_time()) + "\n");
    }
    else
    {
        groups[group_index].non_leader_completed();
    }

    if (id == leader_id)
    {
        write_output("Group leader (Operative " + to_string(id) + ") attempting to write logbook at time " + to_string(get_time()) + "\n");
        logbook.start_writing();
        usleep(walking_time * 1000);
        write_output("Unit " + to_string(group_index + 1) + " has completed intelligence distribution at time " + to_string(get_time()) + "\n");
        logbook.stop_writing();
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "Usage: ./<source_file_name_without_extension>.out <input_file> <output_file>" << endl;
        return 0;
    }

    ifstream inputFile(argv[1]);
    streambuf *cinBuffer = cin.rdbuf();
    cin.rdbuf(inputFile.rdbuf());

    ofstream outputFile(argv[2]);
    streambuf *coutBuffer = cout.rdbuf();
    cout.rdbuf(outputFile.rdbuf());

    cin >> n >> m >> writing_time >> walking_time;
    num_groups = n / m;

    stations.resize(TYPEWRITING_STATIONS_COUNT);
    groups.resize(num_groups);

    pthread_mutex_init(&output_lock, NULL);

    pthread_t staff_threads[INTELLIGENCE_STAFF_COUNT];
    pthread_t operative_threads[n];
    StaffArgs staff_args[INTELLIGENCE_STAFF_COUNT];
    OperativeArgs operative_args[n];

    for (int i = 0; i < INTELLIGENCE_STAFF_COUNT; i++)
    {
        staff_args[i] = {i + 1};
        pthread_create(&staff_threads[i], NULL, staff_thread, &staff_args[i]);
    }

    for (int i = 0; i < n; i++)
    {
        operative_args[i] = {i + 1};
        pthread_create(&operative_threads[i], NULL, operative_thread, &operative_args[i]);
    }

    for (int i = 0; i < INTELLIGENCE_STAFF_COUNT; i++)
    {
        pthread_join(staff_threads[i], NULL);
    }

    for (int i = 0; i < n; i++)
    {
        pthread_join(operative_threads[i], NULL);
    }

    cin.rdbuf(cinBuffer);
    cout.rdbuf(coutBuffer);

    return 0;
}
