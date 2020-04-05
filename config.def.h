/*
 * Define ESC seqences to use for scroll events.
 * Use "cat -v" to figure out favorit key combination.
 *
 * lines is the number of lines scrolled up or down.  If lines is negaive,
 * its the: -1 is 1/1.
 */

struct rule rules[] = {
	/* sequence     event        lines */
	{"\033[5;2~",	SCROLL_UP,   -1},	/* [Shift] + [PageUP] */
	{"\033[6;2~",	SCROLL_DOWN, -1},	/* [Shift] + [PageDown] */
	{"\031",	SCROLL_UP,    1},	/* mouse wheel up */
	{"\005",	SCROLL_DOWN,  1}	/* mouse wheel Down */
};
