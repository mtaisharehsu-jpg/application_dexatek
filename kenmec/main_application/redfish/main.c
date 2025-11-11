#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "include/redfish_init.h"
#include "include/config.h"

// Global flag for graceful shutdown
volatile int running = 1;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down gracefully...\n", signum);
    running = 0;
}

// Forward declaration - print_usage is defined in redfish_init.c
void print_usage(const char *program_name);

void print_version() {
    printf("Redfish Server version %s\n", REDFISH_VERSION);
    printf("Protocol version %s\n", REDFISH_PROTOCOL_VERSION);
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("=== Redfish Server Starting ===\n");
    print_version();
    printf("================================\n");
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // Termination request
    
    // Run as daemon if requested
    if (daemon_mode) {
        printf("Running in daemon mode...\n");
        
        // Fork the process
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            return 1;
        }
        
        // Exit the parent process
        if (pid > 0) {
            printf("Daemon started with PID: %d\n", pid);
            return 0;
        }
        
        // Create new session for the child process
        if (setsid() < 0) {
            perror("setsid failed");
            return 1;
        }
        
        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    
    // Initialize Redfish server
    printf("Initializing Redfish server...\n");
    int ret = redfish_init();
    if (ret != SUCCESS) {
        fprintf(stderr, "Failed to initialize Redfish server: %d\n", ret);
        return 1;
    }
    
    printf("Redfish server initialized successfully!\n");
    printf("Server is running. Press Ctrl+C to stop.\n");
    
    // Main loop - keep the program running
    while (running) {
        sleep(1);
    }
    
    // Cleanup
    printf("Shutting down Redfish server...\n");
    ret = redfish_deinit();
    if (ret != SUCCESS) {
        fprintf(stderr, "Error during cleanup: %d\n", ret);
    }
    
    printf("Redfish server stopped.\n");
    return 0;
}
