/**
 *  Copyright (C) 2004, 2005 Benjamin C Meyer
 *                           (ben+asciimaze at meyerhome dot net)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

/**
 * GenMaze - Generates a maze of any size in two different formats
 * 
 * genmaze uses Eller's algorithm to create the maze.  This algorithm is
 * faster than all others and is also the most memory efficient.  It only
 * uses memory proportional to the width of a row. It creates the maze
 * one row at a time. Once a row is generated, the algorithm no longer 
 * needs it. 
 *
 * Unfortunately the ASCII output requires the last row be saved to properly 
 * generate that output so the implementation requires twice the memory
 * that it should.
 * 
 * Each cell in a row is contained in a set, where two cells are in the same
 * set if there's a path between them through the part of the Maze that's been
 * made so far. This information allows passages to be carved in the current
 * row without creating loops or isolations. Creating a row consists of the
 * following steps:
 *
 * Step 1) Clear each cell and put each cell in it's own set unless the cell
 * was going down in which case mark it going up and keep the set.
 *
 * Step 2) Randomly connect adjacent cells within a row, making horizontal
 * passages. When making horizontal passages, don't connect cells already
 * in the same set ( making a braid )  When carving horizontal passages,
 * connect the sets they're in (since there's now a path between them).
 * Randomly connect cells to the next row (making vertical passages).
 *
 * Step 3) Cells that are in a set by themselves must be connected to the next
 * row (Abandoning a set would create an isolation)
 *
 * Step 4) If it is the last row connect cells horizontally that are not in
 * the same set.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

// For uint on OSX...
#include <readline/readline.h>

// The definition for what a block can be (or'd together).
#define EMPTY 0
#define UP    1
#define DOWN  2
#define LEFT  4
#define RIGHT 8

// Whitespace buffer on left hand side of the maze
#define BUFFER 5

// At this point there are only two types, but abstracted a tiny bit for
// possible future use.
enum MAZETYPE { ASCII, BLOCK };

// Global variables
uint *set;
uint *previousRow;
uint *row;
uint width;
// These two only shown with the ASCII output
bool debugsets;
bool debugrows;

/**
 * Draw a row for a maze that is block based with sharing colums.
 * 
 * This output style is here because it is very easy to output compared to ascii
 * and because the output takes up less room (for very large mazes). 
 * 
 * XXXXXXXXXXXXXXXXX
 * X               X
 * X XXX XXX XXXXX X
 * X   X X X X     X
 * XXX X X X X XXX X
 * X   X   X X   X X
 * XXXXXXXXXXXXXXXXX
 *
 *  @param isLast if this row is the last one.
 */
void outputBlock(bool isLast)
{
    // Top line
    for (uint i = 0; i < width; i++)
        printf((row[i] & UP) ? "X " : "XX");
    printf("X\n");

    // Middle line
    for (uint i = 0; i < width; i++)
        printf((row[i] & LEFT) ? "  " : "X ");
    printf("X\n");

    if (!isLast)
        return;

    // Bottom line
    for (uint i = 0; i < width; i++)
        printf("XX");
    printf("X\n");
}

/**
 * Draw a row for a maze that is ascii based with sharing colums.
 * Place a buffer of size BUFFER to the left of the maze.
 *     ________________________
 *    |                       |
 *    |  ___    __    ______  |
 *    |     |  |  |  |        |
 *    |___  |  |  |  |  ___   |
 *    |     |     |  |     |  |
 *    |_____|_____|__|_____|__|
 *
 * More complicated than outputBlock because it the top row char
 * will change depending on the cell in the previous row.
 *
 *  @param isLast if this row is the last one.
 *  @param isFirst if this row is the first one.
 */
void outputASCII(bool isLast, bool isFirst)
{
    // Top line
    for (int i = 0; i < BUFFER; i++)
        printf(" ");

    printf(isFirst ? " " : "|");
    for (uint r = 0; r < width; r++) {
        printf((row[r] & UP) ? "  " : "__");
        if (previousRow[r] & RIGHT && !(row[r] & RIGHT))
            printf(" ");
        else
            printf(!isFirst && !(previousRow[r] & RIGHT) ? "|" : "_");
    }
    printf("\n");

    // Middle line
    for (int i = 0; i < BUFFER; i++)
        printf(" ");

    printf("|");
    for (uint r = 0; r < width; r++) {
        if (debugsets)
            printf("%*d", 2, set[r]);
        else if (debugrows)
            printf("%*d", 2, row[r]);
        else
            printf("  ");
        printf((row[r] & RIGHT) ? " " : "|");
    }
    printf("\n");

    // If this is the last row in the maze then fill in the bottom line.
    if (!isLast)
        return;

    for (int i = 0; i < BUFFER; i++)
        printf(" ");

    for (uint r = 0; r < width; r++) {
        printf((row[r] & LEFT) ? "_" : "|");
        printf("__");
    }
    printf("|\n");
}

/**
 * Merge set b into set a in sets set
 */
void unionSet(uint a, uint b)
{
    for (uint i = 0; i < width; i++) {
        if (set[i] == b)
            set[i] = a;
    }
}

/**
 * Create and print out a row.
 * @param isLast if this is the last row
 */
void makeRow(bool isLast)
{
    uint startingSetNum = 1;
    // Make sure each cell is in a set and save the previousRow
    for (uint r = 0; r < width; r++) {
        previousRow[r] = row[r];
        if ((row[r] & DOWN))
            row[r] = UP;
        else {
            // Find the lowest set number that isn't already taken
            bool foundNext = false;
            while (!foundNext) {
                bool found = false;
                for (uint r = 0; r < width; r++) {
                    if (set[r] == startingSetNum) {
                        startingSetNum++;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    foundNext = true;
            }
            set[r] = startingSetNum;
            row[r] = EMPTY;
        }
    }

    // Randomly fill in the cells with connections down or to the left
    for (uint i = 0; i < width; i++) {
        if (rand() % 2 == 1) {
            if (i > 0 && set[i] != set[i - 1]) {
                row[i] |= LEFT;
                row[i - 1] |= RIGHT;
                unionSet(set[i], set[i - 1]);
            }
        }
        if ((rand() % 2 == 1) && !isLast) {
            row[i] |= DOWN;
        }
    }

    // If there are any sets that don't move down in this row,
    // make them go down.
    if (!isLast) {
        for (uint r = 0; r < width; r++) {
            if (row[r] & DOWN)
                continue;
            uint mset = set[r];
            bool goDown = true;
            for (uint i = 0; i < width; i++) {
                if (set[i] == mset && row[i] & DOWN) {
                    goDown = false;
                    break;
                }
            }
            if (goDown) {
                row[r] |= DOWN;
            }
        }
    }

    // last row, merge all sets so there is a path from any point
    // to any other point (sense they are all in one set)
    if (isLast) {
        for (uint r = 0; r < width - 1; r++) {
            if (set[r] == set[r + 1])
                continue;
            row[r] |= RIGHT;
            row[r + 1] |= LEFT;
            unionSet(set[r + 1], set[r]);
        }
    }
}

/**
 * Read in paramaters and output a maze line by line.
 */
int main(int argc, char *argv[])
{
    // read in paramaters
    if (argc < 3) {
        fprintf(stderr, "Usage: %s [width] [height] [OPTIONS]\n", argv[0]);
        fprintf(stderr, "\ta  - ASCII style maze (default).\n");
        fprintf(stderr, "\tb  - BLOCK style maze.\n");
        fprintf(stderr, "\tds - Turn set debug on.\n");
        fprintf(stderr, "\tdr - Turn row debug on.\n");
        fprintf(stderr, "\tr  - Turn off random generation.\n");
        return 1;
    }

    // Read in required args
    width = atoi(argv[1]);
    uint height = atoi(argv[2]);

    // Check to make sure they are valid
    if (width == 0 || height == 0) {
        fprintf(stderr, "Maze width and height must be greater then 0.\n");
        return 1;
    }

    MAZETYPE type = ASCII;
    debugsets = false;
    srand(time(NULL));

    // Read in optional args
    for (int i = 2; i < argc; i++) {
        if (0 == strcmp(argv[i], "ds"))
            debugsets = true;

        if (0 == strcmp(argv[i], "dr"))
            debugrows = true;

        // "Turn off" randomness
        if (0 == strcmp(argv[i], "r"))
            srand(1);

        if (0 == strcmp(argv[i], "a"))
            type = ASCII;

        if (0 == strcmp(argv[i], "b"))
            type = BLOCK;
    }

    // Create/init vars
    set = new uint[width];
    row = new uint[width];
    previousRow = new uint[width];
    for (uint i = 0; i < width; i++) {
        set[i] = i + width + 1;
        row[i] = 0;
        previousRow[i] = 0;
    }

    // create & print out the rows
    bool isLast, isFirst;
    for (uint i = 0; i < height; i++) {
        isLast = (i == height - 1);
        isFirst = (i == 0);
        makeRow(isLast);
        if (type == ASCII)
            outputASCII(isLast, isFirst);
        else if (type == BLOCK)
            outputBlock(isLast);
    }

    // Memory cleanup;
    delete[]set;
    delete[]row;
    delete[]previousRow;

    return 0;
}

