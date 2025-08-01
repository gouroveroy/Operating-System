#include <bits/stdc++.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
using namespace std;

#define NUM_STATIONS 4
#define SLEEP_MULTIPLIER 800

int N; // Number of operatives
int M; // Unit size
int x; // Relative time for document recreation (ms)
int y; // Relative time for logbook entry (ms)

int active_readers = 0;
int operations_completed = 0;
int waiting_readers = 0;
bool is_writer_active = false;
sem_t station_sems[NUM_STATIONS]; // Sem for TS
vector<sem_t> group_sems;         // Sem for group
// sem_t reader_sem; // Sem for reader
// sem_t writer_sem; // Sem for writer
pthread_mutex_t logbook_mutex; // Mutex for logbook
pthread_mutex_t output_mutex;  // Mutex for output
pthread_cond_t cond;           // pthread cond
pthread_mutex_t cond_mutex;    // mutex cv
auto start_time = chrono::high_resolution_clock::now();
long long get_time()
{
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(
        end_time - start_time);
    long long elapsed_time_ms = duration.count();
    return elapsed_time_ms;
}
int get_random_number()
{
    // Creates a random device for non-deterministic random number generation
    random_device rd;
    // Initializes a random number generator using the random device
    mt19937 generator(rd());
    // Lambda value for the Poisson distribution
    double lambda = 100.234;
    // Defines a Poisson distribution with the given lambda
    poisson_distribution<int> poissonDist(lambda);
    // Generates and returns a random number based on the Poisson distribution
    return poissonDist(generator);
}
void write_output(const string &msg)
{
    pthread_mutex_lock(&output_mutex);
    cout << msg;
    pthread_mutex_unlock(&output_mutex);
}

class Operative
{
public:
    int id;
    int group_id;
    bool is_leader;
    Operative(int id, int m)
    {
        this->id = id;
        group_id = (id - 1) / m;
        is_leader = id % m == 0;
    }
};
void *intelligence_reader(void *arg)
{
    int staff_id = *(int *)arg;
    while (operations_completed < N / M)
    {
        int delay = get_random_number();
        usleep(delay * 100);
        pthread_mutex_lock(&logbook_mutex);
        waiting_readers++;
        while (is_writer_active)
        {
            pthread_mutex_unlock(&logbook_mutex);
            pthread_mutex_lock(&cond_mutex);
            pthread_cond_wait(&cond, &cond_mutex);
            pthread_mutex_unlock(&cond_mutex);
            pthread_mutex_lock(&logbook_mutex);
        }
        waiting_readers--;
        active_readers++;
        pthread_mutex_unlock(&logbook_mutex);
        int ops;
        pthread_mutex_lock(&logbook_mutex);
        ops = operations_completed;
        pthread_mutex_unlock(&logbook_mutex);
        usleep(get_random_number() * 100);
        write_output("Intelligence Staff " + to_string(staff_id) +
                     " began reviewing logbook at time " + to_string(get_time()) +
                     " ms. Operations completed = " + to_string(ops) + "\n");
        pthread_mutex_lock(&logbook_mutex);
        active_readers--;
        if (active_readers == 0)
        {
            pthread_cond_broadcast(&cond); // Notify all
        }
        pthread_mutex_unlock(&logbook_mutex);
        // Exit read
        usleep(get_random_number() * 100);
    }
    return nullptr;
}
void *operative_worker(void *arg)
{
    Operative *op = (Operative *)arg;
    int station_id = (op->id % NUM_STATIONS);
    int delay = get_random_number();
    usleep(delay * SLEEP_MULTIPLIER);
    write_output("Operative " + to_string(op->id) +
                 " has arrived at typewriting station at time " +
                 to_string(get_time()) + " ms\n");
    // Access TS
    sem_wait(&station_sems[station_id]);
    write_output("Operative " + to_string(op->id) +
                 " started document recreation at time " +
                 to_string(get_time()) + " ms\n");
    usleep(x * SLEEP_MULTIPLIER); // x ms
    write_output("Operative " + to_string(op->id) +
                 " has completed document recreation at time " +
                 to_string(get_time()) + " ms\n");
    sem_post(&station_sems[station_id]);
    sem_post(&group_sems[op->group_id]);
    if (op->is_leader)
    {
        for (int i = 0; i < M; i++)
        {
            sem_wait(&group_sems[op->group_id]);
        }
        write_output("Unit " + to_string(op->group_id + 1) +
                     " has completed document recreation phase at time " +
                     to_string(get_time()) + " ms\n");
        pthread_mutex_lock(&logbook_mutex);
        while (active_readers > 0 || waiting_readers > 0 || is_writer_active)
        {
            pthread_mutex_unlock(&logbook_mutex);
            pthread_mutex_lock(&cond_mutex);
            pthread_cond_wait(&cond, &cond_mutex);
            pthread_mutex_unlock(&cond_mutex);
            pthread_mutex_lock(&logbook_mutex);
        }
        is_writer_active = true;
        pthread_mutex_unlock(&logbook_mutex);
        usleep(y * SLEEP_MULTIPLIER); // y ms
        pthread_mutex_lock(&logbook_mutex);
        operations_completed++;
        write_output("Unit " + to_string(op->group_id + 1) +
                     " has completed intelligence distribution at time " +
                     to_string(get_time()) + " ms\n");
        is_writer_active = false;
        pthread_cond_broadcast(&cond); // notify all
        pthread_mutex_unlock(&logbook_mutex);
    }
    usleep(get_random_number() * SLEEP_MULTIPLIER);
    return nullptr;
}
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "Usage: ./a.out <input_file> <output_file>" << endl;
        return 0;
    }
    // File handling for input and output redirection
    ifstream inputFile(argv[1]);
    streambuf *cinBuffer = cin.rdbuf(); // Save original cin buffer
    cin.rdbuf(inputFile.rdbuf());       // Redirect cin to input file
    ofstream outputFile(argv[2]);
    streambuf *coutBuffer = cout.rdbuf(); // Save original cout buffer
    cout.rdbuf(outputFile.rdbuf());       // Redirect cout to output file
    cin >> N >> M >> x >> y;
    for (int i = 0; i < NUM_STATIONS; i++)
    {
        sem_init(&station_sems[i], 0, 1);
    }
    group_sems.resize(N / M);
    for (int i = 0; i < group_sems.size(); i++)
    {
        sem_init(&group_sems[i], 0, 0);
    }
    pthread_mutex_init(&logbook_mutex, nullptr);
    pthread_mutex_init(&output_mutex, nullptr);
    pthread_cond_init(&cond, nullptr);
    pthread_mutex_init(&cond_mutex, nullptr);
    start_time = chrono::high_resolution_clock::now();
    vector<Operative> operatives;
    for (int i = 1; i <= N; i++)
    {
        operatives.emplace_back(i, M);
    }
    pthread_t operative_threads[N];
    pthread_t staff_threads[2];
    int staff_ids[2] = {1, 2};
    for (int i = 0; i < 2; i++)
    {
        pthread_create(&staff_threads[i], nullptr, intelligence_reader, &staff_ids[i]);
    }
    for (int i = 0; i < N; i++)
    {
        pthread_create(&operative_threads[i], nullptr, operative_worker, &operatives[i]);
    }
    for (int i = 0; i < N; i++)
    {
        pthread_join(operative_threads[i], nullptr);
    }
    for (int i = 0; i < 2; i++)
    {
        pthread_join(staff_threads[i], nullptr);
    }
    pthread_mutex_destroy(&logbook_mutex);
    pthread_mutex_destroy(&output_mutex);
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&cond_mutex);
    for (int i = 0; i < NUM_STATIONS; i++)
    {
        sem_destroy(&station_sems[i]);
    }
    for (int i = 0; i < group_sems.size(); i++)
    {
        sem_destroy(&group_sems[i]);
    }
    cin.rdbuf(cinBuffer);
    cout.rdbuf(coutBuffer);
    return 0;
}
