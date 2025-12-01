/**
 * part2a_101322055_101310590.cpp
 *
 * SYSC 4001 A - Operating Systems | Fall 2025
 * Carleton University
 *
 * Assignment 3 - Part 2: Concurrent Processes in Unix
 * Part 2.a)
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
#include <cstdlib>
#include <ctime>
#include <iomanip>

using namespace std;

// Define shared memory structure
// This structure holds the currently loaded Exam data and Rubric data
struct SharedData {
    int current_student_id;
    char rubric[5][3]; // e.g., "1,A", "2,B", max 3 chars including null terminator
    bool question_marked[5]; // Tracks if question 0-4 (corresponds to exercises 1-5) is marked
};

const int SHM_KEY = 4001;
const int RUBRIC_LINES = 5;
const char* RUBRIC_FILENAME = "rubric.txt";
const char* EXAM_PATTERN = "exams/exam_";

// --- Utility Functions ---

// Loads the rubric from a file into shared memory
void load_rubric(SharedData* shared_mem) {
    ifstream file(RUBRIC_FILENAME);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << RUBRIC_FILENAME << endl;
        exit(1);
    }
    string line;
    int i = 0;
    while (i < RUBRIC_LINES && getline(file, line)) {
        // Simple copy up to 2 chars after the comma (e.g., '1,A\0')
        // Assuming the format is strictly 'X, Y'
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

    // Generate filename based on current_exam_num
    stringstream ss;
    ss << EXAM_PATTERN << setfill('0') << setw(4) << current_exam_num << ".txt";
    string filename = ss.str();

    ifstream file(filename.c_str());
    if (!file.is_open()) {
        // If file doesn't exist, we assume we hit the end of the list before 9999
        // For a robust system, you'd manage the list better, but for this simulation, we check for 9999
        current_exam_num--; // Revert counter if file is missing prematurely
        return false;
    }

    string student_id_str;
    getline(file, student_id_str);
    shared_mem->current_student_id = atoi(student_id_str.c_str());

    // Reset marking status for the new exam
    for (int i = 0; i < RUBRIC_LINES; i++) {
        shared_mem->question_marked[i] = false;
    }

    cout << "--- NEXT EXAM LOADED --- Student ID: " << shared_mem->current_student_id << endl;

    if (shared_mem->current_student_id == 9999) {
        cout << "--- TERMINATION SIGNAL (9999) RECEIVED ---" << endl;
        return false;
    }

    return true;
}

// Rewrites the rubric file with the current shared memory content
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

void ta_process(int ta_id, SharedData* shared_mem, int total_tas) {
    cout << "TA " << ta_id << " started." << endl;

    while (true) {
        // Check for termination
        if (shared_mem->current_student_id == 9999) {
            cout << "TA " << ta_id << " detected termination signal and is exiting." << endl;
            break;
        }

        // --- Task 1: Check and potentially correct the Rubric ---
        cout << "TA " << ta_id << " is checking the Rubric." << endl;
        for (int i = 0; i < RUBRIC_LINES; i++) {
            // Random delay between 500ms and 1000ms (0.5 to 1.0 seconds)
            int delay = 500000 + (rand() % 500001); // in microseconds
            usleep(delay);

            // Randomly decide if a correction is needed (e.g., 10% chance)
            if (rand() % 10 == 0) {
                // Correction needed [cite: 144]
                char old_char = shared_mem->rubric[i][1];
                shared_mem->rubric[i][1] = old_char + 1; // Replace with next ASCII code
                cout << "TA " << ta_id << " MODIFIED Rubric line " << i + 1
                     << ": '" << old_char << "' -> '" << shared_mem->rubric[i][1] << "'" << endl;

                // Save change to the file (Race condition here is allowed in Part 2.a)
                save_rubric(shared_mem);
            }
        }

        // --- Task 2: Mark an exercise in the Exam ---

        // Find an unmarked question
        int question_to_mark = -1;
        for (int i = 0; i < RUBRIC_LINES; i++) {
            // Check if the question is unmarked. Race condition is okay here.
            if (!shared_mem->question_marked[i]) {
                question_to_mark = i;
                // Mark it immediately to prevent another TA from *easily* picking it up
                // (though not guaranteed due to lack of synchronization)
                shared_mem->question_marked[i] = true;
                break;
            }
        }

        if (question_to_mark != -1) {
            // Mark the question [cite: 148]
            cout << "TA " << ta_id << " started marking Question " << question_to_mark + 1
                 << " for Student " << shared_mem->current_student_id << endl;

            // Marking delay between 1.0 and 2.0 seconds
            int delay = 1000000 + (rand() % 1000001); // in microseconds
            usleep(delay);

            // Display completion
            cout << "TA " << ta_id << " FINISHED marking Question " << question_to_mark + 1
                 << " for Student " << shared_mem->current_student_id << endl;
            
            // Note: Writing marks to exam file is ignored per requirement
        } else {
            // All questions are marked. One TA must load the next exam.
            
            // Only TA 1 will attempt to load the next exam to avoid excessive attempts 
            // (though multiple TAs reading is allowed in 2.a)
            if (ta_id == 1) { 
                 cout << "TA " << ta_id << " detected all questions marked. Attempting to load next exam." << endl;
                 // Increment the counter directly in shared memory for the next TA to use
                 // Note: 'current_exam_num' is managed in the main process to ensure ordered file access.
                 // This simulation relies on the main process to update the student ID *after* the TA detects completion.
                 
                 // Instead of loading, TA 1 will signal the main process by setting the student_id to 0 temporarily
                 // This is a simple (non-blocking) mechanism to trigger the main loop to load the next one.
                 shared_mem->current_student_id = 0;
            }
            // Wait briefly for the next exam to be loaded before re-checking the termination condition
            usleep(100000); 
        }
    }
}

// --- Main Program ---

int main(int argc, char* argv[]) {
    srand(time(0) + getpid()); // Seed random number generator

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number_of_TAs (n >= 2)>" << endl;
        return 1;
    }

    int n_tas = atoi(argv[1]);
    if (n_tas < 2) {
        cerr << "Error: Number of TAs must be >= 2." << endl;
        return 1;
    }

    // 1. Setup Shared Memory
    size_t shm_size = sizeof(SharedData);
    int shmid = shmget(SHM_KEY, shm_size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }

    SharedData* shared_mem = (SharedData*)shmat(shmid, NULL, 0);
    if (shared_mem == (SharedData*)-1) {
        perror("shmat failed");
        shmctl(shmid, IPC_RMID, NULL); // Clean up if attach fails
        return 1;
    }

    // Initialize shared memory
    int current_exam_num = 0;
    load_rubric(shared_mem); // Load rubric on startup
    if (!load_next_exam(shared_mem, current_exam_num)) { // Load first exam on startup
        cerr << "Error loading initial exam." << endl;
        shmdt(shared_mem);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    // 2. Fork TA Processes
    vector<pid_t> children;
    for (int i = 1; i <= n_tas; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process (TA)
            ta_process(i, shared_mem, n_tas);
            shmdt(shared_mem); // Detach child from shared memory
            return 0;
        } else if (pid > 0) {
            // Parent process
            children.push_back(pid);
        } else {
            perror("fork failed");
        }
    }

    // 3. Parent Process (Manages exam loading)
    while (shared_mem->current_student_id != 9999) {
        // This is a simple polling loop in the parent to manage next exam loading.
        // The TA processes set current_student_id to 0 when an exam is finished.
        if (shared_mem->current_student_id == 0) {
            if (!load_next_exam(shared_mem, current_exam_num)) {
                // Termination exam (9999) or end of files reached
                shared_mem->current_student_id = 9999;
                break; 
            }
        }
        usleep(100000); // Wait 100ms before checking again
    }
    
    // 4. Wait for all TA processes to finish
    for (pid_t pid : children) {
        waitpid(pid, NULL, 0);
    }

    // 5. Cleanup Shared Memory
    shmdt(shared_mem);
    shmctl(shmid, IPC_RMID, NULL);

    cout << "All TAs finished. Simulation complete." << endl;

    return 0;
}
