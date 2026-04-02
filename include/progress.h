#ifndef VERDIFF_PROGRESS_H
#define VERDIFF_PROGRESS_H

#include "common.h"

typedef enum {
    PROGRESS_PHASE_IDLE = 0,
    PROGRESS_PHASE_SCAN_A = 1,
    PROGRESS_PHASE_SCAN_B = 2,
    PROGRESS_PHASE_PLAN = 3,
    PROGRESS_PHASE_COMPARE = 4,
    PROGRESS_PHASE_WRITE = 5,
    PROGRESS_PHASE_DONE = 6
} ProgressPhase;

struct ProgressState {
    pthread_mutex_t mutex;
    bool initialized;
    bool interactive;
    ProgressPhase phase;
    size_t scanned_a;
    size_t scanned_b;
    size_t compared;
    size_t last_compared_snapshot;
    char current_path[160];
    struct timespec last_emit;
};

int progress_init(ProgressState *state);
void progress_destroy(ProgressState *state);
void progress_phase_begin(ProgressState *state, ProgressPhase phase, const char *message);
void progress_note_scan(ProgressState *state, ProgressPhase phase, const char *path);
void progress_note_compare_start(ProgressState *state, const char *path);
void progress_note_compare_done(ProgressState *state);
void progress_phase_end(ProgressState *state, const char *message);

#endif
