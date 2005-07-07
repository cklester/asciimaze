/**
 *  Copyright (C) 2005 Benjamin C Meyer (ben at meyerhome dot net)
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
 * ASCII A-maze-ment
 *
 * The problem:
 *   Write a program that reads in a simple 2-D maze in a format like this: 

      _______________________END
     |     |        |        |
     |__   |_____   |  ______|
     |        |        |     |
     |  ___   |_____   |     |
     |  |  |           |  |  |
     |  |  |  ___      |  |  |
     |  |        |  |  |  |  |
     |  |_____   |__|__|__|  |
     |  |                    |
Start|__|____________________|

 * The input is guaranteed to be a well-formed maze and to have a unique 
 * solution path from the bottom left grid cell to the top right grid cell.
 * Your program should re-output the maze with the solution path filled in, e.g.: 

      _______________________END
     |     |        |XX XX XX|
     |__   |_____   |  ______|
     |XX XX XX|      XX|     |
     |  ___   |_____   |     |
     |XX|  |XX XX XX XX|  |  |
     |  |  |  ___      |  |  |
     |XX|        |  |  |  |  |
     |  |_____   |__|__|__|  |
     |XX|                    |
Start|__|____________________|

 * Analasis:
 *
 * The best part about this problem is that unlike almost all graph problems
 * it has a unique path.  This means that there is only *one* way through the
 * maze which is also the shortest.  It is because of this feature that I
 * found this perticular problem interesting. 
 *
 * Solution:
 *
 * Breaking it down into three parts:
 * 1) Reading in the graph.
 * 2) Computing route.
 * 3) Output the graph.
 *
 * One could find the path while reading in the map, but the amount of data
 * checking as each layer was read in would make this very memory intensive
 * especially if you are checking against char/string data.  Either way step
 * 1 and 3 aren't what this puzzle is about so the reading and writing should
 * be simple generic functions that translate the ascii data into an internal
 * more optimized structure.  This has the nice side effect of allowing
 * modifications in only those two places to support any maze format.  One
 * could even takes this a step further and have this program input a
 * compressed maze and a wrapper shell script convert ascii maze data into
 * that format. QString's were used to store the data because they are simple
 * to use and not what the point of the problem was about.
 *
 * Enough on that, onto the more juicy stuff.  With the constrains that the 
 * maze will always be in a grid the most obvious choice for the internal data
 * is an array of data pointers.   Also because in a maze you never want to go
 * over the same area twice this turns into a directed graph.
 * 
 * Recursively  back track:
 *
 * This will find a solution, but it won't necessarily find the shortest
 * solution if there is more then one solution (i.e. a braid). It is fast for 
 * all types of mazes, and uses stack space up to the size of the Maze. If at 
 * a wall (or an area already plotted), return failure, else if at the finish,
 * return success, else recursively try moving in the four directions.  If 
 * returning success plot the current postion as part of the soltution path.
 */

#include <stdio.h>
#include <readline/readline.h>
#include <qstringlist.h>
#include <qvector.h>
#include <qtextstream.h>

// Buffer before maze starts
#define BUFFER 5
// char to use when filling in the path
#define PATHMARKER 'X'

// The four properties a cell can have
#define EMPTY 0
#define UP    1
#define DOWN  2
#define LEFT  4
#define RIGHT 8

// Technically not in this problem sense the path is always unique, but useful
// for other mazes
#define CHECKEDFORBRAIDS

#ifdef CHECKEDFORBRAIDS
// Number to use when a cell is marked as already checked
#define CHECKED  32
#endif

struct maze {
    // The char list of rows used in reading/writing/solution marking.
    QList < QString > list;
    // width/height of the maze
    uint width, height;
    // A row or short's.  Each short is a cell that can be or'd with the
    // four possible directions (UP | DOWN | LEFT | RIGHT).
      QVector < short *>rows;
    // The destination points
    uint destX, destY;
};

/**
 * Convert three lines of text into one row 
 *     _______________________  <- line a
 *    |     |        |        | <- line b
 *    |__   |_____   |  ______| <- line c
 *
 *    [08|06|08|12|06|10|12|04] <- example return
 *
 *  @param a first row (already calculated last time by the function)
 *  @param b second row
 *  @param c third row
 *  @return array of the row with directions filled in
 */
short *convertRow(short *a, const QString & b, const QString & c, uint width)
{
    // create a new row
    short *row = new short[width];
    // fill each cell
    for (uint i = 0; i < width; i++) {
        // Don't waste time parsing string data, simply use the last row
        // to init row[i] to UP or EMPTY.
        // See if this call can move up
        row[i] = (((a != NULL) && (a[i] & DOWN)) ? UP : EMPTY);

        // See if this cell can move down
        if (c[i * 3 + BUFFER + 1] != '_')
            row[i] |= DOWN;

        // Only parse Left/right data once and apply to both cells
        // See if this cell can move left/right
        if (!(b[i * 3 + BUFFER] == '|')) {
            row[i] |= LEFT;
            row[i - 1] |= RIGHT;
        }
    }
    return row;
}

/**
 * Read in (from stdin) a maze, parse, and fill m
 */
void read(maze * m)
{
    uint lineNumber = 0;
    char *line = 0;
    while ((line = readline(NULL)) != NULL) {
        m->list.append(line);
        // Don't leak memory after saving line.
        free(line);

        if (lineNumber == 1)
            m->width = (m->list[1].length() - BUFFER) / 3;

        // On the third line figuring out a row
        if (lineNumber++ % 2 != 0 || lineNumber <= 2)
            continue;
        short *lastRow = NULL;
        if (lineNumber > 3)
            lastRow = m->rows[lineNumber / 2 - 2];
        short *col = convertRow(lastRow,
                                m->list[lineNumber - 2],
                                m->list[lineNumber - 1], m->width);
        if (m->rows.size() <= (int) (lineNumber / 2))
            m->rows.resize(lineNumber * 4);
        m->rows.insert((lineNumber / 2) - 1, col);
    }
    m->rows.resize(lineNumber / 2);
    m->height = lineNumber / 2 - 1;
}

/**
 * Write maze m to stdout.
 */
void write(maze * m)
{
    QTextStream cout(stdout, QIODevice::WriteOnly);
    QList < QString >::Iterator it;
    for (it = m->list.begin(); it != m->list.end(); ++it) {
        cout << (*it) << endl;
    }
}

/**
 * Mark ascii cell x y in maze m with the marker that it is part of the solution.
 */
inline void solutionCell(maze * m, int x, int y)
{
    m->list[y * 2 + 1][x * 3 + BUFFER + 1] = PATHMARKER;
    m->list[y * 2 + 1][x * 3 + BUFFER + 2] = PATHMARKER;
}

/**
 * Check if the next cell at point (x,y) coming from the previous
 * cell parent the direction from to see if it is the end of maze m.
 * If it isn't try the cells other directions to see if they lead there.
 *
 * @param x - the starting xcord
 * @param y - the starting ycord
 * @param from - direction just came from
 * @return true if x,y is part of the solution.
 */
bool solveMaze(maze * m, uint x, uint y, int from)
{
    // This if shouldn't be needed if read() performed correctly
    //if( m->height < y || x > m->width )
    //      return;

    short cell = m->rows[y][x];

#ifdef CHECKEDFORBRAIDS
    if (cell & CHECKED)
        return false;
    m->rows[y][x] |= CHECKED;
#endif

    if (x == m->destX && y == m->destY) {
        // Don't bother checking its children
        solutionCell(m, x, y);
        return true;
    }

    bool foundEnd = false;

    // Don't go back the way you just came
    if (from != RIGHT && cell & LEFT)
        foundEnd = solveMaze(m, x - 1, y, LEFT);
    if (!foundEnd && (from != DOWN && (cell & UP)))
        foundEnd = solveMaze(m, x, y - 1, UP);
    if (!foundEnd && (from != UP && (cell & DOWN)))
        foundEnd = solveMaze(m, x, y + 1, DOWN);
    if (!foundEnd && (from != LEFT && (cell & RIGHT)))
        foundEnd = solveMaze(m, x + 1, y, RIGHT);

    // If this is on the path mark it
    if (foundEnd)
        solutionCell(m, x, y);

    return foundEnd;
}

/**
 * Read in an ascii maze, solve it and output it with the solution.
 * @return 1 if there is no path found.
 */
int main(int /* argc */ , char * /* argv[] */ )
{
    maze m;
    m.width = m.height = 0;
    m.rows.resize(2);

    // Read the maze
    read(&m);

    // Choose start and ending points for the maze
    m.destX = m.width - 1;
    m.destY = 0;
    uint startX = 0;
    uint startY = m.height;

    // Attempt to find the solution
    int isSolvable = solveMaze(&m, startX, startY, EMPTY);

    if (isSolvable)
        write(&m);
    else
        fprintf(stderr, "No path found through maze.\n");

    // Memory cleanup
    for (uint row = 0; row <= m.height; row++)
        delete[](m.rows[row]);

    return isSolvable;
}
