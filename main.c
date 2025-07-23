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

// Read CPU statistics from /proc/stat
int read_cpu_stats(cpu_stats_t* stats) {
    FILE* file = fopen("/proc/stat", "r");
    if (!file) {
        perror("Error opening /proc/stat");
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        fprintf(stderr, "Error reading from /proc/stat\n");
        return -1;
    }
    
    fclose(file);
    
    // Store previous values
    stats->prev_user = stats->user;
    stats->prev_nice = stats->nice;
    stats->prev_system = stats->system;
    stats->prev_idle = stats->idle;
    stats->prev_iowait = stats->iowait;
    stats->prev_irq = stats->irq;
    stats->prev_softirq = stats->softirq;
    stats->prev_steal = stats->steal;
    
    // Parse current values
    int result = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &stats->user, &stats->nice, &stats->system, &stats->idle,
                       &stats->iowait, &stats->irq, &stats->softirq, &stats->steal);
    
    if (result < 4) {
        fprintf(stderr, "Error parsing /proc/stat\n");
        return -1;
    }
    
    return 0;
}

// Calculate CPU usage percentage
void calculate_cpu_usage(cpu_stats_t* stats) {
    unsigned long long prev_total = stats->prev_user + stats->prev_nice + 
                                   stats->prev_system + stats->prev_idle + 
                                   stats->prev_iowait + stats->prev_irq + 
                                   stats->prev_softirq + stats->prev_steal;
    
    unsigned long long curr_total = stats->user + stats->nice + 
                                   stats->system + stats->idle + 
                                   stats->iowait + stats->irq + 
                                   stats->softirq + stats->steal;
    
    unsigned long long prev_idle = stats->prev_idle + stats->prev_iowait;
    unsigned long long curr_idle = stats->idle + stats->iowait;
    
    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;
    
    if (total_diff == 0) {
        stats->usage_percent = 0.0;
    } else {
        stats->usage_percent = (double)(total_diff - idle_diff) / total_diff * 100.0;
    }
    
    // Ensure percentage is within valid range
    if (stats->usage_percent < 0.0) stats->usage_percent = 0.0;
    if (stats->usage_percent > 100.0) stats->usage_percent = 100.0;
}

// Read memory statistics from /proc/meminfo
int read_memory_stats(memory_stats_t* stats) {
    FILE* file = fopen("/proc/meminfo", "r");
    if (!file) {
        perror("Error opening /proc/meminfo");
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    char key[64];
    unsigned long value;
    
    // Initialize values
    stats->total = 0;
    stats->available = 0;
    stats->free = 0;
    stats->buffers = 0;
    stats->cached = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%63s %lu kB", key, &value) == 2) {
            if (strcmp(key, "MemTotal:") == 0) {
                stats->total = value;
            } else if (strcmp(key, "MemAvailable:") == 0) {
                stats->available = value;
            } else if (strcmp(key, "MemFree:") == 0) {
                stats->free = value;
            } else if (strcmp(key, "Buffers:") == 0) {
                stats->buffers = value;
            } else if (strcmp(key, "Cached:") == 0) {
                stats->cached = value;
            }
        }
    }
    
    fclose(file);
    
    // Calculate used memory and usage percentage
    if (stats->available > 0) {
        stats->used = stats->total - stats->available;
    } else {
        // Fallback calculation if MemAvailable is not present
        stats->used = stats->total - stats->free - stats->buffers - stats->cached;
        stats->available = stats->free + stats->buffers + stats->cached;
    }
    
    if (stats->total > 0) {
        stats->usage_percent = (double)stats->used / stats->total * 100.0;
    } else {
        stats->usage_percent = 0.0;
    }
    
    // Ensure percentage is within valid range
    if (stats->usage_percent < 0.0) stats->usage_percent = 0.0;
    if (stats->usage_percent > 100.0) stats->usage_percent = 100.0;
    
    return 0;
}

// Read disk statistics using statvfs
int read_disk_stats(const char* path, disk_stats_t* stats) {
    struct statvfs vfs;
    
    if (statvfs(path, &vfs) != 0) {
        perror("Error getting disk statistics");
        return -1;
    }
    
    // Copy mount point
    strncpy(stats->mount_point, path, MAX_PATH_LENGTH - 1);
    stats->mount_point[MAX_PATH_LENGTH - 1] = '\0';
    
    // Calculate disk usage in bytes
    stats->total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
    stats->available = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
    stats->used = stats->total - ((unsigned long long)vfs.f_bfree * vfs.f_frsize);
    
    // Calculate usage percentage
    if (stats->total > 0) {
        stats->usage_percent = (double)stats->used / stats->total * 100.0;
    } else {
        stats->usage_percent = 0.0;
    }
    
    // Ensure percentage is within valid range
    if (stats->usage_percent < 0.0) stats->usage_percent = 0.0;
    if (stats->usage_percent > 100.0) stats->usage_percent = 100.0;
    
    return 0;
}

// Clear terminal screen
void clear_screen(void) {
    printf("\033[2J\033[H");
}

// Print a progress bar with percentage
void print_progress_bar(const char* label, double percentage, const char* color) {
    int filled = (int)(percentage * PROGRESS_BAR_WIDTH / 100.0);
    int empty = PROGRESS_BAR_WIDTH - filled;
    
    printf("%s%-12s%s [", BOLD, label, RESET);
    
    // Print filled portion
    printf("%s", color);
    for (int i = 0; i < filled; i++) {
        printf("█");
    }
    
    // Print empty portion
    printf("%s", RESET);
    for (int i = 0; i < empty; i++) {
        printf("░");
    }
    
    printf("] %s%.1f%%%s\n", BOLD, percentage, RESET);
}

// Convert bytes to human readable format
void format_bytes(unsigned long long bytes, char* buffer, size_t buffer_size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    if (unit_index == 0) {
        snprintf(buffer, buffer_size, "%llu %s", bytes, units[unit_index]);
    } else {
        snprintf(buffer, buffer_size, "%.1f %s", size, units[unit_index]);
    }
}

// Print comprehensive system information
void print_system_info(const cpu_stats_t* cpu, const memory_stats_t* memory, 
                      const disk_stats_t* disk, const config_t* config) {
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    printf("%s%sSystem Status - %s%s\n", BOLD, WHITE, time_str, RESET);
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    // CPU Information
    if (config->show_cpu) {
        printf("%s%sCPU Usage:%s\n", BOLD, CYAN, RESET);
        
        const char* cpu_color = GREEN;
        if (cpu->usage_percent > 80.0) cpu_color = RED;
        else if (cpu->usage_percent > 60.0) cpu_color = YELLOW;
        
        print_progress_bar("CPU", cpu->usage_percent, cpu_color);
        
        printf("  %sDetails:%s User: %.1f%%, System: %.1f%%, Idle: %.1f%%\n\n",
               BOLD, RESET,
               (double)(cpu->user - cpu->prev_user) / 
               (double)((cpu->user + cpu->nice + cpu->system + cpu->idle + 
                        cpu->iowait + cpu->irq + cpu->softirq + cpu->steal) - 
                       (cpu->prev_user + cpu->prev_nice + cpu->prev_system + 
                        cpu->prev_idle + cpu->prev_iowait + cpu->prev_irq + 
                        cpu->prev_softirq + cpu->prev_steal)) * 100.0,
               (double)(cpu->system - cpu->prev_system) / 
               (double)((cpu->user + cpu->nice + cpu->system + cpu->idle + 
                        cpu->iowait + cpu->irq + cpu->softirq + cpu->steal) - 
                       (cpu->prev_user + cpu->prev_nice + cpu->prev_system + 
                        cpu->prev_idle + cpu->prev_iowait + cpu->prev_irq + 
                        cpu->prev_softirq + cpu->prev_steal)) * 100.0,
               (double)(cpu->idle - cpu->prev_idle) / 
               (double)((cpu->user + cpu->nice + cpu->system + cpu->idle + 
                        cpu->iowait + cpu->irq + cpu->softirq + cpu->steal) - 
                       (cpu->prev_user + cpu->prev_nice + cpu->prev_system + 
                        cpu->prev_idle + cpu->prev_iowait + cpu->prev_irq + 
                        cpu->prev_softirq + cpu->prev_steal)) * 100.0);
    }
    
    // Memory Information
    if (config->show_memory) {
        printf("%s%sMemory Usage:%s\n", BOLD, MAGENTA, RESET);
        
        const char* mem_color = GREEN;
        if (memory->usage_percent > 90.0) mem_color = RED;
        else if (memory->usage_percent > 75.0) mem_color = YELLOW;
        
        print_progress_bar("Memory", memory->usage_percent, mem_color);
        
        char total_str[32], used_str[32], available_str[32];
        format_bytes((unsigned long long)memory->total * 1024, total_str, sizeof(total_str));
        format_bytes((unsigned long long)memory->used * 1024, used_str, sizeof(used_str));
        format_bytes((unsigned long long)memory->available * 1024, available_str, sizeof(available_str));
        
        printf("  %sDetails:%s Used: %s, Available: %s, Total: %s\n",
               BOLD, RESET, used_str, available_str, total_str);
        
        char buffers_str[32], cached_str[32];
        format_bytes((unsigned long long)memory->buffers * 1024, buffers_str, sizeof(buffers_str));
        format_bytes((unsigned long long)memory->cached * 1024, cached_str, sizeof(cached_str));
        
        printf("  %sCaching:%s Buffers: %s, Cached: %s\n\n", BOLD, RESET, buffers_str, cached_str);
    }
    
    // Disk Information
    if (config->show_disk) {
        printf("%s%sDisk Usage (%s):%s\n", BOLD, YELLOW, disk->mount_point, RESET);
        
        const char* disk_color = GREEN;
        if (disk->usage_percent > 90.0) disk_color = RED;
        else if (disk->usage_percent > 80.0) disk_color = YELLOW;
        
        print_progress_bar("Disk", disk->usage_percent, disk_color);
        
        char total_str[32], used_str[32], available_str[32];
        format_bytes(disk->total, total_str, sizeof(total_str));
        format_bytes(disk->used, used_str, sizeof(used_str));
        format_bytes(disk->available, available_str, sizeof(available_str));
        
        printf("  %sDetails:%s Used: %s, Available: %s, Total: %s\n\n",
               BOLD, RESET, used_str, available_str, total_str);
    }
    
    printf("═══════════════════════════════════════════════════════════\n");
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