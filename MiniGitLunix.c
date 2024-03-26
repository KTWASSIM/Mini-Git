#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h> // For PATH_MAX
#include <unistd.h> // For unlink

#define MAX_FILENAME 256

extern char SNAPSHOT_FILE[]; // Declare SNAPSHOT_FILE as external
char SNAPSHOT_FILE[] = "snapshots.dat"; // Define SNAPSHOT_FILE content

// Structure to store snapshot information
typedef struct Snapshot {
    char filename[MAX_FILENAME];
    char timestamp[20]; // Format: YYYYMMDD_HHMMSS
    struct Snapshot *next;
} Snapshot;

// Global variables (outside of any function)
Snapshot *head = NULL;

// Function prototypes
char* hashToPath(const char* hash);
int directoryExists(const char* path);
int makeDirectories(const char* path);
int fileExists(const char* filename);
void addSnapshot(const char* filename, const char* timestamp);
void createSnapshot(const char* file);
void viewSnapshots(const char* file);
void restoreSnapshot(const char* snapshot);
int getValidInput(int min, int max);
void getFilenameInput(char* filename);

// Function to generate the path of a file based on its hash
char* hashToPath(const char* hash) {
    char* path = (char*)malloc(strlen(hash) + 3); // Add 2 for '/' and '\0'
    if (path == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    snprintf(path, strlen(hash) + 3, "%c%c/%s", hash[0], hash[1], hash + 2);
    return path;
}

// Function to check if a directory exists
int directoryExists(const char* path) {
    struct stat info;
    if (stat(path, &info) != 0) {
        return 0; // Directory doesn't exist
    }
    return S_ISDIR(info.st_mode);
}

// Function to create directories recursively
int makeDirectories(const char* path) {
    char command[PATH_MAX + 10]; // +10 for "mkdir -p "
    snprintf(command, sizeof(command), "mkdir -p \"%s\"", path);
    return system(command);
}

// Function to check if a file exists
int fileExists(const char* filename) {
    struct stat buffer;
    return stat(filename, &buffer) == 0;
}

// Function to add a snapshot to the linked list
void addSnapshot(const char* filename, const char* timestamp) {
    Snapshot *newSnapshot = (Snapshot *)malloc(sizeof(Snapshot));
    if (newSnapshot == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    strcpy(newSnapshot->filename, filename);
    strcpy(newSnapshot->timestamp, timestamp);
    newSnapshot->next = head;
    head = newSnapshot;
}

char *realpath(const char *path, char *resolved_path) {
    char *ptr;

    // Get absolute path if path is relative
    if (path[0] != '/') {
        if (getcwd(resolved_path, PATH_MAX) == NULL) {
            perror("getcwd");
            return NULL;
        }
        strcat(resolved_path, "/");
        strcat(resolved_path, path);
    } else {
        strcpy(resolved_path, path);
    }

    // Resolve symbolic links
    while ((ptr = strstr(resolved_path, "/./")) != NULL) {
        strcpy(ptr, ptr + 2);
    }

    ptr = resolved_path;
    while ((ptr = strstr(ptr, "/../")) != NULL) {
        char *slash = strrchr(resolved_path, '/');
        if (slash != NULL) {
            *slash = '\0';
        }
        strcpy(ptr, ptr + 3);
    }

    return resolved_path;
}

// Function to create a snapshot
void createSnapshot(const char* file) {
    // Get directory path of the file
    char dir_path[PATH_MAX];
    realpath(file, dir_path);
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash == NULL) {
        perror("Error getting directory path");
        exit(EXIT_FAILURE);
    }
    *last_slash = '\0'; // Null-terminate to get directory path

    // Get current time
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Format date and time
    char timestamp[20]; // Format: YYYYMMDD_HHMMSS
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

    // Create new filename with timestamp
    char new_filename[MAX_FILENAME];
    snprintf(new_filename, sizeof(new_filename),"%s_%s", timestamp, file);

    // Copy file to directory with new filename
    char command[512];
    snprintf(command, sizeof(command), "cp \"%s\" \"%s\"", file, new_filename);
    if (system(command) == -1) {
        perror("Error copying file");
        exit(EXIT_FAILURE);
    }

    // Add snapshot to linked list
    addSnapshot(new_filename, timestamp);

    printf("Snapshot of \"%s\" saved as \"%s\"\n", file,  new_filename);
}

char* extractDateFromFilename(const char* filename) {
    static char datetime[23]; // Format: DDMMYYYY HHMMSS
    snprintf(datetime, sizeof(datetime), "%c%c-%c%c-%c%c%c%c at %c%c:%c%c:%c%c",
        filename[6], filename[7], filename[4], filename[5], filename[0], filename[1], filename[2], filename[3],
        filename[9], filename[10], filename[11], filename[12], filename[13], filename[14]);
    return datetime;
}

// Function to list available snapshots for a file
void viewSnapshots(const char* file) {
    // Traverse linked list and print snapshots
    Snapshot *current = head;
    int found_snapshots = 0;

    printf("Available snapshots for \"%s\":\n", file);
    while (current != NULL) {
        if (strstr(current->filename, file) != NULL) {
            char* snapshot_date = extractDateFromFilename(current->filename);
            printf("\t- Snapshot: %s, Date: %s\n", current->filename, snapshot_date);
            found_snapshots = 1;
        }
        current = current->next;
    }

    if (!found_snapshots) {
        printf(" - No snapshots found.\n");
    }
}

// Function to restore a snapshot of a file
void restoreSnapshot(const char* snapshot) {
    // Check if the snapshot file exists
    if (!fileExists(snapshot)) {
        printf("Error: Snapshot file '%s' not found.\n", snapshot);
        return;
    }

    // Extract the filename from the snapshot path
    char* last_underscore = strrchr(snapshot, '_');
    if (last_underscore == NULL) {
        printf("Error: Invalid snapshot path.\n");
        return;
    }
    char filename[MAX_FILENAME];
    strcpy(filename, last_underscore + 1);

    // Copy the snapshot file to the current directory with the original filename
    char command[MAX_FILENAME];
    snprintf(command, sizeof(command), "cp \"%s\" \"%s\"", snapshot, filename);
    if (system(command) == -1) {
        perror("Error restoring snapshot");
        return;
    }

    printf("Snapshot '%s' restored as '%s'.\n", snapshot, filename);
}

// Function to get valid integer input within a specific range
int getValidInput(int min, int max) {
    int choice;
    while (scanf("%d", &choice) != 1 || choice < min || choice > max) {
        printf("Invalid input. Please enter a number between %d and %d: ", min, max);
        scanf("%*[^\n]"); // Clear input buffer
    }
    scanf("%*[^\n]"); // Clear remaining newline character (if any)
    return choice;
}

// Function to get user input for filename (handles spaces)
void getFilenameInput(char* filename) {
    printf("Enter the filename: ");
    scanf("%s", filename);
}

// Function to load snapshot information from a file
int loadSnapshotsFromFile() {
    FILE *fp = fopen(SNAPSHOT_FILE, "rb");
    if (fp == NULL) {
        return 0; // No snapshot file found (not an error)
    }

    while (!feof(fp)) {
        Snapshot *newSnapshot = (Snapshot *)malloc(sizeof(Snapshot));
        if (newSnapshot == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }

        fread(newSnapshot, sizeof(Snapshot), 1, fp);
        newSnapshot->next = head;
        head = newSnapshot;
    }

    fclose(fp);
    return 1; // Snapshots loaded successfully
}

// Function to save snapshot information to a file
int saveSnapshotsToFile() {
    FILE *fp = fopen(SNAPSHOT_FILE, "wb");
    if (fp == NULL) {
        perror("Error opening snapshot file");
        return 0;
    }

    Snapshot *current = head;
    while (current != NULL) {
        fwrite(current, sizeof(Snapshot), 1, fp);
        current = current->next;
    }

    fclose(fp);
    return 1; // Snapshots saved successfully
}

void deleteSnapshot(const char* snapshot) {
    // Check if the snapshot file exists
    if (!fileExists(snapshot)) {
        printf("Error: Snapshot file '%s' not found.\n", snapshot);
        return;
    }

    // Remove the snapshot from the linked list
    Snapshot *current = head;
    Snapshot *prev = NULL;

    while (current != NULL && strcmp(current->filename, snapshot) != 0) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        printf("Snapshot '%s' not found.\n", snapshot);
        return;
    }

    if (prev == NULL) {
        head = current->next;
    } else {
        prev->next = current->next;
    }

    // Delete the snapshot file
    if (unlink(snapshot) != 0) {
        perror("Error deleting snapshot file");
        return;
    }

    free(current);
    printf("Snapshot '%s' deleted successfully.\n", snapshot);
}

int main() {
    char filename[MAX_FILENAME];
    int choice;

    // Load snapshots from file on program startup
    loadSnapshotsFromFile();

    do {
        printf("\n");
        printf("Mini Git:\n");
        printf("1. Create a snapshot\n");
        printf("2. View snapshots\n");
        printf("3. Restore a snapshot\n");
        printf("4. Delete a snapshot\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");
        choice = getValidInput(1, 5);

        switch (choice) {
            case 1:
                getFilenameInput(filename);
                createSnapshot(filename);
                saveSnapshotsToFile();
                break;
            case 2:
                getFilenameInput(filename);
                viewSnapshots(filename);
                break;
            case 3:
                getFilenameInput(filename);
                viewSnapshots(filename);
                printf("Enter the name of the snapshot to restore: ");
                scanf("%s", filename);
                restoreSnapshot(filename);
                break;
            case 4:
                getFilenameInput(filename);
                viewSnapshots(filename);
                printf("Enter the name of the snapshot to delete: ");
                scanf("%s", filename);
                deleteSnapshot(filename);
                saveSnapshotsToFile();
                break;
            case 5:
                printf("Exiting Mini Git.\n");
                // Save snapshots to file before exiting
                saveSnapshotsToFile();
                break;
            default:
                printf("Invalid choice.\n");
        }
    } while (choice != 5);

    return 0;
}
