/*
Copyright (c) 2009, John Roark <jroark@cs.usfca.edu>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.
    * Neither the name of the <ORGANIZATION> nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* Copyright 2008 John Roark <jroark@cs.usfca.edu>
 * cc intersections.c -o intersections -lshp
 */
#include <shapefil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(a, b) ((a < b) ? a : b)
#define max(a, b) ((a > b) ? a : b)

typedef struct _coord {
    double x;
    double y;
} coord, *pcoord;

/* TODO:
 * - load dbf and don't compare polylines with the same cnn
 * - allow intersections for lines that don't quite meet (specify a tolerance)
 * - sort and remove duplicates
 */

static pcoord sIntersectsAt(pcoord pLine1Start, pcoord pLine1End,
                            pcoord pLine2Start, pcoord pLine2End) {
    double A1, B1, C1, A2, B2, C2;
    double det;
    static coord ret = {0, 0};

    memset(&ret, 0, sizeof(coord));

    if (!pLine1Start || !pLine1End || !pLine2Start || !pLine2End)
        return NULL;

    /* Ax + By = C */
    A1 = pLine1End->y - pLine1Start->y;
    B1 = pLine1Start->x - pLine1End->x;
    C1 = (A1 * pLine1Start->x) + (B1 * pLine1Start->y);

    A2 = pLine2End->y - pLine2Start->y;
    B2 = pLine2Start->x - pLine2End->x;
    C2 = (A2 * pLine2Start->x) + (B2 * pLine2Start->y);

    det = (A1 * B2) - (A2 * B1);

    if (det == 0) {
        /* parallel ? */
        return NULL;
    } else {
        ret.x = (B2 * C1 - B1 * C2) / det;
        ret.y = (A1 * C2 - A2 * C1) / det;

        /* check if ret lies within line1 and line2 */
        if ((min(pLine1Start->x, pLine1End->x) <= ret.x) &&
            (ret.x <= max(pLine1Start->x, pLine1End->x)) &&
            (min(pLine1Start->y, pLine1End->y) <= ret.y) &&
            (ret.y <= max(pLine1Start->y, pLine1End->y))) {
            if ((min(pLine2Start->x, pLine2End->x) <= ret.x) &&
                (ret.x <= max(pLine2Start->x, pLine2End->x)) &&
                (min(pLine2Start->y, pLine2End->y) <= ret.y) &&
                (ret.y <= max(pLine2Start->y, pLine2End->y))) {
                return (pcoord)&ret;
            } else
                return NULL;
        } else
            return NULL;
    }

    return NULL;
}

static int sFindAndWriteIntersections(SHPObject **paSHPObjects, int count,
                                      DBFHandle hDBF, SHPHandle hSHPOut) {
    int i = 0;
    int total = 0;

    for (i = 0; i < count; i++) {
        int j = 0;
        for (j = 0; j < (paSHPObjects[i]->nVertices - 1); j++) {
            int k = 0;
            for (k = 0; k < count; k++) {
                int l = 0;
                for (l = 0; l < (paSHPObjects[k]->nVertices - 1); l++) {
                    coord line1Start, line1End;
                    coord line2Start, line2End;
                    pcoord pisec = NULL;

                    if (i != k) /* don't find intersections within the same
                                   polyline */
                    {
                        char *pszTmpName = NULL;
                        char pszStreetName[12];
                        /* read STREETNAME */
                        pszTmpName = DBFReadStringAttribute(hDBF, i, 2);

                        if (pszTmpName)
                            strcpy(pszStreetName, pszTmpName);
                        pszTmpName = DBFReadStringAttribute(hDBF, k, 2);

                        if (strcmp(pszTmpName, pszStreetName)) {
                            line1Start.x = paSHPObjects[i]->padfX[j];
                            line1Start.y = paSHPObjects[i]->padfY[j];
                            line1End.x = paSHPObjects[i]->padfX[j + 1];
                            line1End.y = paSHPObjects[i]->padfY[j + 1];

                            line2Start.x = paSHPObjects[k]->padfX[l];
                            line2Start.y = paSHPObjects[k]->padfY[l];
                            line2End.x = paSHPObjects[k]->padfX[l + 1];
                            line2End.y = paSHPObjects[k]->padfY[l + 1];

                            pisec = sIntersectsAt(&line1Start, &line1End,
                                                  &line2Start, &line2End);

                            if (pisec) {
                                SHPObject *tmpObject = NULL;

                                tmpObject = SHPCreateSimpleObject(
                                    SHPT_POINT, 1, &(pisec->x), &(pisec->y),
                                    NULL);

                                printf("%s & %s @ %f,%f\n", pszTmpName,
                                       pszStreetName, pisec->x, pisec->y);
                                if (tmpObject) {
                                    SHPWriteObject(hSHPOut, -1, tmpObject);
                                    SHPDestroyObject(tmpObject);
                                    total++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return total;
}

int main(int argc, char *argv[]) {
    SHPHandle hSHP = NULL;
    SHPHandle hSHPOut = NULL;

    DBFHandle hDBF = NULL;
    DBFHandle hDBFOut = NULL;

    if (argc != 3) {
        fprintf(stderr, "\nReads in a shapefile of polylines and finds all "
                        "intersections\n");
        fprintf(stderr, "and writes them to a shapefile of points.\n");
        fprintf(stderr, "\nUsage: %s <shapefile> <outfile>\n\n", argv[0]);
        exit(-1);
    }

    /* open the shpfile */
    hSHP = SHPOpen(argv[1], "rb");
    hDBF = DBFOpen(argv[1], "rb");
    if (hSHP && hDBF) {
        int points = 0;
        int i = 0;
        int nEntities = 0, nShapetype = 0;

        double adfMinBound[4] = {0}, adfMaxBound[4] = {0};
        SHPObject **paSHPObjects = NULL;

        SHPGetInfo(hSHP, &nEntities, &nShapetype, adfMinBound, adfMaxBound);

        if (nShapetype == SHPT_ARC) {
            printf("\n%s.shp contains %d polylines\nmin bounds %f,%f\nmax "
                   "bounds %f,%f\n",
                   argv[1], nEntities, adfMinBound[0], adfMinBound[1],
                   adfMaxBound[0], adfMaxBound[1]);
        } else {
            fprintf(stderr, "\nError: %s.shp does not contain polylines\n",
                    argv[1]);
            SHPClose(hSHP);
            DBFClose(hDBF);
            exit(-1);
        }

        if (nEntities) {
            paSHPObjects =
                (SHPObject **)malloc(sizeof(SHPObject *) * nEntities);
            if (paSHPObjects)
                memset(paSHPObjects, 0, sizeof(SHPObject *) * nEntities);
            else {
                fprintf(stderr,
                        "\nError: unable to allocate %d bytes of memory\n",
                        sizeof(SHPObject *) * nEntities);
                SHPClose(hSHP);
                DBFClose(hDBF);
                exit(-1);
            }
        }

        /* read in all the entries */
        for (i = 0; i < nEntities; i++)
            paSHPObjects[i] = SHPReadObject(hSHP, i);

        hSHPOut = SHPCreate(argv[2], SHPT_POINT);
        if (!hSHPOut) {
            fprintf(stderr, "\nError: unable to create %s.shp\n", argv[2]);
            for (i = 0; i < nEntities; i++)
                if (paSHPObjects[i])
                    SHPDestroyObject(paSHPObjects[i]);
            free(paSHPObjects);
            SHPClose(hSHP);
            DBFClose(hDBF);
            exit(-1);
        }

        /* find all the intersections */
        points =
            sFindAndWriteIntersections(paSHPObjects, nEntities, hDBF, hSHPOut);
        SHPClose(hSHPOut);

        printf("Wrote %d intersections to %s.shp\n", points, argv[2]);

        /* cleanup time */
        for (i = 0; i < nEntities; i++)
            if (paSHPObjects[i])
                SHPDestroyObject(paSHPObjects[i]);

        free(paSHPObjects);

        SHPClose(hSHP);
        DBFClose(hDBF);
    } else {
        fprintf(stderr, "\nError: unable to open \"%s.shp\"\n", argv[1]);
        if (hSHP)
            SHPClose(hSHP);
        if (hDBF)
            DBFClose(hDBF);
        exit(-1);
    }

    return 0;
}
