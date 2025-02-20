// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <sys/wait.h>
// #include <string.h>
// #include <errno.h>

// #define NUM_CHILDREN 5
// #define MAX_LINE 1024
// #define MAX_FILES 100
// #define MAX_USERS 100

// // Child indices mapping:
// enum { PROCESS_CREATE = 0, MEMORY_ALLOC, FILE_OPEN, USER_LOGIN, SYSTEM_BOOT };

// typedef struct {
//     char filename[256];
//     int count;
// } FileCount;

// typedef struct {
//     char username[64];
// } UserEntry;

// void error_exit(const char *msg) {
//     perror(msg);
//     exit(EXIT_FAILURE);
// }

// int main(void) {
//     int p2c[NUM_CHILDREN][2];  // Parent to Child pipes
//     int c2p[NUM_CHILDREN][2];  // Child to Parent pipes
//     pid_t pids[NUM_CHILDREN];

//     // Create pipes for each child (two pipes per child)
//     for (int i = 0; i < NUM_CHILDREN; i++) {
//         if (pipe(p2c[i]) == -1)
//             error_exit("pipe p2c creation failed");
//         if (pipe(c2p[i]) == -1)
//             error_exit("pipe c2p creation failed");
//     }

//     // Fork child processes
//     for (int i = 0; i < NUM_CHILDREN; i++) {
//         pids[i] = fork();
//         if (pids[i] < 0) {
//             error_exit("fork failed");
//         } else if (pids[i] == 0) {  // Child process
//             // Close unused ends:
//             // Close write end of p2c and read end of c2p for this child
//             for (int j = 0; j < NUM_CHILDREN; j++) {
//                 if (j == i) {
//                     close(p2c[j][1]);  // child reads from this pipe
//                     close(c2p[j][0]);  // child writes to this pipe
//                 } else {
//                     // Close all other pipes not used by this child
//                     close(p2c[j][0]); close(p2c[j][1]);
//                     close(c2p[j][0]); close(c2p[j][1]);
//                 }
//             }
//             // Open a FILE stream for reading from the parent
//             FILE *fp_in = fdopen(p2c[i][0], "r");
//             if (!fp_in) error_exit("fdopen failed in child");

//             char line[MAX_LINE];
//             int count = 0;
//             long mem_sum = 0;
//             FileCount fileCounts[MAX_FILES];
//             int fileCountSize = 0;
//             int uniqueUsers = 0;
//             UserEntry users[MAX_USERS];

//             // Initialize fileCounts array
//             for (int k = 0; k < MAX_FILES; k++) {
//                 fileCounts[k].count = 0;
//                 fileCounts[k].filename[0] = '\0';
//             }

//             // Child-specific processing
//             while (fgets(line, sizeof(line), fp_in)) {
//                 // Remove trailing newline if any
//                 line[strcspn(line, "\n")] = '\0';
//                 count++;
//                 if (i == PROCESS_CREATE) {
//                     // Simply count PROCESS_CREATE events
//                     ;
//                 } else if (i == MEMORY_ALLOC) {
//                     // Parse SIZE field: e.g., SIZE:1048576
//                     char *ptr = strstr(line, "SIZE:");
//                     if (ptr) {
//                         long size;
//                         if (sscanf(ptr, "SIZE:%ld", &size) == 1) {
//                             mem_sum += size;
//                         }
//                     }
//                 } else if (i == FILE_OPEN) {
//                     // Parse PATH field: e.g., PATH:"/etc/passwd"
//                     char *ptr = strstr(line, "PATH:");
//                     if (ptr) {
//                         char path[256];
//                         if (sscanf(ptr, "PATH:\"%255[^\"]\"", path) == 1) {
//                             // Look up if this file is already in our array
//                             int found = 0;
//                             for (int j = 0; j < fileCountSize; j++) {
//                                 if (strcmp(fileCounts[j].filename, path) == 0) {
//                                     fileCounts[j].count++;
//                                     found = 1;
//                                     break;
//                                 }
//                             }
//                             if (!found && fileCountSize < MAX_FILES) {
//                                 strcpy(fileCounts[fileCountSize].filename, path);
//                                 fileCounts[fileCountSize].count = 1;
//                                 fileCountSize++;
//                             }
//                         }
//                     }
//                 } else if (i == USER_LOGIN) {
//                     // Parse USER field: e.g., USER:"alice"
//                     char *ptr = strstr(line, "USER:");
//                     if (ptr) {
//                         char user[64];
//                         if (sscanf(ptr, "USER:\"%63[^\"]\"", user) == 1) {
//                             // Check if user is already recorded
//                             int found = 0;
//                             for (int j = 0; j < uniqueUsers; j++) {
//                                 if (strcmp(users[j].username, user) == 0) {
//                                     found = 1;
//                                     break;
//                                 }
//                             }
//                             if (!found && uniqueUsers < MAX_USERS) {
//                                 strcpy(users[uniqueUsers].username, user);
//                                 uniqueUsers++;
//                             }
//                         }
//                     }
//                 } else if (i == SYSTEM_BOOT) {
//                     // Count system boot events
//                     ;
//                 }
//             }
//             fclose(fp_in);

//             // Prepare result string
//             char result[512];
//             if (i == PROCESS_CREATE) {
//                 snprintf(result, sizeof(result), "PROCESS_CREATE events: %d", count);
//             } else if (i == MEMORY_ALLOC) {
//                 snprintf(result, sizeof(result), "MEMORY_ALLOC events: Total memory allocated: %ld bytes", mem_sum);
//             } else if (i == FILE_OPEN) {
//                 // Find the file with maximum count
//                 int maxCount = 0;
//                 char maxFile[256] = "";
//                 for (int j = 0; j < fileCountSize; j++) {
//                     if (fileCounts[j].count > maxCount) {
//                         maxCount = fileCounts[j].count;
//                         strcpy(maxFile, fileCounts[j].filename);
//                     }
//                 }
//                 snprintf(result, sizeof(result), "FILE_OPEN events: Most accessed file: \"%s\" (%d times)", maxFile, maxCount);
//             } else if (i == USER_LOGIN) {
//                 snprintf(result, sizeof(result), "USER_LOGIN events: Number of unique users: %d", uniqueUsers);
//             } else if (i == SYSTEM_BOOT) {
//                 snprintf(result, sizeof(result), "SYSTEM_BOOT events: Number of system boots: %d", count);
//             }

//             // Write result to parent
//             FILE *fp_out = fdopen(c2p[i][1], "w");
//             if (!fp_out) error_exit("fdopen failed in child (c2p)");
//             fprintf(fp_out, "%s", result);
//             fflush(fp_out);
//             fclose(fp_out);
//             exit(EXIT_SUCCESS);
//         }
//         // Parent process continues to next child...
//     }

//     // Parent: close unused pipe ends.
//     // For each child, close the read end of p2c and the write end of c2p.
//     for (int i = 0; i < NUM_CHILDREN; i++) {
//         close(p2c[i][0]);
//         close(c2p[i][1]);
//     }

//     // Open the events log file (the file provided with the assignment)
//     FILE *logFile = fopen("events_log.txt", "r");
//     if (!logFile) error_exit("Failed to open events_log.txt");

//     // Counters for overall statistics (for the 5 event types)
//     int overallCount = 0;
//     int typeCount[NUM_CHILDREN] = {0};

//     char line[MAX_LINE];
//     while (fgets(line, sizeof(line), logFile)) {
//         overallCount++;
//         // Determine event type by looking for keywords in the line
//         if (strstr(line, "PROCESS_CREATE"))
//             typeCount[PROCESS_CREATE]++;
//         else if (strstr(line, "MEMORY_ALLOC"))
//             typeCount[MEMORY_ALLOC]++;
//         else if (strstr(line, "FILE_OPEN"))
//             typeCount[FILE_OPEN]++;
//         else if (strstr(line, "USER_LOGIN"))
//             typeCount[USER_LOGIN]++;
//         else if (strstr(line, "SYSTEM_BOOT"))
//             typeCount[SYSTEM_BOOT]++;
        
//         // Dispatch the line to the correct child
//         int childIndex = -1;
//         if (strstr(line, "PROCESS_CREATE"))
//             childIndex = PROCESS_CREATE;
//         else if (strstr(line, "MEMORY_ALLOC"))
//             childIndex = MEMORY_ALLOC;
//         else if (strstr(line, "FILE_OPEN"))
//             childIndex = FILE_OPEN;
//         else if (strstr(line, "USER_LOGIN"))
//             childIndex = USER_LOGIN;
//         else if (strstr(line, "SYSTEM_BOOT"))
//             childIndex = SYSTEM_BOOT;
        
//         if (childIndex != -1) {
//             // Write the line to the child's pipe
//             if (write(p2c[childIndex][1], line, strlen(line)) == -1) {
//                 fprintf(stderr, "Failed to write to child %d pipe: %s\n", childIndex, strerror(errno));
//             }
//         }
//     }
//     fclose(logFile);

//     // Done sending events: close all write ends to signal EOF to children.
//     for (int i = 0; i < NUM_CHILDREN; i++) {
//         close(p2c[i][1]);
//     }

//     // Wait for children to finish and collect results
//     char childResults[NUM_CHILDREN][512];
//     for (int i = 0; i < NUM_CHILDREN; i++) {
//         FILE *fp = fdopen(c2p[i][0], "r");
//         if (fp) {
//             if (fgets(childResults[i], sizeof(childResults[i]), fp) == NULL) {
//                 strcpy(childResults[i], "No data");
//             }
//             fclose(fp);
//         } else {
//             strcpy(childResults[i], "Error reading result");
//         }
//         wait(NULL); // Wait for each child (could be done in a loop with proper pid matching)
//     }

//     // Print child results in order:
//     printf("%s\n", childResults[PROCESS_CREATE]);
//     printf("%s\n", childResults[MEMORY_ALLOC]);
//     printf("%s\n", childResults[FILE_OPEN]);
//     printf("%s\n", childResults[USER_LOGIN]);
//     printf("%s\n\n", childResults[SYSTEM_BOOT]);

//     // Calculate most common event type
//     int maxType = 0;
//     for (int i = 1; i < NUM_CHILDREN; i++) {
//         if (typeCount[i] > typeCount[maxType])
//             maxType = i;
//     }
//     const char *eventNames[NUM_CHILDREN] = {
//         "PROCESS_CREATE", "MEMORY_ALLOC", "FILE_OPEN", "USER_LOGIN", "SYSTEM_BOOT"
//     };

//     // Print overall statistics exactly as required
//     printf("Overall Statistics:\n");
//     printf("Total events processed: %d\n", overallCount);
//     printf("Most common event type: %s (%d occurrences)\n", eventNames[maxType], typeCount[maxType]);

//     return 0;
// }

// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NUM_CHILDREN 5
#define BUFFER_SIZE 1024

typedef enum {
    PROCESS_CREATE,
    MEMORY_ALLOC,
    FILE_OPEN,
    USER_LOGIN,
    SYSTEM_BOOT,
    UNKNOWN
} EventType;

EventType get_event_type(const char *event_str) {
    if (strcmp(event_str, "PROCESS_CREATE") == 0) return PROCESS_CREATE;
    if (strcmp(event_str, "MEMORY_ALLOC") == 0) return MEMORY_ALLOC;
    if (strcmp(event_str, "FILE_OPEN") == 0) return FILE_OPEN;
    if (strcmp(event_str, "USER_LOGIN") == 0) return USER_LOGIN;
    if (strcmp(event_str, "SYSTEM_BOOT") == 0) return SYSTEM_BOOT;
    return UNKNOWN;
}

typedef struct FileEntry {
    char *path;
    int count;
    FileEntry *next;  // Use typedef'd type
};

typedef struct UserEntry {
    char *username;
    UserEntry *next;  // Use typedef'd type
};

void child_process(int index, int input_fd, int output_fd) {
    close(input_fd);
    close(output_fd);

    int in_fd = input_fd;
    int out_fd = output_fd;

    switch (index) {
        case PROCESS_CREATE: {
            int count = 0;
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(in_fd, buffer, BUFFER_SIZE)) > 0) {
                count++;
            }
            dprintf(out_fd, "PROCESS_CREATE:%d\n", count);
            break;
        }
        case MEMORY_ALLOC: {
            long total = 0;
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(in_fd, buffer, BUFFER_SIZE)) > 0) {
                char *size_start = strstr(buffer, "SIZE:");
                if (size_start) {
                    size_start += 5;
                    total += strtol(size_start, NULL, 10);
                }
            }
            dprintf(out_fd, "MEMORY_ALLOC:%ld\n", total);
            break;
        }
        case FILE_OPEN: {
            FileEntry *head = NULL;
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(in_fd, buffer, BUFFER_SIZE)) > 0) {
                char *path_start = strstr(buffer, "PATH:\"");
                if (path_start) {
                    path_start += 6;
                    char *path_end = strchr(path_start, '"');
                    if (path_end) {
                        int path_len = path_end - path_start;
                        char *path = malloc(path_len + 1);
                        strncpy(path, path_start, path_len);
                        path[path_len] = '\0';

                        FileEntry *current = head;
                        while (current) {
                            if (strcmp(current->path, path) == 0) {
                                current->count++;
                                free(path);
                                path = NULL;
                                break;
                            }
                            current = current->next;
                        }
                        if (path) {
                            FileEntry *new_entry = malloc(sizeof(FileEntry));
                            new_entry->path = path;
                            new_entry->count = 1;
                            new_entry->next = head;
                            head = new_entry;
                        }
                    }
                }
            }
            int max_count = 0;
            char *max_path = "";
            FileEntry *current = head;
            while (current) {
                if (current->count > max_count) {
                    max_count = current->count;
                    max_path = current->path;
                }
                current = current->next;
            }
            dprintf(out_fd, "FILE_OPEN:%s:%d\n", max_path, max_count);
            while (head) {
                FileEntry *next = head->next;
                free(head->path);
                free(head);
                head = next;
            }
            break;
        }
        case USER_LOGIN: {
            UserEntry *head = NULL;
            int count = 0;
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(in_fd, buffer, BUFFER_SIZE)) > 0) {
                char *user_start = strstr(buffer, "USER:\"");
                if (user_start) {
                    user_start += 6;
                    char *user_end = strchr(user_start, '"');
                    if (user_end) {
                        int user_len = user_end - user_start;
                        char *username = malloc(user_len + 1);
                        strncpy(username, user_start, user_len);
                        username[user_len] = '\0';

                        UserEntry *current = head;
                        while (current) {
                            if (strcmp(current->username, username) == 0) {
                                free(username);
                                username = NULL;
                                break;
                            }
                            current = current->next;
                        }
                        if (username) {
                            UserEntry *new_entry = malloc(sizeof(UserEntry));
                            new_entry->username = username;
                            new_entry->next = head;
                            head = new_entry;
                            count++;
                        }
                    }
                }
            }
            dprintf(out_fd, "USER_LOGIN:%d\n", count);
            while (head) {
                UserEntry *next = head->next;
                free(head->username);
                free(head);
                head = next;
            }
            break;
        }
        case SYSTEM_BOOT: {
            int count = 0;
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(in_fd, buffer, BUFFER_SIZE)) > 0) {
                count++;
            }
            dprintf(out_fd, "SYSTEM_BOOT:%d\n", count);
            break;
        }
        default:
            exit(EXIT_FAILURE);
    }
    close(in_fd);
    close(out_fd);
    exit(EXIT_SUCCESS);
}

int main() {
    int input_pipes[NUM_CHILDREN][2];
    int output_pipes[NUM_CHILDREN][2];
    pid_t pids[NUM_CHILDREN];

    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(input_pipes[i]) == -1 || pipe(output_pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pids[i] == 0) {
            for (int j = 0; j < NUM_CHILDREN; j++) {
                if (j != i) {
                    close(input_pipes[j][0]);
                    close(input_pipes[j][1]);
                    close(output_pipes[j][0]);
                    close(output_pipes[j][1]);
                }
            }
            close(input_pipes[i][1]);
            close(output_pipes[i][0]);
            child_process(i, input_pipes[i][0], output_pipes[i][1]);
        } else {
            close(input_pipes[i][0]);
            close(output_pipes[i][1]);
        }
    }

    FILE *file = fopen("events_log.txt", "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    int event_counts[NUM_CHILDREN] = {0};
    while (fgets(line, sizeof(line), file)) {
        char *event_str = strtok(line, " ");
        event_str = strtok(NULL, " ");
        EventType type = get_event_type(event_str);
        if (type == UNKNOWN) continue;

        dprintf(input_pipes[type][1], "%s", line);
        event_counts[type]++;
    }
    fclose(file);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(input_pipes[i][1]);
    }

    int process_create = 0;
    long memory_alloc = 0;
    char file_path[BUFFER_SIZE] = "";
    int file_count = 0;
    int user_login = 0;
    int system_boot = 0;

    for (int i = 0; i < NUM_CHILDREN; i++) {
        char buffer[BUFFER_SIZE] = "";
        ssize_t bytes_read = read(output_pipes[i][0], buffer, BUFFER_SIZE);
        if (bytes_read <= 0) continue;

        switch (i) {
            case PROCESS_CREATE:
                sscanf(buffer, "PROCESS_CREATE:%d", &process_create);
                break;
            case MEMORY_ALLOC:
                sscanf(buffer, "MEMORY_ALLOC:%ld", &memory_alloc);
                break;
            case FILE_OPEN:
                sscanf(buffer, "FILE_OPEN:%[^:]:%d", file_path, &file_count);
                break;
            case USER_LOGIN:
                sscanf(buffer, "USER_LOGIN:%d", &user_login);
                break;
            case SYSTEM_BOOT:
                sscanf(buffer, "SYSTEM_BOOT:%d", &system_boot);
                break;
        }
        close(output_pipes[i][0]);
    }

    int total_events = 0;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        total_events += event_counts[i];
    }

    const char *event_names[] = {"PROCESS_CREATE", "MEMORY_ALLOC", "FILE_OPEN", "USER_LOGIN", "SYSTEM_BOOT"};
    int max_count = 0;
    const char *most_common = "";
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (event_counts[i] > max_count) {
            max_count = event_counts[i];
            most_common = event_names[i];
        }
    }

    printf("PROCESS_CREATE events: %d\n", process_create);
    printf("MEMORY_ALLOC events: Total memory allocated: %ld bytes\n", memory_alloc);
    printf("FILE_OPEN events: Most accessed file: \"%s\" (%d times)\n", file_path, file_count);
    printf("USER_LOGIN events: Number of unique users: %d\n", user_login);
    printf("SYSTEM_BOOT events: Number of system boots: %d\n\n", system_boot);
    printf("Overall Statistics:\n");
    printf("Total events processed: %d\n", total_events);
    printf("Most common event type: %s (%d occurrences)\n", most_common, max_count);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}
