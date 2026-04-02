#include "progress.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static long diff_millis(const struct timespec *lhs, const struct timespec *rhs) {
    long sec = lhs->tv_sec - rhs->tv_sec;
    long nsec = lhs->tv_nsec - rhs->tv_nsec;
    return sec * 1000L + nsec / 1000000L;
}

static const char *phase_name(ProgressPhase phase) {
    switch (phase) {
        case PROGRESS_PHASE_SCAN_A:
            return "INFO";
        case PROGRESS_PHASE_SCAN_B:
            return "INFO";
        case PROGRESS_PHASE_PLAN:
            return "INFO";
        case PROGRESS_PHASE_COMPARE:
            return "INFO";
        case PROGRESS_PHASE_WRITE:
            return "INFO";
        case PROGRESS_PHASE_DONE:
            return "INFO";
        case PROGRESS_PHASE_IDLE:
            break;
    }
    return "INFO";
}

static void clip_path(const char *src, char *dest, size_t dest_size) {
    size_t len = strlen(src);
    if (len + 1U <= dest_size) {
        memcpy(dest, src, len + 1U);
        return;
    }
    if (dest_size <= 4U) {
        dest[0] = '\0';
        return;
    }
    size_t keep = dest_size - 4U;
    memcpy(dest, src, keep);
    memcpy(dest + keep, "...", 4U);
}

static void emit_locked(ProgressState *state, bool force) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!force && diff_millis(&now, &state->last_emit) < 200L) {
        return;
    }
    state->last_emit = now;

    char line[320];
    if (state->phase == PROGRESS_PHASE_COMPARE) {
        snprintf(
            line,
            sizeof(line),
            "[%s] Comparing files: scanned_source=%zu scanned_target=%zu compared=%zu current=%s",
            phase_name(state->phase),
            state->scanned_a,
            state->scanned_b,
            state->compared,
            state->current_path[0] == '\0' ? "-" : state->current_path
        );
    } else {
        snprintf(
            line,
            sizeof(line),
            "[%s] Reading files: scanned_source=%zu scanned_target=%zu current=%s",
            phase_name(state->phase),
            state->scanned_a,
            state->scanned_b,
            state->current_path[0] == '\0' ? "-" : state->current_path
        );
    }

    if (state->interactive) {
        fprintf(stderr, "\r%-120s", line);
    } else {
        fprintf(stderr, "%s\n", line);
    }
    fflush(stderr);
}

int progress_init(ProgressState *state) {
    memset(state, 0, sizeof(*state));
    int rc = pthread_mutex_init(&state->mutex, NULL);
    if (rc != 0) {
        return rc;
    }
    state->initialized = true;
    state->interactive = isatty(fileno(stderr)) != 0;
    clock_gettime(CLOCK_MONOTONIC, &state->last_emit);
    return 0;
}

void progress_destroy(ProgressState *state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    pthread_mutex_destroy(&state->mutex);
    state->initialized = false;
}

void progress_phase_begin(ProgressState *state, ProgressPhase phase, const char *message) {
    if (state == NULL || !state->initialized) {
        return;
    }
    pthread_mutex_lock(&state->mutex);
    state->phase = phase;
    state->current_path[0] = '\0';
    if (state->interactive) {
        fprintf(stderr, "\r%-120s\r", "");
    }
    fprintf(stderr, "[%s] %s\n", phase_name(phase), message);
    fflush(stderr);
    clock_gettime(CLOCK_MONOTONIC, &state->last_emit);
    pthread_mutex_unlock(&state->mutex);
}

void progress_note_scan(ProgressState *state, ProgressPhase phase, const char *path) {
    if (state == NULL || !state->initialized) {
        return;
    }
    pthread_mutex_lock(&state->mutex);
    state->phase = phase;
    if (phase == PROGRESS_PHASE_SCAN_A) {
        state->scanned_a++;
    } else if (phase == PROGRESS_PHASE_SCAN_B) {
        state->scanned_b++;
    }
    clip_path(path, state->current_path, sizeof(state->current_path));
    emit_locked(state, false);
    pthread_mutex_unlock(&state->mutex);
}

void progress_note_compare_start(ProgressState *state, const char *path) {
    if (state == NULL || !state->initialized) {
        return;
    }
    pthread_mutex_lock(&state->mutex);
    state->phase = PROGRESS_PHASE_COMPARE;
    clip_path(path, state->current_path, sizeof(state->current_path));
    emit_locked(state, false);
    pthread_mutex_unlock(&state->mutex);
}

void progress_note_compare_done(ProgressState *state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    pthread_mutex_lock(&state->mutex);
    state->phase = PROGRESS_PHASE_COMPARE;
    state->compared++;
    emit_locked(state, false);
    pthread_mutex_unlock(&state->mutex);
}

void progress_phase_end(ProgressState *state, const char *message) {
    if (state == NULL || !state->initialized) {
        return;
    }
    pthread_mutex_lock(&state->mutex);
    if (state->interactive) {
        fprintf(stderr, "\r%-120s\r", "");
    }
    fprintf(stderr, "[%s] %s\n", phase_name(state->phase), message);
    fflush(stderr);
    pthread_mutex_unlock(&state->mutex);
}
