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

// Child indices mapping:
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
    int p2c[NUM_CHILDREN][2];  // Parent to Child pipes
    int c2p[NUM_CHILDREN][2];  // Child to Parent pipes
    pid_t pids[NUM_CHILDREN];

    // Create pipes for each child (two pipes per child)
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(p2c[i]) == -1)
            error_exit("pipe p2c creation failed");
        if (pipe(c2p[i]) == -1)
            error_exit("pipe c2p creation failed");
    }

    // Fork child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            error_exit("fork failed");
        } else if (pids[i] == 0) {  // Child process
            // Close unused ends:
            // Close write end of p2c and read end of c2p for this child
            for (int j = 0; j < NUM_CHILDREN; j++) {
                if (j == i) {
                    close(p2c[j][1]);  // child reads from this pipe
                    close(c2p[j][0]);  // child writes to this pipe
                } else {
                    // Close all other pipes not used by this child
                    close(p2c[j][0]); close(p2c[j][1]);
                    close(c2p[j][0]); close(c2p[j][1]);
                }
            }
            // Open a FILE stream for reading from the parent
            FILE *fp_in = fdopen(p2c[i][0], "r");
            if (!fp_in) error_exit("fdopen failed in child");

            char line[MAX_LINE];
            int count = 0;
            long mem_sum = 0;
            FileCount fileCounts[MAX_FILES];
            int fileCountSize = 0;
            int uniqueUsers = 0;
            UserEntry users[MAX_USERS];

            // Initialize fileCounts array
            for (int k = 0; k < MAX_FILES; k++) {
                fileCounts[k].count = 0;
                fileCounts[k].filename[0] = '\0';
            }

            // Child-specific processing
            while (fgets(line, sizeof(line), fp_in)) {
                // Remove trailing newline if any
                line[strcspn(line, "\n")] = '\0';
                count++;
                if (i == PROCESS_CREATE) {
                    // Simply count PROCESS_CREATE events
                    ;
                } else if (i == MEMORY_ALLOC) {
                    // Parse SIZE field: e.g., SIZE:1048576
                    char *ptr = strstr(line, "SIZE:");
                    if (ptr) {
                        long size;
                        if (sscanf(ptr, "SIZE:%ld", &size) == 1) {
                            mem_sum += size;
                        }
                    }
                } else if (i == FILE_OPEN) {
                    // Parse PATH field: e.g., PATH:"/etc/passwd"
                    char *ptr = strstr(line, "PATH:");
                    if (ptr) {
                        char path[256];
                        if (sscanf(ptr, "PATH:\"%255[^\"]\"", path) == 1) {
                            // Look up if this file is already in our array
                            int found = 0;
                            for (int j = 0; j < fileCountSize; j++) {
                                if (strcmp(fileCounts[j].filename, path) == 0) {
                                    fileCounts[j].count++;
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found && fileCountSize < MAX_FILES) {
                                strcpy(fileCounts[fileCountSize].filename, path);
                                fileCounts[fileCountSize].count = 1;
                                fileCountSize++;
                            }
                        }
                    }
                } else if (i == USER_LOGIN) {
                    // Parse USER field: e.g., USER:"alice"
                    char *ptr = strstr(line, "USER:");
                    if (ptr) {
                        char user[64];
                        if (sscanf(ptr, "USER:\"%63[^\"]\"", user) == 1) {
                            // Check if user is already recorded
                            int found = 0;
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
                    // Count system boot events
                    ;
                }
            }
            fclose(fp_in);

            // Prepare result string
            char result[512];
            if (i == PROCESS_CREATE) {
                snprintf(result, sizeof(result), "PROCESS_CREATE events: %d", count);
            } else if (i == MEMORY_ALLOC) {
                snprintf(result, sizeof(result), "MEMORY_ALLOC events: Total memory allocated: %ld bytes", mem_sum);
            } else if (i == FILE_OPEN) {
                // Find the file with maximum count
                int maxCount = 0;
                char maxFile[256] = "";
                for (int j = 0; j < fileCountSize; j++) {
                    if (fileCounts[j].count > maxCount) {
                        maxCount = fileCounts[j].count;
                        strcpy(maxFile, fileCounts[j].filename);
                    }
                }
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
        // Parent process continues to next child...
    }

    // Parent: close unused pipe ends.
    // For each child, close the read end of p2c and the write end of c2p.
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(p2c[i][0]);
        close(c2p[i][1]);
    }

    // Open the events log file (the file provided with the assignment)
    FILE *logFile = fopen("events_log.txt", "r");
    if (!logFile) error_exit("Failed to open events_log.txt");

    // Counters for overall statistics (for the 5 event types)
    int overallCount = 0;
    int typeCount[NUM_CHILDREN] = {0};

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), logFile)) {
        overallCount++;
        // Determine event type by looking for keywords in the line
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
        
        // Dispatch the line to the correct child
        int childIndex = -1;
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
            // Write the line to the child's pipe
            if (write(p2c[childIndex][1], line, strlen(line)) == -1) {
                fprintf(stderr, "Failed to write to child %d pipe: %s\n", childIndex, strerror(errno));
            }
        }
    }
    fclose(logFile);

    // Done sending events: close all write ends to signal EOF to children.
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(p2c[i][1]);
    }

    // Wait for children to finish and collect results
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
        wait(NULL); // Wait for each child (could be done in a loop with proper pid matching)
    }

    // Print child results in order:
    printf("%s\n", childResults[PROCESS_CREATE]);
    printf("%s\n", childResults[MEMORY_ALLOC]);
    printf("%s\n", childResults[FILE_OPEN]);
    printf("%s\n", childResults[USER_LOGIN]);
    printf("%s\n\n", childResults[SYSTEM_BOOT]);

    // Calculate most common event type
    int maxType = 0;
    for (int i = 1; i < NUM_CHILDREN; i++) {
        if (typeCount[i] > typeCount[maxType])
            maxType = i;
    }
    const char *eventNames[NUM_CHILDREN] = {
        "PROCESS_CREATE", "MEMORY_ALLOC", "FILE_OPEN", "USER_LOGIN", "SYSTEM_BOOT"
    };

    // Print overall statistics exactly as required
    printf("Overall Statistics:\n");
    printf("Total events processed: %d\n", overallCount);
    printf("Most common event type: %s (%d occurrences)\n", eventNames[maxType], typeCount[maxType]);

    return 0;
}
