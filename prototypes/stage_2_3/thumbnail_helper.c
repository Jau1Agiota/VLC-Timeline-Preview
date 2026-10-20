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
static DWORD WINAPI watchdog(void *opaque)
{
    Capture *c = opaque;
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
    /* First displayed candidate after startup positioning, NOT a verified PTS.
     * Unique per-picture slots avoid reading a buffer reused after unlock. */
    if (InterlockedCompareExchange(&c->captured, 1, 0) == 0) {
        c->width = f->width; c->height = f->height;
        for (unsigned y = 0; y < f->height; ++y)
            memcpy(c->image + (size_t)y * f->width * 4u,
                   slot->pixels + (size_t)y * f->pitch, f->width * 4u);
        for (size_t pixel = 0; pixel < (size_t)c->width * c->height; ++pixel)
            c->image[pixel * 4u + 3u] = 255; /* RV32 unused byte: opaque BMP. */
        c->capture_latency = GetTickCount64() - c->started;
        SetEvent(c->ready);
    }
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

int wmain(int argc, wchar_t **argv)
{
    if (argc < 4 || argc > 5) {
        fwprintf(stderr, L"usage: thumbnail-helper.exe LOCAL.mp4|mkv TARGET_MS NEW.bmp [TIMEOUT_MS]\n");
        return 2;
    }
    uint64_t target, deadline = 10000;
    /* Limit float start-time precision/range for this prototype to 24 hours. */
    if (!number(argv[2], 86400000, &target) ||
        (argc == 5 && (!number(argv[4], 120000, &deadline) || deadline < 100))) return 2;
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
    c.started = GetTickCount64(); c.timeout = (DWORD)deadline;
    c.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    c.finished = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE guard = NULL;
    int rc = 3, attached_error = 0, attached_end = 0;
    const char *status = "decoder_error";
    libvlc_instance_t *vlc = NULL;
    libvlc_media_t *media = NULL;
    libvlc_media_player_t *player = NULL;
    libvlc_event_manager_t *events = NULL;
    if (!c.ready || !c.finished) goto done;
    guard = CreateThread(NULL, 0, watchdog, &c, 0, NULL);
    if (!guard) goto done;
    const char *options[] = {"--ignore-config", "--no-audio", "--no-spu",
        "--no-osd", "--no-video-title-show", "--no-input-fast-seek"};
    vlc = libvlc_new((int)(sizeof(options) / sizeof(options[0])), options);
    if (!vlc) goto done;
    media = libvlc_media_new_path(vlc, source);
    if (!media) goto done;
    char start[64];
    snprintf(start, sizeof(start), ":start-time=%llu.%03llu",
             (unsigned long long)(target / 1000), (unsigned long long)(target % 1000));
    libvlc_media_add_option(media, start);
    player = libvlc_media_player_new_from_media(media);
    if (!player) goto done;
    libvlc_video_set_callbacks(player, lock_pixels, NULL, display, &c);
    libvlc_video_set_format_callbacks(player, setup, cleanup);
    events = libvlc_media_player_event_manager(player);
    attached_error = !libvlc_event_attach(events, libvlc_MediaPlayerEncounteredError, player_event, &c);
    attached_end = !libvlc_event_attach(events, libvlc_MediaPlayerEndReached, player_event, &c);
    if (!attached_error || !attached_end || libvlc_media_player_play(player)) goto done;
    if (WaitForSingleObject(c.ready, c.timeout) != WAIT_OBJECT_0) {
        rc = 124; status = "timeout"; goto done;
    }
    /* Error/EOF can signal ready before the image is copied. Only display sets
     * captured; its SetEvent follows the copy. Event callbacks may race that
     * copy, so stop joins all callbacks BEFORE reading the captured image. */
done:
    if (player) {
        libvlc_media_player_stop(player);
        if (attached_error) libvlc_event_detach(events, libvlc_MediaPlayerEncounteredError, player_event, &c);
        if (attached_end) libvlc_event_detach(events, libvlc_MediaPlayerEndReached, player_event, &c);
        libvlc_media_player_release(player);
    }
    if (media) libvlc_media_release(media);
    if (vlc) libvlc_release(vlc);
    if (rc != 124 && c.captured && !c.decoder_error) {
        if (save_bmp(argv[3], &c)) { rc = 0; status = "candidate_captured"; }
        else { rc = 4; status = "output_error"; }
    } else if (rc != 124 && c.ended && !c.decoder_error) status = "ended_without_video";
    free(source);
    if (c.finished) SetEvent(c.finished);
    if (guard) { WaitForSingleObject(guard, INFINITE); CloseHandle(guard); }
    if (c.ready) CloseHandle(c.ready);
    if (c.finished) CloseHandle(c.finished);
    printf("{\"status\":\"%s\",\"requested_ms\":%llu,\"capture_latency_ms\":%llu,"
           "\"total_latency_ms\":%llu,\"width\":%u,\"height\":%u,"
           "\"frame_timestamp_verified\":false,\"exit_code\":%d}\n",
           status, (unsigned long long)target, (unsigned long long)c.capture_latency,
           (unsigned long long)(GetTickCount64() - c.started), c.width, c.height, rc);
    return rc;
}
