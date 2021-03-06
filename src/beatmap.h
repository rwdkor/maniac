#ifndef BEATMAP_H
#define BEATMAP_H

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>

// 7th bit in type is hold
#define TYPE_HOLD 128u

#define MIN_VERSION 12
#define MAX_VERSION 14

#define ERROR_UNSUPPORTED_BEATMAP -1

struct beatmap_meta {
	int set_id;
	int map_id;
	int columns;
	char title[256];
	char artist[256];
	char version[256];
};

struct hitpoint {
	int column;
	int end_time;
	int start_time;
};

struct action {
	int down;
	char key;
	int time;
};

/**
 * Searches for a beatmap in DEFAULT_OSU_PATH + base given a part of the
 * file name, and stores the absolute path to it in *map.
 * Returns the length of the path stored, or zero on failure.
 */
size_t find_beatmap(char *base, char *partial, char **map);

/**
 * Parse a beatmap file (*.osu) into an array of hitpoint structs pointed to by
 * **points and a metadata struct.
 * Returns the number of points parsed and stored.
 */
size_t parse_beatmap(char *file, struct hitpoint **points,
		     struct beatmap_meta **meta);

/**
 * Parses a total of count hitmapts from **points into **actions.
 * Returns the number of actions parsed and stored, which should be count * 2.
 */
int parse_hitpoints(size_t count, size_t columns, struct hitpoint **points,
		    struct action **actions);

/**
 * Sort the array of actions given through **actions by time.
 * Returns nonzero on failure.
 */
int sort_actions(int count, struct action **actions);

#endif /* BEATMAP_H */
