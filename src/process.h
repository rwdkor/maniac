#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"

#include <string.h>
#include <limits.h>

#ifdef ON_LINUX
// Enable GNU extensions (process_vm_readv).
// TODO: This is hacky and undocumented.
#define __USE_GNU

  #include <sys/uio.h>
#endif /* ON_LINUX */

#ifdef ON_WINDOWS

  #include <tlhelp32.h>

  // #define TIME_SIG   "\xDB\x5D\xE8\x8B\x45\xE8\xA3"
  #define TIME_SIG	"\x24\x34\x8B\x44\x24\x34\xA3"

HANDLE game_proc;
#endif /* ON_WINDOWS */

void *time_address;
pid_t game_proc_id;

/**
 * Gets and returns the runtime of the currently playing song, internally
 * referred to as `maptime`.
 */
hot int32_t get_maptime(void);

/**
 * Returns the process id of the given process or zero if it was not found.
 */
unsigned long get_process_id(const char *name);

/**
 * Copies game memory starting at `base` for `size` bytes into `buffer`.
 * Internally, this is a wrapper for _read_game_memory, with argument checking.
 * Returns number of bytes read and copied.
 */
ssize_t read_game_memory(void *base, void *buffer, size_t size);


/**
 * Returns the address of the playback time in the address space of the game
 * process.
 * Windows: Scans memory for the address using a signature.
 * Linux: Returns static address (LINUX_TIME_ADDRESS).
 */
void *get_time_address(void);

#endif /* PROCESS_H */
