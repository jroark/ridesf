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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shapefil.h"

static int sSegmentPolyline(SHPObject *polyline, int index, SHPHandle hSHPOut,
                            DBFHandle hDBF, DBFHandle hDBFOut) {
    int i = 0;
    int k = 0, iFields = 0;
    double x[2] = {0, 0}, y[2] = {0, 0}, z[2] = {0, 0};
    SHPObject *shpObject = NULL;

    iFields = DBFGetFieldCount(hDBF);
    for (i = 0; i < (polyline->nVertices - 1); i++) {
        x[0] = polyline->padfX[i];
        x[1] = polyline->padfX[i + 1];
        y[0] = polyline->padfY[i];
        y[1] = polyline->padfY[i + 1];
        shpObject = SHPCreateSimpleObject(SHPT_ARC, 2, x, y, z);
        if (shpObject) {
            int entity = SHPWriteObject(hSHPOut, -1, shpObject);
            SHPDestroyObject(shpObject);
            shpObject = NULL;

            for (k = 0; k < iFields; k++) {
                const char *string = NULL;
                int integer = 0;
                double dbl = 0;
                DBFFieldType type = DBFGetFieldInfo(hDBF, k, NULL, NULL, NULL);

                switch (type) {
                case FTString:
                    string = DBFReadStringAttribute(hDBF, index, k);
                    DBFWriteStringAttribute(hDBFOut, entity, k, string);
                    break;
                case FTDouble:
                    dbl = DBFReadDoubleAttribute(hDBF, index, k);
                    DBFWriteDoubleAttribute(hDBFOut, entity, k, dbl);
                    break;
                case FTInteger:
                    integer = DBFReadIntegerAttribute(hDBF, index, k);
                    DBFWriteIntegerAttribute(hDBFOut, entity, k, integer);
                    break;
                case FTInvalid:
                default:
                    break;
                }
            }
        }
    }

    return i;
}

int main(int argc, char *argv[]) {
    char *pszTmp = NULL;
    DBFHandle hDBF = NULL;
    DBFHandle hDBFOut = NULL;
    SHPHandle hSHP = NULL;
    SHPHandle hSHPOut = NULL;

    if (argc != 3) {
        fprintf(stderr, "\nReads in a shapefile of polylines and breaks them "
                        "apart into two point line segments\n");
        fprintf(stderr, "\nUsage: %s <shapefile> <outfile>\n\n", argv[0]);
        exit(-1);
    }

    if ((pszTmp = strstr(argv[1], ".shp")) != NULL)
        *pszTmp = 0;

    hDBF = DBFOpen(argv[1], "rb");
    if (hDBF) {
        char szDBFOut[256];

        memset(szDBFOut, 0, 256);
        snprintf(szDBFOut, 255, "%s.dbf", argv[2]);
        hDBFOut = DBFCreate(argv[2]);

        if (hDBFOut) {
            int k = 0, iFields = 0;

            iFields = DBFGetFieldCount(hDBF);
            for (k = 0; k < iFields; k++) {
                char pszFieldName[12] = {0};
                int iWidth = 0;
                int iDecimals = 0;
                DBFFieldType type =
                    DBFGetFieldInfo(hDBF, k, pszFieldName, &iWidth, &iDecimals);

                DBFAddField(hDBFOut, pszFieldName, type, iWidth,
                            (type == FTDouble) ? iDecimals : 0);
            }
        } else {
            fprintf(stderr, "Error: Unable to create %s.dbf!\n", argv[2]);
            DBFClose(hDBF);
            exit(-1);
        }
    } else {
        fprintf(stderr, "Error: Unable to open %s.dbf!\n", argv[1]);
        exit(-1);
    }

    /* open the shpfile */
    hSHP = SHPOpen(argv[1], "rb");
    if (hSHP && hDBF && hDBFOut) {
        int count = 0;
        int i = 0;
        int nEntities = 0, nShapetype = 0;
        char szSHPOut[256];

        double adfMinBound[4] = {0}, adfMaxBound[4] = {0};
        SHPObject *pSHPObject = NULL;

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
            DBFClose(hDBFOut);
            exit(-1);
        }

        memset(szSHPOut, 0, 256);
        snprintf(szSHPOut, 255, "%s.shp", argv[2]);
        hSHPOut = SHPCreate(szSHPOut, SHPT_ARC);

        if (!hSHPOut) {
            fprintf(stderr, "\nError: unable to create %s.shp\n", argv[2]);
            SHPClose(hSHP);
            DBFClose(hDBF);
            DBFClose(hDBFOut);
            exit(-1);
        }

        if (nEntities) {
            /* read in all the entries */
            for (i = 0; i < nEntities; i++) {
                pSHPObject = SHPReadObject(hSHP, i);
                count +=
                    sSegmentPolyline(pSHPObject, i, hSHPOut, hDBF, hDBFOut);
                SHPDestroyObject(pSHPObject);
                pSHPObject = NULL;
            }
        }

        DBFClose(hDBF);
        DBFClose(hDBFOut);
        SHPClose(hSHP);
        SHPClose(hSHPOut);
    } else {
        fprintf(stderr, "\nError: unable to open \"%s\"\n", argv[1]);
        if (hDBF)
            DBFClose(hDBF);
        if (hSHP)
            SHPClose(hSHP);
        if (hDBFOut)
            DBFClose(hDBFOut);
        exit(-1);
    }

    return 0;
}
