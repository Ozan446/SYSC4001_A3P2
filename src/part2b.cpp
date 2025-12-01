/**
 * part2b_101322055_101310590.cpp
 *
 * SYSC 4001 A - Operating Systems | Fall 2025
 * Carleton University
 *
 * Assignment 3 - Part 2: Concurrent Processes in Unix
 * Part 2.b)
 * 
 * @author Ozan Kaya    (Student Number: 101322055),
 *         Nate Babyak  (Student Number: 101310590)
 * @version 2025.11.30
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h> // Header for semaphores
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <iomanip>

using namespace std;

// Define shared memory structure (same as 2.a)
struct SharedData {
    int current_student_id;
    char rubric[5][3]; // e.g., "1,A", "2,B", max 3 chars including null terminator
    bool question_marked[5]; // Tracks if question 0-4 (corresponds to exercises 1-5) is marked
};

// --- Global Constants ---
const int SHM_KEY = 4001; // Shared Memory Key
const int SEM_KEY_RUBRIC = 5001; // Semaphore for Rubric Read/Write
const int SEM_KEY_MARKS = 5002; // Semaphore for Question Marking Array
const int SEM_KEY_PARENT_SIGNAL = 5003; // Semaphore for Next Exam Signal
const int RUBRIC_LINES = 5;
const char* RUBRIC_FILENAME = "rubric.txt";
const char* EXAM_PATTERN = "exams/exam_";

// Semaphore operation structs
struct sembuf P = {0, -1, 0}; // P operation (Wait/Lock): subtract 1
struct sembuf V = {0, +1, 0}; // V operation (Signal/Unlock): add 1

// --- Utility Functions ---

// Locks the specified semaphore
void lock_sem(int semid) {
    if (semop(semid, &P, 1) == -1) {
        perror("semop P failed");
        exit(1);
    }
}

// Unlocks the specified semaphore
void unlock_sem(int semid) {
    if (semop(semid, &V, 1) == -1) {
        perror("semop V failed");
        exit(1);
    }
}

// Loads the rubric from a file into shared memory (No change, as this happens at startup by Parent)
void load_rubric(SharedData* shared_mem) {
    ifstream file(RUBRIC_FILENAME);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << RUBRIC_FILENAME << endl;
        exit(1);
    }
    string line;
    int i = 0;
    while (i < RUBRIC_LINES && getline(file, line)) {
        shared_mem->rubric[i][0] = line[0];
        shared_mem->rubric[i][1] = line[2];
        shared_mem->rubric[i][2] = '\0';
        i++;
    }
    file.close();
}

// Loads the next exam into shared memory. Returns true on success, false if 9999 is reached.
bool load_next_exam(SharedData* shared_mem, int& current_exam_num) {
    current_exam_num++;

    stringstream ss;
    ss << EXAM_PATTERN << setfill('0') << setw(4) << current_exam_num << ".txt";
    string filename = ss.str();

    ifstream file(filename.c_str());
    if (!file.is_open()) {
        // If file doesn't exist, check if we hit the termination number
        if (current_exam_num > 9999) { // Should not happen if 9999 file is last
            current_exam_num--;
            return false;
        }
        // Fallback for missing file
        current_exam_num--;
        return false;
    }

    string student_id_str;
    getline(file, student_id_str);
    shared_mem->current_student_id = atoi(student_id_str.c_str());

    // Reset marking status for the new exam
    for (int i = 0; i < RUBRIC_LINES; i++) {
        shared_mem->question_marked[i] = false;
    }

    if (shared_mem->current_student_id == 9999) {
        cout << "--- TERMINATION SIGNAL (9999) RECEIVED ---" << endl;
        return false;
    }
    
    cout << "--- NEXT EXAM LOADED --- Student ID: " << shared_mem->current_student_id << endl;

    return true;
}

// Rewrites the rubric file with the current shared memory content
// NOTE: This function is called *inside* the critical section
void save_rubric(SharedData* shared_mem) {
    ofstream file(RUBRIC_FILENAME);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << RUBRIC_FILENAME << " for writing" << endl;
        return;
    }

    for (int i = 0; i < RUBRIC_LINES; i++) {
        file << shared_mem->rubric[i][0] << "," << shared_mem->rubric[i][1] << endl;
    }
    file.close();
    cout << "--- Rubric file saved ---" << endl;
}

// --- TA Process Logic ---

void ta_process(int ta_id, SharedData* shared_mem, int semid_rubric, int semid_marks, int semid_parent_signal) {
    cout << "TA " << ta_id << " started." << endl;

    while (true) {
        // Check for termination outside critical sections (read only)
        if (shared_mem->current_student_id == 9999) {
            cout << "TA " << ta_id << " detected termination signal and is exiting." << endl;
            break;
        }

        // --- Task 1: Check and potentially correct the Rubric (CRITICAL SECTION 1) ---
        lock_sem(semid_rubric); // LOCK RUBRIC

        cout << "TA " << ta_id << " entered Rubric critical section." << endl;
        for (int i = 0; i < RUBRIC_LINES; i++) {
            // Random delay between 500ms and 1000ms
            int delay = 500000 + (rand() % 500001); // in microseconds
            usleep(delay);

            // Randomly decide if a correction is needed (e.g., 10% chance)
            if (rand() % 10 == 0) {
                // Correction needed
                char old_char = shared_mem->rubric[i][1];
                // Check if we can increment without hitting non-printable characters or wrapping
                if (old_char >= 'A' && old_char < 'Z') {
                    shared_mem->rubric[i][1] = old_char + 1; 
                } else if (old_char == 'Z') {
                     shared_mem->rubric[i][1] = 'A'; // Wrap around
                } else {
                     shared_mem->rubric[i][1] = 'A'; // Default to A if corrupted
                }
                
                cout << "TA " << ta_id << " MODIFIED Rubric line " << i + 1
                     << ": '" << old_char << "' -> '" << shared_mem->rubric[i][1] << "'" << endl;

                // Since we are inside the critical section, this file save is mutually exclusive
                save_rubric(shared_mem);
            }
        }
        cout << "TA " << ta_id << " left Rubric critical section." << endl;
        unlock_sem(semid_rubric); // UNLOCK RUBRIC

        // --- Task 2: Mark an exercise in the Exam (CRITICAL SECTION 2) ---

        lock_sem(semid_marks); // LOCK MARKS ARRAY

        int question_to_mark = -1;
        // Find an unmarked question (Read-Modify-Write)
        for (int i = 0; i < RUBRIC_LINES; i++) {
            if (!shared_mem->question_marked[i]) {
                question_to_mark = i;
                shared_mem->question_marked[i] = true; // Set to true here to prevent other TAs from picking it
                break;
            }
        }

        unlock_sem(semid_marks); // UNLOCK MARKS ARRAY (The selection is done)

        if (question_to_mark != -1) {
            // Mark the question (Outside critical section, allowing other TAs to work)
            cout << "TA " << ta_id << " started marking Question " << question_to_mark + 1
                 << " for Student " << shared_mem->current_student_id << endl;

            // Marking delay between 1.0 and 2.0 seconds
            int delay = 1000000 + (rand() % 1000001); // in microseconds
            usleep(delay);

            // Display completion
            cout << "TA " << ta_id << " FINISHED marking Question " << question_to_mark + 1
                 << " for Student " << shared_mem->current_student_id << endl;
        } else {
            // All questions are marked. Signal the parent to load the next exam.

            lock_sem(semid_parent_signal); // LOCK PARENT SIGNAL VARIABLE

            // Double check that another TA hasn't already signaled/loaded the next exam
            if (shared_mem->current_student_id != 9999) {
                 cout << "TA " << ta_id << " detected all questions marked. SIGNALING parent." << endl;
                 shared_mem->current_student_id = 0; // The signal to the parent to load the next exam
            }
            
            unlock_sem(semid_parent_signal); // UNLOCK PARENT SIGNAL VARIABLE
            
            // Wait briefly for the next exam to be loaded before checking the termination condition
            usleep(100000); 
        }
    }
}

// --- Main Program ---

int main(int argc, char* argv[]) {
    srand(time(0) + getpid());

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number_of_TAs (n >= 2)>" << endl;
        return 1;
    }

    int n_tas = atoi(argv[1]);
    if (n_tas < 2) {
        cerr << "Error: Number of TAs must be >= 2." << endl;
        return 1;
    }

    // --- 1. Setup Shared Memory ---
    size_t shm_size = sizeof(SharedData);
    int shmid = shmget(SHM_KEY, shm_size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }

    SharedData* shared_mem = (SharedData*)shmat(shmid, NULL, 0);
    if (shared_mem == (SharedData*)-1) {
        perror("shmat failed");
        shmctl(shmid, IPC_RMID, NULL); 
        return 1;
    }
    
    // --- 2. Setup Semaphores ---
    
    // a) Rubric Mutex
    int semid_rubric = semget(SEM_KEY_RUBRIC, 1, IPC_CREAT | 0666);
    if (semid_rubric < 0) { perror("semget RUBRIC failed"); shmctl(shmid, IPC_RMID, NULL); return 1; }
    // b) Marks Array Mutex
    int semid_marks = semget(SEM_KEY_MARKS, 1, IPC_CREAT | 0666);
    if (semid_marks < 0) { perror("semget MARKS failed"); semctl(semid_rubric, 0, IPC_RMID); shmctl(shmid, IPC_RMID, NULL); return 1; }
    // c) Parent Signal Mutex
    int semid_parent_signal = semget(SEM_KEY_PARENT_SIGNAL, 1, IPC_CREAT | 0666);
    if (semid_parent_signal < 0) { perror("semget SIGNAL failed"); semctl(semid_rubric, 0, IPC_RMID); semctl(semid_marks, 0, IPC_RMID); shmctl(shmid, IPC_RMID, NULL); return 1; }


    // Initialize all semaphores to 1 (Unlocked/Binary Mutex)
    semun arg;
    arg.val = 1;
    if (semctl(semid_rubric, 0, SETVAL, arg) < 0) { perror("semctl RUBRIC failed"); return 1; }
    if (semctl(semid_marks, 0, SETVAL, arg) < 0) { perror("semctl MARKS failed"); return 1; }
    if (semctl(semid_parent_signal, 0, SETVAL, arg) < 0) { perror("semctl SIGNAL failed"); return 1; }

    
    // --- 3. Initial Shared Data Setup ---
    int current_exam_num = 0;
    load_rubric(shared_mem); 
    if (!load_next_exam(shared_mem, current_exam_num)) { 
        cerr << "Error loading initial exam or received 9999 immediately." << endl;
        // Clean up
        shmdt(shared_mem);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid_rubric, 0, IPC_RMID);
        semctl(semid_marks, 0, IPC_RMID);
        semctl(semid_parent_signal, 0, IPC_RMID);
        return 1;
    }

    // --- 4. Fork TA Processes ---
    vector<pid_t> children;
    for (int i = 1; i <= n_tas; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process (TA)
            // Pass semaphore IDs to the TA function
            ta_process(i, shared_mem, semid_rubric, semid_marks, semid_parent_signal);
            shmdt(shared_mem); 
            return 0;
        } else if (pid > 0) {
            children.push_back(pid);
        } else {
            perror("fork failed");
            // In a real scenario, you'd clean up shared memory and semaphores here too.
        }
    }

    // --- 5. Parent Process (Manages exam loading) ---
    while (shared_mem->current_student_id != 9999) {
        // Parent MUST protect the check/load logic using the PARENT_SIGNAL semaphore
        lock_sem(semid_parent_signal);
        
        if (shared_mem->current_student_id == 0) {
            if (!load_next_exam(shared_mem, current_exam_num)) {
                // Termination exam (9999) or end of files reached
                shared_mem->current_student_id = 9999;
            }
        }
        
        unlock_sem(semid_parent_signal);

        usleep(100000); // Wait 100ms before checking again
    }
    
    // 6. Wait for all TA processes to finish
    for (pid_t pid : children) {
        waitpid(pid, NULL, 0);
    }

    // 7. Cleanup Shared Resources
    shmdt(shared_mem);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid_rubric, 0, IPC_RMID);
    semctl(semid_marks, 0, IPC_RMID);
    semctl(semid_parent_signal, 0, IPC_RMID);

    cout << "All TAs finished. Simulation complete." << endl;

    return 0;
}
