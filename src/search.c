#include "search.h"

// TODO: This uses way more memory than it probably should -- check if osu!
// 	 imposes any character limits on these values.
struct name_meta {
	char author[STR_LEN];
	char title[STR_LEN];
	char difficulty[STR_LEN];
};

static int parse_window_title(char *title, struct name_meta *meta);

static int parse_folder_name(char *name, struct name_meta *meta);

static int find_beatmap_folder(char *base_path, struct name_meta *title_meta,
			       char **out_name);

/**
 * Replaces the characters <, >, :, ", /, \, |, ?, * with _ if remove is set to
 * 0. Otherwise, removes them.
 */
static int sanitize_string(char *string, int remove);

/**
 * Parses a generic beatmap meta string.
 *
 * By default, expects a string of the format `${author} - ${title}``.
 * If `skip_par`, expects `${author} - ${title} (???)`,
 * if `has_diff`, expects `${author} - ${title} ${difficulty}`.
 *
 * `skip_par` may only be set if `has_diff` is also set.
 *
 * Assumes that author names can't contain a `-` and that titles can't contain a
 * `[` (i. e. that the last `[` is part of the difficulty).
 */
static int parse_generic_string(char *string, struct name_meta *meta,
				int skip_par, int has_diff);

size_t find_beatmap_exp(char *base_path, char *window_title) {
	if (!base_path || !window_title) {
		debug("received null pointer");
	}

	/**
	 * The folder name format is `${id} ${author} - ${title}`, where `id` is
	 * always a number.
	 *
	 * The file name format is `${author} - ${title} (???) ${difficulty}.osu`,
	 * where `difficulty` always contains an opening and closing bracket.
	 * I have no clue what the name (?) in the parentheses signifies but
	 * it's not in the title so it's ignored for now.
	 *
	 * The window title format is `osu!  - ${author} - ${title} ${difficulty}`,
	 * the two spaces after the `!` are intentional.
	 */

	struct name_meta title_meta;
	if (!(parse_window_title(window_title, &title_meta))) {
		printf("failed parsing window title\n");
		return 0;
	}

	debug("parsed window title");
	debug("\tauthor: '%s'", title_meta.author);
	debug("\ttitle: '%s'", title_meta.title);
	debug("\tdifficulty: '%s'", title_meta.difficulty);

	char *folder_name;
	if (!(find_beatmap_folder(base_path, &title_meta, &folder_name))) {
		printf("couldn't find the beatmap folder\n");
		return 0;
	}

	return 1;
}

static int find_beatmap_folder(char *base_path, struct name_meta *title_meta,
			       char **out_name) {
	DIR *dp;
	struct dirent *ep;

	if (!(dp = opendir(base_path))) {
		debug("couldn't open directory %s", base_path);
		debug("\t'%s'", strerror(errno));
		return 0;
	}

	*out_name = malloc(MAX_PATH);

	while ((ep = readdir(dp)) != NULL) {
		// TODO: Why does `ep->d_type` not exist on my machine?

		// Ignore `.`, `..` and hidden directories
		if (ep->d_name[0] == '.')
			continue;

		struct name_meta folder_meta;
		if (!(parse_folder_name(ep->d_name, &folder_meta))) {
			debug("failed parsing folder name '%s'", ep->d_name);
			continue;
		}
	}

	closedir(dp);

	return 0;
}

static int parse_folder_name(char *name, struct name_meta *meta) {
	// `${id} ${author} - ${title}`

	// Skip the beatmap ID
	while (1) {
		if (*name++ == ' ') {
			break;
		} else if (*name == '\0') {
			debug("found unexpected NULL");
			return 0;
		}
	}

	if (!(parse_generic_string(name, meta, 0, 0))) {
		debug("generic meta parsing failed");
		return 0;
	}

	if (sanitize_string(meta->author, 0) < 0) {
		debug("failed sanitizing '%s'", meta->author);
	}

	if (sanitize_string(meta->title, 0) < 0) {
		debug("failed sanitizing '%s'", meta->title);
	}

	if (sanitize_string(meta->difficulty, 0) < 0) {
		debug("failed sanitizing '%s'", meta->difficulty);
	}

	return 1;
}

static int parse_window_title(char *title, struct name_meta *meta) {
	// `osu!  - ${author} - ${title} ${difficulty}`

	// Skip the `osu!  - ` part.
	title += 8;

	if (!(parse_generic_string(title, meta, 0, 1))) {
		debug("generic meta parsing failed");
		return 0;
	}

	// Doesn't need any sanitizing.

	return 1;
}

// TODO: Check for errors during parsing.
static int parse_generic_string(char *string, struct name_meta *meta,
				int skip_par, int has_diff) {
	int i = 0;
	do {
		if (string[i] == '-') {
			strncpy_s(meta->author, STR_LEN, string, i - 1);
			meta->author[i - 1] = '\0';
			break;
		}
	} while (string[++i]);

	// Skip the author name we just parsed and the `- ` bit after that.
	string += (i += 2);

	if (has_diff) {
		int first_bracket = 0;
		while (i++) {
			if (string[i] == '[') {
				first_bracket = i;
			} else if (string[i] == '\0') {
				break;
			}
		} // As a side benefit, i is now strlen(title)

		strncpy_s(meta->title, STR_LEN, string, first_bracket - 1);
		meta->title[first_bracket - 1] = '\0';

		strncpy_s(meta->difficulty, STR_LEN, string + first_bracket,
			  i - first_bracket);
		meta->difficulty[i - first_bracket] = '\0';
	} else {
		size_t len = strlen(string);
		strncpy_s(meta->title, STR_LEN, string, len);
		meta->title[len] = '\0';
	}

	return 1;
}

static int sanitize_string(char *string, int remove) {
	if (!string) {
		debug("received null pointer");
		return -1;
	}

	int i = 0;
	do {
		switch (*string) {
			case '<':
			case '>':
			case ':':
			case '"':
			case '/':
			case '\\':
			case '|':
			case '?':
			case '*':
				*string = remove == 0 ? '_' : ':';
				i++;
				break;
		}
	} while (string++);

	return i;
}
