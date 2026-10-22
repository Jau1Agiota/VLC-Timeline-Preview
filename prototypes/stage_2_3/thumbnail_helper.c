/* Stage 2.3 research prototype. GPL-2.0-or-later; see ../../COPYING.
 * A captured BMP is NOT evidence of timestamp accuracy. See the results report.
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <vlc/vlc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <malloc.h>

#define MAX_W 320u
#define MAX_H 180u
#define SLOTS 3u
#define NEAR_END_WINDOW_MS 100u
#define NEAR_END_TAIL_MS 250u

typedef struct Capture Capture;
typedef struct Format Format;
typedef struct {
    Format *format;
    unsigned char *pixels;
    int busy;
} Slot;
struct Capture {
    HANDLE ready, finished;
    DWORD timeout;
    ULONGLONG started, capture_latency;
    volatile LONG captured, decoder_error, ended;
    int retain_until_end;
    CRITICAL_SECTION image_mutex;
    unsigned width, height;
    unsigned char image[MAX_W * MAX_H * 4];
};
struct Format {
    Capture *capture;
    unsigned width, height, pitch, lines;
    HANDLE available;
    CRITICAL_SECTION mutex;
    Slot slots[SLOTS];
};

/* Independent process deadline also bounds a stuck LibVLC initialization/stop.
 * Deliberately do not depend on stdio locks or decoder cooperation here. */
typedef struct { HANDLE finished; DWORD timeout; } Deadline;
static DWORD WINAPI watchdog(void *opaque)
{
    Deadline *c = opaque;
    if (WaitForSingleObject(c->finished, c->timeout) != WAIT_OBJECT_0)
        TerminateProcess(GetCurrentProcess(), 124);
    return 0;
}

static unsigned setup(void **opaque, char *chroma, unsigned *w, unsigned *h,
                      unsigned *pitches, unsigned *lines)
{
    Capture *capture = *opaque;
    if (!*w || !*h) return 0;
    double scale = 1.;
    if (*w > MAX_W) scale = (double)MAX_W / *w;
    if (*h * scale > MAX_H) scale = (double)MAX_H / *h;
    unsigned width = (unsigned)(*w * scale);
    unsigned height = (unsigned)(*h * scale);
    if (!width) width = 1;
    if (!height) height = 1;
    Format *f = calloc(1, sizeof(*f));
    if (!f) return 0;
    f->capture = capture;
    f->width = width; f->height = height;
    f->pitch = (width * 4u + 31u) & ~31u;
    f->lines = (height + 31u) & ~31u;
    InitializeCriticalSection(&f->mutex);
    f->available = CreateSemaphoreW(NULL, SLOTS, SLOTS, NULL);
    if (!f->available) goto fail;
    for (unsigned i = 0; i < SLOTS; ++i) {
        f->slots[i].format = f;
        f->slots[i].pixels = _aligned_malloc((size_t)f->pitch * f->lines, 32);
        if (!f->slots[i].pixels) goto fail;
        memset(f->slots[i].pixels, 0, (size_t)f->pitch * f->lines);
    }
    memcpy(chroma, "RV32", 4);
    *w = width; *h = height;
    pitches[0] = f->pitch; lines[0] = f->lines;
    *opaque = f;
    return SLOTS;
fail:
    for (unsigned i = 0; i < SLOTS; ++i) _aligned_free(f->slots[i].pixels);
    if (f->available) CloseHandle(f->available);
    DeleteCriticalSection(&f->mutex);
    free(f);
    return 0;
}

static void cleanup(void *opaque)
{
    Format *f = opaque;
    /* Called by vmem output teardown; no player control from a callback. */
    for (unsigned i = 0; i < SLOTS; ++i) _aligned_free(f->slots[i].pixels);
    CloseHandle(f->available);
    DeleteCriticalSection(&f->mutex);
    free(f);
}

static void *lock_pixels(void *opaque, void **planes)
{
    Format *f = opaque;
    if (WaitForSingleObject(f->available, INFINITE) != WAIT_OBJECT_0)
        TerminateProcess(GetCurrentProcess(), 3);
    EnterCriticalSection(&f->mutex);
    Slot *slot = NULL;
    for (unsigned i = 0; i < SLOTS; ++i)
        if (!f->slots[i].busy) { slot = &f->slots[i]; slot->busy = 1; break; }
    LeaveCriticalSection(&f->mutex);
    if (!slot) TerminateProcess(GetCurrentProcess(), 3);
    planes[0] = slot->pixels;
    return slot;
}

static void display(void *opaque, void *picture)
{
    (void)opaque;
    Slot *slot = picture;
    Format *f = slot->format;
    Capture *c = f->capture;
    /* Normal path: first displayed candidate. Tail retry: last delivered
     * candidate before stop, which need not be the stream's final frame.
     * Neither mode verifies PTS. Unique slots prevent buffer reuse races. */
    EnterCriticalSection(&c->image_mutex);
    if (c->retain_until_end || InterlockedCompareExchange(&c->captured, 1, 0) == 0) {
        c->width = f->width; c->height = f->height;
        for (unsigned y = 0; y < f->height; ++y)
            memcpy(c->image + (size_t)y * f->width * 4u,
                   slot->pixels + (size_t)y * f->pitch, f->width * 4u);
        for (size_t pixel = 0; pixel < (size_t)c->width * c->height; ++pixel)
            c->image[pixel * 4u + 3u] = 255; /* RV32 unused byte: opaque BMP. */
        c->capture_latency = GetTickCount64() - c->started;
        InterlockedExchange(&c->captured, 1);
        if (!c->retain_until_end) SetEvent(c->ready);
    }
    LeaveCriticalSection(&c->image_mutex);
    EnterCriticalSection(&f->mutex);
    slot->busy = 0;
    LeaveCriticalSection(&f->mutex);
    ReleaseSemaphore(f->available, 1, NULL);
}

static void player_event(const libvlc_event_t *event, void *opaque)
{
    Capture *c = opaque;
    if (event->type == libvlc_MediaPlayerEncounteredError)
        InterlockedExchange(&c->decoder_error, 1);
    else if (event->type == libvlc_MediaPlayerEndReached)
        InterlockedExchange(&c->ended, 1);
    SetEvent(c->ready);
}

static void put16(unsigned char *p, unsigned value)
{ p[0] = (unsigned char)value; p[1] = (unsigned char)(value >> 8); }
static void put32(unsigned char *p, uint32_t value)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (8u * i)); }
static int write_all(HANDLE file, const void *data, DWORD size)
{
    const unsigned char *p = data;
    while (size) {
        DWORD written = 0;
        if (!WriteFile(file, p, size, &written, NULL) || !written) return 0;
        p += written; size -= written;
    }
    return 1;
}
static int save_bmp(const wchar_t *path, const Capture *c)
{
    unsigned char header[54] = {0};
    DWORD stride = c->width * 4u, bytes = stride * c->height;
    header[0] = 'B'; header[1] = 'M';
    put32(header + 2, 54u + bytes); put32(header + 10, 54);
    put32(header + 14, 40); put32(header + 18, c->width);
    put32(header + 22, c->height); put16(header + 26, 1);
    put16(header + 28, 32); put32(header + 34, bytes);
    /* RV32 vmem masks on Windows x64 are 0xff0000/0xff00/0xff:
     * little-endian B,G,R,unused matches BI_RGB BMP. */
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    int ok = write_all(file, header, sizeof(header));
    for (unsigned y = c->height; ok && y > 0; --y)
        ok = write_all(file, c->image + (size_t)(y - 1) * stride, stride);
    if (!CloseHandle(file)) ok = 0;
    if (!ok) DeleteFileW(path);
    return ok;
}
static char *utf8(const wchar_t *s)
{
    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, -1,
                                   NULL, 0, NULL, NULL);
    if (!bytes) return NULL;
    char *out = malloc((size_t)bytes);
    if (out && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, -1,
                                   out, bytes, NULL, NULL)) { free(out); out = NULL; }
    return out;
}
/* GCC 6 MinGW can check stdio calls as MSVCRT even with ANSI stdio
 * selected. Emit decimal digits directly; no 64-bit printf dialect is needed.
 * UINT64_MAX has 20 decimal digits, plus the terminating NUL. */
static const char *u64_decimal(char buffer[21], uint64_t value)
{
    char *end = buffer + 20;
    *end = '\0';
    do {
        *--end = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    return end;
}

/* One retry only for a video input that reached EOF without a picture.
 * Decode the final 250 ms and retain its last delivered candidate until EOF.
 * This bounds the requested retry, NOT the unobservable delivered frame PTS. */
static int near_end_retry_start(const Capture *c, int retried, int timed_out,
                               int has_video, uint64_t target, uint64_t length,
                               uint64_t *retry_start)
{
    if (retried || timed_out || !has_video || !c->ended || c->captured ||
        c->decoder_error || length < NEAR_END_TAIL_MS ||
        target > length || length - target > NEAR_END_WINDOW_MS)
        return 0;
    *retry_start = length - NEAR_END_TAIL_MS;
    return 1;
}

static int number(const wchar_t *text, uint64_t max, uint64_t *value)
{
    if (!*text) return 0;
    for (const wchar_t *p = text; *p; ++p) if (*p < L'0' || *p > L'9') return 0;
    wchar_t *end;
    errno = 0;
    unsigned long long n = wcstoull(text, &end, 10);
    if (errno || *end || n > max) return 0;
    *value = (uint64_t)n;
    return 1;
}

static int extract(int argc, wchar_t **argv, libvlc_instance_t **retained, unsigned request_id)
{
    const ULONGLONG entry = GetTickCount64();
    unsigned init_ms = 0, create_ms = 0, play_ms = 0, wait_ms = 0, stop_ms = 0, bmp_ms = 0;
    ULONGLONG phase = 0, wait_started = 0;
    if (argc < 4 || argc > 6) {
        fwprintf(stderr, L"usage: thumbnail-helper.exe LOCAL.mp4|mkv TARGET_MS NEW.bmp [TIMEOUT_MS] [baseline|low-cache|fast-seek|hw-none|hw-auto]\n");
        return 2;
    }
    uint64_t target, deadline = 10000;
    /* Limit float start-time precision/range for this prototype to 24 hours. */
    if (!number(argv[2], 86400000, &target) ||
        (argc >= 5 && (!number(argv[4], 120000, &deadline) || deadline < 100))) return 2;
    const wchar_t *profile = argc == 6 ? argv[5] : L"baseline";
    if (wcscmp(profile,L"baseline") && wcscmp(profile,L"low-cache") && wcscmp(profile,L"fast-seek") &&
        wcscmp(profile,L"hw-none") && wcscmp(profile,L"hw-auto")) return 2;
    DWORD needed = GetFullPathNameW(argv[1], 0, NULL, NULL);
    if (!needed || needed > 32767) return 2;
    wchar_t *path = calloc((size_t)needed + 1, sizeof(*path));
    if (!path) return 3;
    DWORD actual = GetFullPathNameW(argv[1], needed + 1, path, NULL);
    if (!actual || actual > needed || path[1] != L':' || path[2] != L'\\') {
        free(path); return 2;
    }
    wchar_t root[] = {path[0], L':', L'\\', 0};
    DWORD attrs = GetFileAttributesW(path);
    const wchar_t *ext = wcsrchr(path, L'.');
    if (GetDriveTypeW(root) != DRIVE_FIXED || attrs == INVALID_FILE_ATTRIBUTES ||
        (attrs & FILE_ATTRIBUTE_DIRECTORY) || !ext ||
        (_wcsicmp(ext, L".mp4") && _wcsicmp(ext, L".mkv"))) { free(path); return 2; }
    char *source = utf8(path);
    free(path);
    if (!source) return 2;
    /* Do this before VLC starts threads. Only this helper's environment changes. */
    if (!SetEnvironmentVariableW(L"VLC_PLUGIN_PATH", NULL) &&
        GetLastError() != ERROR_ENVVAR_NOT_FOUND) { free(source); return 3; }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const char *version = libvlc_get_version();
    fprintf(stderr, "LibVLC runtime: %s\n", version);
    if (strncmp(version, "3.0.24", 6) || (version[6] && version[6] != ' ')) {
        free(source); return 2;
    }
    Capture c = {0};
    InitializeCriticalSection(&c.image_mutex);
    c.started = GetTickCount64(); c.timeout = (DWORD)deadline;
    c.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    c.finished = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE guard = NULL;
    int rc = 3, attached_error = 0, attached_end = 0;
    int retried = 0, has_video = 0;
    uint64_t effective_start = target, attempt_length = 0, initial_length = 0;
    const char *initial_status = "not_started";
    const char *status = "decoder_error";
    libvlc_instance_t *vlc = retained ? *retained : NULL;
    libvlc_media_t *media = NULL;
    libvlc_media_player_t *player = NULL;
    libvlc_event_manager_t *events = NULL;
    if (!c.ready || !c.finished) goto done;
    Deadline budget = {c.finished,c.timeout};
    guard = CreateThread(NULL, 0, watchdog, &budget, 0, NULL);
    if (!guard) goto done;
    const char *options[8] = {"--ignore-config", "--no-audio", "--no-spu",
        "--no-osd", "--no-video-title-show", "--no-input-fast-seek"};
    int option_count = 6;
    if (wcscmp(profile,L"baseline")) options[option_count++] = "--file-caching=0";
    if (!wcscmp(profile,L"fast-seek")) options[5] = "--input-fast-seek";
    if (!wcscmp(profile,L"hw-none")) options[option_count++] = "--avcodec-hw=none";
    if (!wcscmp(profile,L"hw-auto")) options[option_count++] = "--avcodec-hw=any";
    phase = GetTickCount64();
    if (!vlc) vlc = libvlc_new(option_count, options);
    if (retained) *retained = vlc;
    init_ms = (unsigned)(GetTickCount64() - phase);
    if (!vlc) goto done;
open_media:
    attempt_length = 0;
    has_video = 0;
    phase = GetTickCount64();
    media = libvlc_media_new_path(vlc, source);
    if (!media) goto done;
    char start[64], seconds[21];
    snprintf(start, sizeof(start), ":start-time=%s.%03u",
             u64_decimal(seconds, effective_start / 1000), (unsigned)(effective_start % 1000));
    libvlc_media_add_option(media, start);
    player = libvlc_media_player_new_from_media(media);
    if (!player) goto done;
    libvlc_video_set_callbacks(player, lock_pixels, NULL, display, &c);
    libvlc_video_set_format_callbacks(player, setup, cleanup);
    events = libvlc_media_player_event_manager(player);
    attached_error = !libvlc_event_attach(events, libvlc_MediaPlayerEncounteredError, player_event, &c);
    attached_end = !libvlc_event_attach(events, libvlc_MediaPlayerEndReached, player_event, &c);
    create_ms += (unsigned)(GetTickCount64() - phase);
    phase = GetTickCount64();
    const int play_error = (!attached_error || !attached_end) ? -1 : libvlc_media_player_play(player);
    play_ms += (unsigned)(GetTickCount64() - phase);
    if (play_error) goto done;
    wait_started = GetTickCount64();
    if (WaitForSingleObject(c.ready, c.timeout) != WAIT_OBJECT_0) {
        rc = 124; status = "timeout"; goto done;
    }
    /* Error/EOF can signal ready before the image is copied. Only display sets
     * captured; its SetEvent follows the copy. Event callbacks may race that
     * copy, so stop joins all callbacks BEFORE reading the captured image. */
done:
    if (wait_started) wait_ms += (unsigned)(GetTickCount64() - wait_started);
    wait_started = 0;
    phase = GetTickCount64();
    if (player) {
        libvlc_time_t duration = libvlc_media_player_get_length(player);
        if (duration > 0) attempt_length = (uint64_t)duration;
        if (!retried && c.ended && !c.captured && !c.decoder_error && rc != 124) {
            libvlc_media_track_t **tracks = NULL;
            unsigned count = libvlc_media_tracks_get(media, &tracks);
            for (unsigned i = 0; i < count; ++i)
                if (tracks[i]->i_type == libvlc_track_video) has_video = 1;
            libvlc_media_tracks_release(tracks, count);
        }
        libvlc_media_player_stop(player);
        if (attached_error) libvlc_event_detach(events, libvlc_MediaPlayerEncounteredError, player_event, &c);
        if (attached_end) libvlc_event_detach(events, libvlc_MediaPlayerEndReached, player_event, &c);
        libvlc_media_player_release(player);
        player = NULL;
        events = NULL;
    }
    if (media) { libvlc_media_release(media); media = NULL; }
    if (!retried) {
        initial_length = attempt_length;
        initial_status = rc == 124 ? "timeout" :
                         c.decoder_error ? "decoder_error" :
                         c.captured ? "candidate_captured" :
                         c.ended ? "ended_without_video" : "decoder_error";
    }
    stop_ms += (unsigned)(GetTickCount64() - phase);
    uint64_t retry_start;
    if (near_end_retry_start(&c, retried, rc == 124, has_video,
                             target, attempt_length, &retry_start)) {
        /* stop/release above drains all callbacks before resetting their state.
         * Keep the SAME instance, watchdog and overall deadline; no new budget. */
        retried = 1;
        c.retain_until_end = 1;
        effective_start = retry_start;
        c.captured = c.decoder_error = c.ended = 0;
        attached_error = attached_end = 0;
        ResetEvent(c.ready);
        goto open_media;
    }
    phase = GetTickCount64();
    if (vlc && !retained) libvlc_release(vlc);
    stop_ms += (unsigned)(GetTickCount64() - phase);
    phase = GetTickCount64();
    if (rc != 124 && c.captured && !c.decoder_error) {
        if (save_bmp(argv[3], &c)) {
            rc = 0;
            status = retried ? "fallback_candidate_captured" : "candidate_captured";
        }
        else { rc = 4; status = "output_error"; }
    } else if (rc != 124 && c.ended && !c.decoder_error) status = "ended_without_video";
    bmp_ms = (unsigned)(GetTickCount64() - phase);
    free(source);
    if (c.finished) SetEvent(c.finished);
    if (guard) { WaitForSingleObject(guard, INFINITE); CloseHandle(guard); }
    DeleteCriticalSection(&c.image_mutex);
    if (c.ready) CloseHandle(c.ready);
    if (c.finished) CloseHandle(c.finished);
    char target_text[21], capture_text[21], total_text[21];
    char start_text[21], length_text[21];
    printf("{\"status\":\"%s\",\"requested_ms\":%s,\"capture_latency_ms\":%s,"
           "\"total_latency_ms\":%s,\"width\":%u,\"height\":%u,"
           "\"frame_timestamp_verified\":false,\"exit_code\":%d,"
           "\"fallback_used\":%s,\"attempts\":%u,\"effective_start_ms\":%s,"
           "\"initial_length_ms\":%s,\"initial_extraction_status\":\"%s\","
           "\"libvlc_init_ms\":%u,\"media_player_create_ms\":%u,\"play_call_ms\":%u,"
           "\"decode_wait_ms\":%u,\"stop_release_ms\":%u,\"bmp_output_ms\":%u,\"entry_to_result_ms\":%u,\"request_id\":%u}\n",
           status, u64_decimal(target_text, target),
           u64_decimal(capture_text, (uint64_t)c.capture_latency),
           u64_decimal(total_text, (uint64_t)(GetTickCount64() - c.started)),
           c.width, c.height, rc, retried ? "true" : "false",
           (unsigned)(1 + retried), u64_decimal(start_text, effective_start),
           u64_decimal(length_text, initial_length), initial_status, init_ms, create_ms, play_ms,
           wait_ms, stop_ms, bmp_ms, (unsigned)(GetTickCount64() - entry), request_id);
    fflush(stdout);
    return rc;
}

/* Protocol v1: fixed argv media/output, at most one line in flight. Keep only
 * LibVLC's instance/plugin initialization; NEVER reuse a player/decoder/clock.
 * Cancellation is process termination by the controller, not a concurrent seek.
 */
static int request_line(const char *line, unsigned fields[3])
{
    for (unsigned i=0; i<3; ++i) {
        if (*line < '0' || *line > '9') return 0;
        unsigned value=0;
        while (*line >= '0' && *line <= '9') {
            unsigned digit=(unsigned)(*line++ - '0');
            if (value > (1000000000u-digit)/10u) return 0;
            value=value*10u+digit;
        }
        fields[i]=value;
        if (i<2) { if (*line++ != ' ') return 0; }
    }
    return (*line == '\n' && !line[1]) || (*line == '\r' && line[1]=='\n' && !line[2]);
}

int wmain(int argc, wchar_t **argv)
{
    if (argc < 2 || wcscmp(argv[1],L"--worker")) return extract(argc,argv,NULL,0);
    if (argc != 4 && argc != 5) return 2;
    /* This independent watchdog also bounds idle stdin and final teardown.
     * The GUI has a shorter idle budget; standalone workers cannot live forever.
     */
    Deadline lifetime={0}; lifetime.timeout=120000;
    lifetime.finished=CreateEventW(NULL,TRUE,FALSE,NULL);
    if (!lifetime.finished) return 3;
    HANDLE guard=CreateThread(NULL,0,watchdog,&lifetime,0,NULL);
    if (!guard) { CloseHandle(lifetime.finished); return 3; }
    libvlc_instance_t *retained=NULL;
    unsigned previous=0;
    int rc=0;
    char line[128];
    for (unsigned count=0; count<64 && fgets(line,sizeof(line),stdin); ++count) {
        unsigned fields[3];
        if (!request_line(line,fields) || fields[0] <= previous || fields[0]>64 ||
            fields[1]>86400000 || fields[2]<100 || fields[2]>120000) { rc=2; break; }
        previous=fields[0];
        wchar_t target[21], timeout[21];
        swprintf(target,21,L"%u",fields[1]); swprintf(timeout,21,L"%u",fields[2]);
        wchar_t *request[]={argv[0],argv[2],target,argv[3],timeout,argc==5?argv[4]:L"baseline"};
        rc=extract(6,request,&retained,fields[0]);
        if (rc) break; // A failed instance is never reused.
    }
    /* Shorten the remaining teardown budget before releasing the instance. */
    Deadline teardown={0}; teardown.timeout=5000;
    teardown.finished=CreateEventW(NULL,TRUE,FALSE,NULL);
    HANDLE final_guard=teardown.finished?CreateThread(NULL,0,watchdog,&teardown,0,NULL):NULL;
    if (!final_guard) TerminateProcess(GetCurrentProcess(),124);
    if (retained) libvlc_release(retained);
    SetEvent(teardown.finished); WaitForSingleObject(final_guard,INFINITE);
    CloseHandle(final_guard); CloseHandle(teardown.finished);
    SetEvent(lifetime.finished); WaitForSingleObject(guard,INFINITE);
    CloseHandle(guard); CloseHandle(lifetime.finished);
    return rc;
}
