#pragma once
#include <iostream>



constexpr char CSI = 0x1b;

#define CODE(name, sequence) inline std::ostream & name(std::ostream & s) { s << CSI << sequence; return s; }

/** Resets all termial settings to default. */
CODE(reset, "c")

/** Text wraps to next line if longer than the length of the display area.  */
CODE(enableLineWrap, "[7h")

/** Disables line wrapping. */
CODE(disableLineWrap, "[7l")

/** Set default font. */
CODE(defaultFont, "(")

/** Set alternate font. */
CODE(alternateFont, ")")

/*
Cursor Control
Cursor Home 		<ESC>[{ROW};{COLUMN}H
Sets the cursor position where subsequent text will begin. If no row/column parameters are provided (ie. <ESC>[H), the cursor will move to the home position, at the upper left of the screen.
*/

/* Moves the cursor up by COUNT rows; the default count is 1. */
CODE(cursorUp, "[1A")

inline std::string cursorUp(unsigned lines) {
    return STR(CSI << "[" << lines << "A");
}

/**Moves the cursor down by COUNT rows; the default count is 1. */
CODE(cursorDown, "[1B")

inline std::string cursorDown(unsigned lines) {
    return STR(CSI << "[" << lines << "B");
}


/** Moves the cursor forward by COUNT columns; the default count is 1. */
CODE(cursorRight, "[1C")

/** Moves the cursor backward by COUNT columns; the default count is 1. */
CODE(cursorLeft, "[1D")

/*Force Cursor Position	<ESC>[{ROW};{COLUMN}f
Identical to Cursor Home. */

/** Save current cursor position. */
CODE(cursorSave, "[s")

/** Restores cursor position after a Save Cursor. */
CODE(cursorRestore, "[u")

/** Save current cursor position. */
CODE(cursorAttrSave, "7")

/** Restores cursor position after a Save Cursor. */
CODE(cursorAttrRestore, "8")

/**Erases from the current cursor position to the end of the current line. */
CODE(eraseEol, "[K")

/**Erases from the current cursor position to the start of the current line. */
CODE(eraseStartofLine, "[1K")

/** Erases the entire current line. */
CODE(eraseLine, "[2K")

/** Erases the screen from the current line down to the bottom of the screen. */
CODE(eraseDown, "[J")

/** Erases the screen from the current line up to the top of the screen. */
CODE(eraseUp, "[1J")

/** Erases the screen with the background colour and moves the cursor to home. */
CODE(eraseScreen, "[2J")

CODE(resetAttr, "[0m")
CODE(bright, "[1m")
CODE(dim, "[2m")
CODE(underscore, "[4m")
CODE(blink,"[5m")
CODE(reverse,"[7m")
CODE(hidden,"[8m")
CODE(black,"[30m")
CODE(red,"[31m")
CODE(green,"[32m")
CODE(yellow,"[33m")
CODE(blue,"[34m")
CODE(magenta,"[35m")
CODE(cyan,"[36m")
CODE(white,"[37m")
CODE(bgBlack,"[40m")
CODE(bgRed,"[41m")
CODE(bgGreen,"[42m")
CODE(bgYellow,"[43m")
CODE(bgBlue,"[44m")
CODE(bgMagenta,"[45m")
CODE(bgCyan,"[46m")
CODE(bgWhite,"[47m")

#undef CODE
