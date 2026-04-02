#include "compare.h"
#include "index.h"
#include "output.h"
#include "progress.h"
#include "scanner.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *default_report_path = "verdiff_report.txt";

static void usage(FILE *stream) {
    fprintf(stream, "Usage: verdiff [--thread N|--threads N|-j N] [--lines] [--skip-unchanged] [-o file] DIR_A DIR_B\n");
}

static size_t default_thread_count(void) {
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online < 1) {
        return 4;
    }
    return (size_t)online;
}

/*
 * main: The Grand Orchestrator.
 * Parses your terribly formatted command-line arguments, provisions the index maps, 
 * orchestrates the fancy ANSI progress updates, kicks off the thread pools, 
 * retrieves the verdicts, and cleanly writes out the final aggregated report
 * before gracefully bowing out.
 */
int main(int argc, char **argv) {
    Config config = {
        .thread_count = default_thread_count(),
        .include_lines = false,
        .include_unchanged = true,
        .hash_buffer_size = 1U << 20U,
        .mmap_threshold = 16U << 20U,
        .verify_equal_hashes = true,
    };

    int argi = 1;
    while (argi < argc) {
        if (strcmp(argv[argi], "--lines") == 0) {
            config.include_lines = true;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--skip-unchanged") == 0) {
            config.include_unchanged = false;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "-j") == 0 ||
            strcmp(argv[argi], "--thread") == 0 ||
            strcmp(argv[argi], "--threads") == 0) {
            if (argi + 1 >= argc) {
                usage(stderr);
                return 2;
            }
            config.thread_count = (size_t)strtoul(argv[argi + 1], NULL, 10);
            if (config.thread_count == 0) {
                fprintf(stderr, "invalid thread count\n");
                return 2;
            }
            argi += 2;
            continue;
        }
        if (strcmp(argv[argi], "-o") == 0) {
            if (argi + 1 >= argc) {
                usage(stderr);
                return 2;
            }
            config.output_path = argv[argi + 1];
            argi += 2;
            continue;
        }
        break;
    }

    if (argc - argi != 2) {
        usage(stderr);
        return 2;
    }

    config.root_a = argv[argi];
    config.root_b = argv[argi + 1];
    if (config.output_path == NULL) {
        config.output_path = default_report_path;
    }

    FileIndex index_a = {0};
    FileIndex index_b = {0};
    ResultSet results = {0};
    ProgressState progress = {0};
    RunStats stats = {0};
    bool results_ready = false;
    int rc = file_index_init(&index_a, 1024);
    if (rc == 0) {
        rc = file_index_init(&index_b, 1024);
    }
    if (rc == 0) {
        rc = result_set_init(&results);
        results_ready = (rc == 0);
    }
    if (rc == 0) {
        rc = progress_init(&progress);
    }
    if (rc == 0) {
        progress_phase_begin(&progress, PROGRESS_PHASE_SCAN_A, "Reading source directory...");
        rc = scan_directory(config.root_a, &index_a, &progress, PROGRESS_PHASE_SCAN_A);
        progress_phase_end(&progress, "Finished reading source directory.");
    }
    if (rc == 0) {
        progress_phase_begin(&progress, PROGRESS_PHASE_SCAN_B, "Reading target directory...");
        rc = scan_directory(config.root_b, &index_b, &progress, PROGRESS_PHASE_SCAN_B);
        progress_phase_end(&progress, "Finished reading target directory.");
    }
    if (rc == 0) {
        progress_phase_begin(&progress, PROGRESS_PHASE_PLAN, "Building comparison plan...");
        rc = plan_and_compare(&config, &index_a, &index_b, &results, &progress);
        progress_phase_end(&progress, "Finished comparing files.");
    }
    if (rc == 0) {
        stats.files_in_a = index_a.count;
        stats.files_in_b = index_b.count;
        stats.total_files_scanned = index_a.count + results.added_count;
        stats.scan_time = time(NULL);
    }

    if (rc == 0) {
        progress_phase_begin(&progress, PROGRESS_PHASE_WRITE, "Writing detailed report...");
        FILE *detail = fopen(config.output_path, "wb");
        if (detail == NULL) {
            rc = errno;
        }
        if (rc == 0 && write_detailed_report(detail, &config, &stats, &results) != 0) {
            rc = errno != 0 ? errno : EIO;
        }
        if (detail != NULL) {
            fclose(detail);
        }
        progress_phase_end(&progress, "Detailed report written.");
    }

    if (rc == 0) {
        if (write_summary(stdout, &config, &stats, &results, config.output_path) != 0) {
            rc = errno != 0 ? errno : EIO;
        }
    }

    if (rc != 0) {
        fprintf(stderr, "verdiff: %s\n", strerror(rc));
    }

    if (results_ready) {
        result_set_destroy(&results);
    }
    progress_destroy(&progress);
    file_index_destroy(&index_a);
    file_index_destroy(&index_b);
    return rc == 0 ? 0 : 1;
}
