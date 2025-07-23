#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>

// ANSI color codes for better visualization
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"

// Configuration constants
#define MAX_PATH_LENGTH 256
#define MAX_LINE_LENGTH 1024
#define DEFAULT_REFRESH_RATE 1
#define PROGRESS_BAR_WIDTH 50

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

// Data structures
typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    unsigned long long prev_user, prev_nice, prev_system, prev_idle;
    unsigned long long prev_iowait, prev_irq, prev_softirq, prev_steal;
    double usage_percent;
} cpu_stats_t;

typedef struct {
    unsigned long total;
    unsigned long available;
    unsigned long used;
    unsigned long free;
    unsigned long buffers;
    unsigned long cached;
    double usage_percent;
} memory_stats_t;

typedef struct {
    char mount_point[MAX_PATH_LENGTH];
    unsigned long long total;
    unsigned long long used;
    unsigned long long available;
    double usage_percent;
} disk_stats_t;

typedef struct {
    int refresh_rate;
    int show_cpu;
    int show_memory;
    int show_disk;
    char disk_path[MAX_PATH_LENGTH];
    int continuous;
} config_t;

// Function declarations
void signal_handler(int signum);
void print_usage(const char* program_name);
void parse_arguments(int argc, char* argv[], config_t* config);
int read_cpu_stats(cpu_stats_t* stats);
int read_memory_stats(memory_stats_t* stats);
int read_disk_stats(const char* path, disk_stats_t* stats);
void calculate_cpu_usage(cpu_stats_t* stats);
void print_progress_bar(const char* label, double percentage, const char* color);
void print_system_info(const cpu_stats_t* cpu, const memory_stats_t* memory, 
                      const disk_stats_t* disk, const config_t* config);
void clear_screen(void);

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    running = 0;
}

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Linux System Monitoring Tool\n\n");
    printf("Options:\n");
    printf("  -r, --refresh SECONDS    Refresh rate in seconds (default: 1)\n");
    printf("  -c, --cpu                Show CPU usage only\n");
    printf("  -m, --memory             Show memory usage only\n");
    printf("  -d, --disk PATH          Show disk usage for specified path (default: /)\n");
    printf("  -o, --once               Run once and exit (no continuous monitoring)\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                       Monitor all resources with 1s refresh\n", program_name);
    printf("  %s -r 2 -c               Monitor CPU only with 2s refresh\n", program_name);
    printf("  %s -d /home -o           Show disk usage for /home once\n", program_name);
}

// Parse command line arguments
void parse_arguments(int argc, char* argv[], config_t* config) {
    // Initialize default configuration
    config->refresh_rate = DEFAULT_REFRESH_RATE;
    config->show_cpu = 1;
    config->show_memory = 1;
    config->show_disk = 1;
    strcpy(config->disk_path, "/");
    config->continuous = 1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--refresh") == 0) {
            if (i + 1 < argc) {
                config->refresh_rate = atoi(argv[++i]);
                if (config->refresh_rate <= 0) {
                    fprintf(stderr, "Error: Refresh rate must be positive\n");
                    exit(1);
                }
            } else {
                fprintf(stderr, "Error: --refresh requires a value\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cpu") == 0) {
            config->show_memory = 0;
            config->show_disk = 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) {
            config->show_cpu = 0;
            config->show_disk = 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--disk") == 0) {
            config->show_cpu = 0;
            config->show_memory = 0;
            if (i + 1 < argc) {
                strncpy(config->disk_path, argv[++i], MAX_PATH_LENGTH - 1);
                config->disk_path[MAX_PATH_LENGTH - 1] = '\0';
            } else {
                fprintf(stderr, "Error: --disk requires a path\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--once") == 0) {
            config->continuous = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            exit(1);
        }
    }
}

int main(int argc, char* argv[]) {
    config_t config;
    cpu_stats_t cpu_stats = {0};
    memory_stats_t memory_stats = {0};
    disk_stats_t disk_stats = {0};
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    parse_arguments(argc, argv, &config);
    
    printf("%s%sLinux System Monitoring Tool%s\n", BOLD, CYAN, RESET);
    printf("Press Ctrl+C to exit\n\n");
    
    // Initial CPU reading (needed for calculating usage percentage)
    if (config.show_cpu) {
        read_cpu_stats(&cpu_stats);
        sleep(1);  // Wait 1 second for initial measurement
    }
    
    do {
        // Read current stats
        if (config.show_cpu && read_cpu_stats(&cpu_stats) != 0) {
            fprintf(stderr, "Error reading CPU stats\n");
            continue;
        }
        
        if (config.show_memory && read_memory_stats(&memory_stats) != 0) {
            fprintf(stderr, "Error reading memory stats\n");
            continue;
        }
        
        if (config.show_disk && read_disk_stats(config.disk_path, &disk_stats) != 0) {
            fprintf(stderr, "Error reading disk stats for %s\n", config.disk_path);
            continue;
        }
        
        // Calculate usage percentages
        if (config.show_cpu) {
            calculate_cpu_usage(&cpu_stats);
        }
        
        // Display results
        if (config.continuous) {
            clear_screen();
            printf("%s%sLinux System Monitoring Tool%s\n", BOLD, CYAN, RESET);
            printf("Press Ctrl+C to exit | Refresh rate: %ds\n\n", config.refresh_rate);
        }
        
        print_system_info(&cpu_stats, &memory_stats, &disk_stats, &config);
        
        if (config.continuous) {
            sleep(config.refresh_rate);
        }
        
    } while (config.continuous && running);
    
    printf("\n%s%sMonitoring stopped.%s\n", BOLD, GREEN, RESET);
    return 0;
} 