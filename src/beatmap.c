#include "beatmap.h"

size_t find_beatmap_exp(char *base_path, char *window_title);

/**
 * Parses a raw beatmap line into a beatmap_meta struct pointed to by *meta.
 * Returns the number of tokens read.
 */
static int parse_beatmap_line(char *line, struct beatmap_meta *meta);

/**
 * Parses a key:value set into *meta.
 */
static int parse_beatmap_token(char *key, char *value,
			       struct beatmap_meta *meta);

/**
 * Parses a raw hitobject line into a hitpoint struct pointed to by *point.
 * Returns the number of tokens read.
 */
static int parse_hitobject_line(char *line, int columns,
				struct hitpoint *point);

/**
 * Populates *start and *end with data from hitpoint *point.
 */
static void hitpoint_to_action(char *keys, struct hitpoint *point,
			       struct action *start, struct action *end);

/**
 * Searches for a file or folder in `base`, matching all directory entries
 * against `partial`. The best match is returned through *out_file.
 * Returns the length of the matched path or zero on failure.
 */
static size_t find_partial_file(char *base, char *partial, char **out_file);

/**
 * `levenshtein.c` - levenshtein
 * MIT licensed.
 * Copyright (c) 2015 Titus Wormer <tituswormer@gmail.com>
 * See https://github.com/wooorm/levenshtein.c
 * Returns a size_t, depicting the difference between `a` and `b`.
 */
size_t levenshtein_n(const char *a, size_t length, const char *b, size_t bLength);

const char col_keys[9] = "asdfjkl[";

size_t find_beatmap(char *base, char *partial, char **map) {
	if (!base || !partial || !map) {
		debug("received null pointer");
		return 0;
	}

	char *folder = NULL;
	size_t folder_len = 0;

	if (!(folder_len = find_partial_file(base, partial, &folder))) {
		debug("couldn't find folder (%s)", partial);
		return 0;
	}

	size_t map_len = 256, base_len = strlen(base);
	*map = malloc(map_len);

	// Absolute path to our base.
	strcpy(*map, base);
	// A.p. to the beatmap folder.
	strcpy(*map + base_len, folder);
	// Add a trailing separator and terminating zero.
	strcpy(*map + base_len + folder_len, (char[2]) {
		(char) SEPERATOR, '\0'
	});

	free(folder);

	char *beatmap = NULL;
	size_t beatmap_len = 0;

	if (!(beatmap_len = find_partial_file(*map, partial, &beatmap))) {
		debug("couldn't find beatmap in %s", *map);
		return 0;
	}

	// This is now the absolute path to our beatmap.
	strcpy(*map + base_len + folder_len + 1, beatmap);

	free(beatmap);

	map_len = base_len + folder_len + 1 + beatmap_len;

	// Change block size to fit what we really need.
	*map = realloc(*map, map_len + 1);

	// Verify that the file we found is a beatmap.
	if (strcmp(*map + map_len - 4, ".osu") != 0) {
		debug("%s is not a beatmap", *map);

		free(*map);

		return 0;
	}

	return map_len;
}

// TODO: Inefficient as it calls realloc() for every parsed line. Allocate
// 	 memory in chunks and copy it to adequately sized buffer once done.
size_t parse_beatmap(char *file, struct hitpoint **points,
		     struct beatmap_meta **meta) {
	debug("parsing beatmap '%s'", file);

	if (!points || !meta || !file) {
		debug("received null pointer");
		return 0;
	}

	FILE *stream;

	if (!(stream = fopen(file, "r"))) {
		debug("couldn't open file %s", file);
		return 0;
	}

	const size_t line_len = 256;
	char *line = malloc(line_len);

	// First line always contains version.
	if (fgets(line, (int) line_len, stream)) {
		short version = (short) strtol(line + 17, NULL, 10);

		debug("beatmap version is %d", version);

		if (version < MIN_VERSION || version > MAX_VERSION) {
			printf("parsing an unsupported beatmap (%d)",
			       version);
		}
	}

	*points = NULL;
	*meta = calloc(1, sizeof(struct beatmap_meta));

	struct hitpoint cur_point;
	size_t hp_size = sizeof(struct hitpoint), num_parsed = 0;

	char cur_section[128];

	// TODO: This loop body is all kinds of messed up.
	while (fgets(line, (int) line_len, stream)) {
		line[strlen(line) - 1] = '\0';

		if (!(strcmp(cur_section, "[General]"))) {
			int err = parse_beatmap_line(line, *meta);

			if (err <= 0) {
				return err;
			}
		} else if (!(strcmp(cur_section, "[Metadata]"))
			   || !(strcmp(cur_section, "[Difficulty]"))) {
			parse_beatmap_line(line, *meta);
		} else if (!(strcmp(cur_section, "[HitObjects]"))) {
			parse_hitobject_line(line, meta[0]->columns,
					     &cur_point);

			*points = realloc(*points, ++num_parsed * hp_size);
			points[0][num_parsed - 1] = cur_point;
		}

		if (line[0] == '[') {
			strcpy(cur_section, line);

			const int len = (int) strlen(line);
			for (int i = 0; i < len; i++) {
				if (line[i] == ']') {
					cur_section[i + 1] = '\0';
				}
			}
		}
	}

	free(line);
	fclose(stream);

	return num_parsed;
}

// TODO: This function is not thread safe.
static int parse_beatmap_line(char *line, struct beatmap_meta *meta) {
	int i = 0;
	// strtok() modifies its arguments, work with a copy.
	char *ln = strdup(line);
	char *token = NULL, *key = NULL, *value = NULL;

	// Metadata lines come in key:value pairs.
	token = strtok(ln, ":");
	while (token != NULL) {
		switch (i++) {
		case 0:
			key = strdup(token);
			break;
		case 1:
			value = strdup(token);
			int err = parse_beatmap_token(key, value, meta);

			if (err <= 0) {
				return err;
			}

			break;
		}

		token = strtok(NULL, ":");
	}

	free(ln);
	free(key);
	free(token);
	free(value);

	return 1;
}

static int parse_beatmap_token(char *key, char *value,
			       struct beatmap_meta *meta) {
	if (!key || !value || !meta) {
		debug("received null pointer");
		return 0;
	}

	if (!(strcmp(key, "Mode"))) {
		int mode = (int) strtol(value, NULL, 10);

		debug("mode is %d", mode);

		if (mode != 3) {
			return ERROR_UNSUPPORTED_BEATMAP;
		}
	} else if (!(strcmp(key, "Title"))) {
		value[strlen(value)] = '\0';

		strcpy(meta->title, value);
	} else if (!(strcmp(key, "Artist"))) {
		value[strlen(value)] = '\0';

		strcpy(meta->artist, value);
	} else if (!(strcmp(key, "Version"))) {
		value[strlen(value)] = '\0';

		strcpy(meta->version, value);
	} else if (!(strcmp(key, "BeatmapID"))) {
		meta->map_id = strtol(value, NULL, 10);
	} else if (!(strcmp(key, "BeatmapSetID"))) {
		meta->set_id = strtol(value, NULL, 10);
	} else if (!(strcmp(key, "CircleSize"))) {
		// This doesn't work for non-mania maps
		meta->columns = strtol(value, NULL, 10);

		if (meta->columns > 9) {
			return ERROR_UNSUPPORTED_BEATMAP;
		}
	}

	return 1;
}

// TODO: This function is not thread safe.
// This chokes on non-mania maps.
static int parse_hitobject_line(char *line, int columns, struct hitpoint *point) {
	int secval, end_time, hold = 0, i = 0;
	char *ln = strdup(line), *token = NULL;

	// Line is expected to follow the following format:
	// x, y, time, type, hitSound, extras (= a:b:c:d:)
	token = strtok(ln, ",");
	while (token != NULL) {
		secval = (int) strtol(token, NULL, 10);

		switch (i++) {
		// X
		case 0:
			point->column = secval / (COLS_WIDTH / columns);
			break;
			// Start time
		case 2:
			point->start_time = secval - 15;
			break;
			// Type
		case 3:
			hold = secval & TYPE_HOLD;
			break;
			// Extra string, first element is either 0 or end time
		case 5:
			end_time = (int) strtol(strtok(token, ":"), NULL, 10);

			point->end_time = hold ? end_time :
					  point->start_time + TAPTIME_MS;

			break;
		}

		token = strtok(NULL, ",");
	}

	free(ln);
	free(token);

	return i;
}

int parse_hitpoints(size_t count, size_t columns, struct hitpoint **points,
		    struct action **actions) {
	// Allocate enough memory for all actions at once.
	*actions = malloc((2 * count) * sizeof(struct action));

	struct hitpoint *cur_point;
	size_t num_actions = 0, i = 0;

	char *key_subset = malloc(columns + 1);
	key_subset[columns] = '\0';

	const size_t col_size = sizeof(col_keys) - 1;
	const size_t subset_offset = (col_size / 2) - (columns / 2);

	memmove(key_subset, col_keys + subset_offset,
		col_size - (subset_offset * 2));

	if (columns % 2) {
		memmove(key_subset + columns / 2 + 1, key_subset + columns / 2,
			columns / 2);
		key_subset[columns / 2] = ' ';
	}

	while (i < count) {
		cur_point = (*points) + i++;

		// Don't care about the order here.
		struct action *end = *actions + num_actions++;
		struct action *start = *actions + num_actions++;

		hitpoint_to_action(key_subset, cur_point, start, end);
	}

	free(key_subset);

	// TODO: Check if all *actions memory was used and free() if applicable.

	return num_actions;
}

static void hitpoint_to_action(char *const keys, struct hitpoint *point,
			       struct action *start, struct action *end) {
	end->time = point->end_time;
	start->time = point->start_time;

	end->down = 0;		// Keyup.
	start->down = 1;	// Keydown.

	char key = keys[point->column];

	end->key = key;
	start->key = key;
}

// TODO: Implement a more efficient sorting algorithm than selection sort.
int sort_actions(int total, struct action **actions) {
	int min, i, j;
	struct action *act = *actions, tmp;

	// For every element but the last, which will end up to be the biggest.
	for (i = 0; i < (total - 1); i++) {
		min = i;

		// In the subarray starting at a[j]...
		for (j = i; j < total; j++)
			// ...find the smallest element.
			if ((act + j)->time < (act + min)->time) min = j;

		// Swap current element with the smallest element of subarray.
		tmp = act[i];
		act[i] = act[min];
		act[min] = tmp;
	}

	return i - total + 1;
}

static size_t find_partial_file(char *base, char *partial, char **out_file) {
	if (!base || !partial || !out_file) {
		debug("received null pointer");
		return 0;
	}

	DIR *dp;
	struct dirent *ep;

	if (!(dp = opendir(base))) {
		debug("couldn't open directory %s", base);
		return 0;
	}

	const int file_len = 256;
	*out_file = malloc(file_len);

	size_t least_distance = SIZE_MAX;
	size_t partial_len = strlen(partial);

	while ((ep = readdir(dp))) {
		char *name = ep->d_name;

		if (name[0] == '.')
			continue;

		size_t last_dot = 0;
		size_t name_len = strlen(name);
		for (size_t i = 0; i < name_len; i++)
			if (name[i] == '.')
				last_dot = i;

		size_t distance = levenshtein_n(partial, partial_len, name,
						last_dot == 0 ? name_len : last_dot);

		if (distance < least_distance) {
			least_distance = distance;

			strcpy(*out_file, name);
		}
	}

	closedir(dp);

	debug("found file '%s'", *out_file);

	return strlen(*out_file);
}

size_t levenshtein_n(const char *a, const size_t length, const char *b, const size_t bLength) {
	// Shortcut optimizations / degenerate cases.
	if (a == b) {
		return 0;
	}

	if (length == 0) {
		return bLength;
	}

	if (bLength == 0) {
		return length;
	}

	size_t *cache = calloc(length, sizeof(size_t));
	size_t index = 0;
	size_t bIndex = 0;
	size_t distance;
	size_t bDistance;
	size_t result;
	char code;

	while (index < length) {
		cache[index] = index + 1;
		index++;
	}

	while (bIndex < bLength) {
		code = b[bIndex];
		result = distance = bIndex++;
		index = SIZE_MAX;

		while (++index < length) {
			bDistance = code == a[index] ? distance : distance + 1;
			distance = cache[index];

			cache[index] = result = distance > result
						? bDistance > result
						  ? result + 1
						  : bDistance
						: bDistance > distance
						  ? distance + 1
						  : bDistance;
		}
	}

	free(cache);

	return result;
}
