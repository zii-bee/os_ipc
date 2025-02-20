#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define NUM_CHILDREN 5
#define MAX_LINE 1024
#define MAX_FILES 100
#define MAX_USERS 100

// enum for child indices
enum { PROCESS_CREATE = 0, MEMORY_ALLOC, FILE_OPEN, USER_LOGIN, SYSTEM_BOOT };

typedef struct {
    char filename[256];
    int count;
} FileCount;

typedef struct {
    char username[64];
} UserEntry;

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void) {
    int p2c[NUM_CHILDREN][2];  // 5 pipes from parent to children
    int c2p[NUM_CHILDREN][2];  // 5 pipes from each child to parent
    pid_t pids[NUM_CHILDREN]; // generate 5 pids

    // initialize all pipes 
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(p2c[i]) == -1)
            error_exit("pipe p2c creation failed");
        if (pipe(c2p[i]) == -1)
            error_exit("pipe c2p creation failed");
    }

    // 5 forks for 5 child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            error_exit("fork failed");
        } else if (pids[i] == 0) {  // inside the child process
            // close unused ends
            for (int j = 0; j < NUM_CHILDREN; j++) {
                if (j == i) { // given we are at the 'current' child
                    close(p2c[j][1]);  // child reads from this pipe
                    close(c2p[j][0]);  // child writes to this pipe
                } else {
                    // close all other pipes not used by this child
                    close(p2c[j][0]); close(p2c[j][1]);
                    close(c2p[j][0]); close(c2p[j][1]);
                }
            }
            // initialize file stream in r at reading end
            FILE *fp_in = fdopen(p2c[i][0], "r");
            if (!fp_in) error_exit("fdopen failed in child");

            char line[MAX_LINE]; // consider a max line
            // setting up required counters needed for analysis of sample file
            int count = 0;
            long mem_sum = 0;
            FileCount fileCounts[MAX_FILES];
            int fileCountSize = 0; 
            int uniqueUsers = 0;
            UserEntry users[MAX_USERS];

            // filecount array initialized
            for (int k = 0; k < MAX_FILES; k++) {
                fileCounts[k].count = 0;
                fileCounts[k].filename[0] = '\0';
            }

            // child specific
            while (fgets(line, sizeof(line), fp_in)) {
                // clean spaces, regular preprocessing
                line[strcspn(line, "\n")] = '\0';
                count++;
                if (i == PROCESS_CREATE) {
                    // count PROCESS_CREATE events
                    ;
                } else if (i == MEMORY_ALLOC) {
                    // parse SIZE field
                    char *ptr = strstr(line, "SIZE:");
                    if (ptr) {
                        long size;
                        if (sscanf(ptr, "SIZE:%ld", &size) == 1) {
                            mem_sum += size; // add to memory
                        }
                    }
                } else if (i == FILE_OPEN) {
                    // parse PATH field
                    char *ptr = strstr(line, "PATH:");
                    if (ptr) {
                        char path[256];
                        // deepseek parsing ftw
                        if (sscanf(ptr, "PATH:\"%255[^\"]\"", path) == 1) {
                            // look up if this file is already in our array
                            int found = 0;
                            for (int j = 0; j < fileCountSize; j++) {
                                if (strcmp(fileCounts[j].filename, path) == 0) {
                                    fileCounts[j].count++; // if found, do ++
                                    found = 1;
                                    break;
                                }
                            }
                            // if not found, create (fingers crossed MAX_FILES is large enough)
                            if (!found && fileCountSize < MAX_FILES) {
                                strcpy(fileCounts[fileCountSize].filename, path);
                                fileCounts[fileCountSize].count = 1; // set to one cause just found 1 instance
                                fileCountSize++;
                            }
                        }
                    }
                } else if (i == USER_LOGIN) {
                    // parse USER field
                    char *ptr = strstr(line, "USER:");
                    if (ptr) {
                        char user[64];
                        if (sscanf(ptr, "USER:\"%63[^\"]\"", user) == 1) {
                            // check if user is already recorded
                            int found = 0;
                            // same finding and ++ idea with a found int flag 
                            for (int j = 0; j < uniqueUsers; j++) {
                                if (strcmp(users[j].username, user) == 0) {
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found && uniqueUsers < MAX_USERS) {
                                strcpy(users[uniqueUsers].username, user);
                                uniqueUsers++;
                            }
                        }
                    }
                } else if (i == SYSTEM_BOOT) {
                    // count system boot events
                    ;
                }
            }
            fclose(fp_in); // close filestream

            // resultant string
            char result[512];
            if (i == PROCESS_CREATE) {
                snprintf(result, sizeof(result), "PROCESS_CREATE events: %d", count);
            } else if (i == MEMORY_ALLOC) {
                snprintf(result, sizeof(result), "MEMORY_ALLOC events: Total memory allocated: %ld bytes", mem_sum);
            } else if (i == FILE_OPEN) {
                // find file with maxcount
                int maxCount = 0;
                char maxFile[256] = ""; //max file string initialized as char arr
                for (int j = 0; j < fileCountSize; j++) {
                    if (fileCounts[j].count > maxCount) { // basic maxfinding strat
                        maxCount = fileCounts[j].count;
                        strcpy(maxFile, fileCounts[j].filename); // set filename if highest
                    }
                }
                // format string and write into print buffer
                snprintf(result, sizeof(result), "FILE_OPEN events: Most accessed file: \"%s\" (%d times)", maxFile, maxCount);
            } else if (i == USER_LOGIN) {
                snprintf(result, sizeof(result), "USER_LOGIN events: Number of unique users: %d", uniqueUsers);
            } else if (i == SYSTEM_BOOT) {
                snprintf(result, sizeof(result), "SYSTEM_BOOT events: Number of system boots: %d", count);
            }

            // Write result to parent
            FILE *fp_out = fdopen(c2p[i][1], "w");
            if (!fp_out) error_exit("fdopen failed in child (c2p)");
            fprintf(fp_out, "%s", result);
            fflush(fp_out);
            fclose(fp_out);
            exit(EXIT_SUCCESS);
        }
    }

    // for each child, close the read end of p2c and the write end of c2p
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(p2c[i][0]);
        close(c2p[i][1]);
    }

    // open event_logs file 
    FILE *logFile = fopen("events_log.txt", "r");
    if (!logFile) error_exit("Failed to open events_log.txt"); // error check

    // counters for overall stats
    int overallCount = 0;
    int typeCount[NUM_CHILDREN] = {0};

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), logFile)) {
        overallCount++;
        // strstr to look for relevant substring in a line and do counter based on that
        if (strstr(line, "PROCESS_CREATE"))
            typeCount[PROCESS_CREATE]++;
        else if (strstr(line, "MEMORY_ALLOC"))
            typeCount[MEMORY_ALLOC]++;
        else if (strstr(line, "FILE_OPEN"))
            typeCount[FILE_OPEN]++;
        else if (strstr(line, "USER_LOGIN"))
            typeCount[USER_LOGIN]++;
        else if (strstr(line, "SYSTEM_BOOT"))
            typeCount[SYSTEM_BOOT]++;
        
        // process delegation to child based on found flag substr
        int childIndex = -1; // init to no child, remains -1 if none of the relevant keywords are found 
        if (strstr(line, "PROCESS_CREATE"))
            childIndex = PROCESS_CREATE;
        else if (strstr(line, "MEMORY_ALLOC"))
            childIndex = MEMORY_ALLOC;
        else if (strstr(line, "FILE_OPEN"))
            childIndex = FILE_OPEN;
        else if (strstr(line, "USER_LOGIN"))
            childIndex = USER_LOGIN;
        else if (strstr(line, "SYSTEM_BOOT"))
            childIndex = SYSTEM_BOOT;
        
        if (childIndex != -1) {
            // write the line to the child's pipe
            if (write(p2c[childIndex][1], line, strlen(line)) == -1) {
                fprintf(stderr, "Failed to write to child %d pipe: %s\n", childIndex, strerror(errno));
            }
        }
    }
    fclose(logFile);

    // close write ends when done sending events
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(p2c[i][1]);
    }

    // wait for each child to finish result compilation
    char childResults[NUM_CHILDREN][512];
    for (int i = 0; i < NUM_CHILDREN; i++) {
        FILE *fp = fdopen(c2p[i][0], "r");
        if (fp) {
            if (fgets(childResults[i], sizeof(childResults[i]), fp) == NULL) {
                strcpy(childResults[i], "No data");
            }
            fclose(fp);
        } else {
            strcpy(childResults[i], "Error reading result");
        }
        wait(NULL); // wait for child
    }

    // print child results from sample out
    printf("%s\n", childResults[PROCESS_CREATE]);
    printf("%s\n", childResults[MEMORY_ALLOC]);
    printf("%s\n", childResults[FILE_OPEN]);
    printf("%s\n", childResults[USER_LOGIN]);
    printf("%s\n\n", childResults[SYSTEM_BOOT]);

    // get most common event 
    int maxType = 0;
    for (int i = 1; i < NUM_CHILDREN; i++) {
        if (typeCount[i] > typeCount[maxType])
            maxType = i;
    }
    const char *eventNames[NUM_CHILDREN] = {
        "PROCESS_CREATE", "MEMORY_ALLOC", "FILE_OPEN", "USER_LOGIN", "SYSTEM_BOOT"
    };

    // print statistics in sampe output format
    printf("Overall Statistics:\n");
    printf("Total events processed: %d\n", overallCount);
    printf("Most common event type: %s (%d occurrences)\n", eventNames[maxType], typeCount[maxType]);

    return 0;
}

