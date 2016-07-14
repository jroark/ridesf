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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <curl/curl.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

#include "shapefil.h"

#define ELEVATION_URL "http://ws.geonames.org/srtm3?lat=%.6f&lng=%.6f"

typedef struct _ElevationData {
    unsigned char *data;
    size_t size;

    int elevation; /* in meters */
} ElevationData, *pElevationData;

/* globals */
SHPHandle g_hSHP = NULL;
DBFHandle g_hDBF = NULL;
DBFHandle g_hDBFOut = NULL;
CURL *g_hCurl = NULL;

int g_stage = 0; /* */

void sigproc() {
    if (!g_stage) {
        if (g_hSHP)
            SHPClose(g_hSHP);
        if (g_hDBF)
            DBFClose(g_hDBF);
        if (g_hDBFOut)
            DBFClose(g_hDBFOut);
        if (g_hCurl)
            curl_easy_cleanup(g_hCurl);
        curl_global_cleanup();

        printf("\nUser pressed Ctrl-c\n");
        exit(0);
    } else /* we're busy */
    {
        /* let us finish writing and cleanup later */
        g_stage = -1;
        signal(SIGINT, sigproc);
    }
}

static size_t sElevationFetchCallback(void *ptr, size_t size, size_t nmemb,
                                      void *data) {
    size_t realsize = size * nmemb;
    pElevationData pEData = (pElevationData)data;

    pEData->data = realloc(pEData->data, pEData->size + realsize + 1);
    if (pEData->data) {
        memcpy(&(pEData->data[pEData->size]), ptr, realsize);
        pEData->size += realsize;
        pEData->data[pEData->size] = 0;
    }
    return realsize;
}

static int sFindElevations(SHPObject *polyline, int index, DBFHandle hDBF,
                           DBFHandle hDBFOut, int s_elev, int e_elev,
                           CURL *hCurl) {
    char szUrl[256];
    ElevationData eData = {0, 0, 0.0};

    /* set the options */
    curl_easy_setopt(hCurl, CURLOPT_WRITEFUNCTION, sElevationFetchCallback);
    curl_easy_setopt(hCurl, CURLOPT_WRITEDATA, (void *)&eData);
    curl_easy_setopt(hCurl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    if (polyline->nVertices > 2)
        printf("Polyline %d has %d vertices\n", index, polyline->nVertices);

    g_stage = 1; /* */
    memset(szUrl, 0, 256);
    snprintf(szUrl, 255, ELEVATION_URL, polyline->padfY[0], polyline->padfX[0]);
    printf("Finding elevation of start point %.6f, %.6f... ",
           polyline->padfY[0], polyline->padfX[0]);
    curl_easy_setopt(hCurl, CURLOPT_URL, szUrl);
    curl_easy_perform(hCurl);
    if (eData.data) {
        eData.elevation = atoi((char *)eData.data);
        printf("%d\n", eData.elevation);
        DBFWriteIntegerAttribute(hDBFOut, index, s_elev, eData.elevation);
    }

    memset(szUrl, 0, 256);
    memset(&eData, 0, sizeof(ElevationData));
    snprintf(szUrl, 255, ELEVATION_URL,
             polyline->padfY[polyline->nVertices - 1],
             polyline->padfX[polyline->nVertices - 1]);
    printf("Finding elevation of end point %.6f, %.6f... ",
           polyline->padfY[polyline->nVertices - 1],
           polyline->padfX[polyline->nVertices - 1]);
    curl_easy_setopt(hCurl, CURLOPT_URL, szUrl);
    curl_easy_perform(hCurl);
    if (eData.data) {
        eData.elevation = atoi((char *)eData.data);
        printf("%d\n", eData.elevation);
        DBFWriteIntegerAttribute(hDBFOut, index, e_elev, eData.elevation);
    }

    if (g_stage == -1) {
        g_stage = 0;
        sigproc();
    }

    g_stage = 0;

    return 0;
}

int main(int argc, char *argv[]) {
    char *pszTmp = NULL;

    if (argc != 2) {
        fprintf(stderr, "\nReads in a shapefile of polylines and finds "
                        "elevation of the start and end points\n");
        fprintf(stderr, "and updates the associated dbf.\n");
        fprintf(stderr, "\nUsage: %s <shapefile>\n\n", argv[0]);
        exit(-1);
    }

    if ((pszTmp = strstr(argv[1], ".shp")) != NULL)
        *pszTmp = 0;

    /* initialize CURL */
    curl_global_init(CURL_GLOBAL_ALL);
    g_hCurl = curl_easy_init();

    if (!g_hCurl) {
        fprintf(stderr, "Error: unable to initialize curl!\n");
        exit(-1);
    }

    g_hDBF = DBFOpen(argv[1], "rb");
    if (g_hDBF) {
        g_hDBFOut = DBFCreate("elevations.dbf");

        if (g_hDBFOut) {
            int k = 0, iFields = 0;

            iFields = DBFGetFieldCount(g_hDBF);
            for (k = 0; k < iFields; k++) {
                char pszFieldName[12] = {0};
                int iWidth = 0;
                int iDecimals = 0;
                DBFFieldType type = DBFGetFieldInfo(g_hDBF, k, pszFieldName,
                                                    &iWidth, &iDecimals);

                DBFAddField(g_hDBFOut, pszFieldName, type, iWidth,
                            (type == FTDouble) ? iDecimals : 0);
            }

            // DBFWriteHeader (g_hDBFOut);
        } else {
            fprintf(stderr, "Error: Unable to create elevations.dbf!\n");
            DBFClose(g_hDBF);
            curl_easy_cleanup(g_hCurl);
            curl_global_cleanup();
            exit(-1);
        }
    } else {
        fprintf(stderr, "Error: Unable to open %s.dbf!\n", argv[1]);
        curl_easy_cleanup(g_hCurl);
        curl_global_cleanup();
        exit(-1);
    }

    /* open the shpfile */
    g_hSHP = SHPOpen(argv[1], "rb");
    if (g_hSHP && g_hDBF && g_hDBFOut) {
        int elevations = 0;
        int i = 0;
        int k = 0, iFields = 0;
        int nEntities = 0, nShapetype = 0;

        double adfMinBound[4] = {0}, adfMaxBound[4] = {0};
        SHPObject *pSHPObject = NULL;

        signal(SIGINT, sigproc);
        SHPGetInfo(g_hSHP, &nEntities, &nShapetype, adfMinBound, adfMaxBound);

        if (nShapetype == SHPT_ARC) {
            printf("\n%s.shp contains %d polylines\nmin bounds %f,%f\nmax "
                   "bounds %f,%f\n",
                   argv[1], nEntities, adfMinBound[0], adfMinBound[1],
                   adfMaxBound[0], adfMaxBound[1]);
        } else {
            fprintf(stderr, "\nError: %s.shp does not contain polylines\n",
                    argv[1]);
            SHPClose(g_hSHP);
            DBFClose(g_hDBF);
            DBFClose(g_hDBFOut);
            curl_easy_cleanup(g_hCurl);
            curl_global_cleanup();
            exit(-1);
        }

        if (nEntities) {
            int s_elev = 0;
            int e_elev = 0;

            s_elev = DBFGetFieldIndex(g_hDBFOut, "s_elev");
            if (s_elev == -1)
                s_elev = DBFAddField(g_hDBFOut, "s_elev", FTInteger, 48, 0);

            e_elev = DBFGetFieldIndex(g_hDBFOut, "e_elev");
            if (e_elev == -1)
                e_elev = DBFAddField(g_hDBFOut, "e_elev", FTInteger, 48, 0);

            iFields = DBFGetFieldCount(g_hDBF);

            /* read in all the entries */
            for (i = 0; i < nEntities; i++) {
                pSHPObject = SHPReadObject(g_hSHP, i);

                /* for every object write all existing fields to the new dbf */
                for (k = 0; k < iFields; k++) {
                    const char *string = NULL;
                    int integer = 0;
                    double dbl = 0;
                    DBFFieldType type =
                        DBFGetFieldInfo(g_hDBF, k, NULL, NULL, NULL);

                    switch (type) {
                    case FTString:
                        string = DBFReadStringAttribute(g_hDBF, i, k);
                        DBFWriteStringAttribute(g_hDBFOut, i, k, string);
                        break;
                    case FTDouble:
                        dbl = DBFReadDoubleAttribute(g_hDBF, i, k);
                        DBFWriteDoubleAttribute(g_hDBFOut, i, k, dbl);
                        break;
                    case FTInteger:
                        integer = DBFReadIntegerAttribute(g_hDBF, i, k);
                        DBFWriteIntegerAttribute(g_hDBFOut, i, k, integer);
                        break;
                    case FTInvalid:
                    default:
                        break;
                    }
                }

                /* now add our new elevation fields */
                if (sFindElevations(pSHPObject, i, g_hDBF, g_hDBFOut, s_elev,
                                    e_elev, g_hCurl))
                    elevations++;
                SHPDestroyObject(pSHPObject);
                pSHPObject = NULL;
            }
        }

        DBFClose(g_hDBF);
        DBFClose(g_hDBFOut);
        SHPClose(g_hSHP);
    } else {
        fprintf(stderr, "\nError: unable to open \"%s\"\n", argv[1]);
        if (g_hDBF)
            DBFClose(g_hDBF);
        if (g_hSHP)
            SHPClose(g_hSHP);
        if (g_hDBFOut)
            DBFClose(g_hDBFOut);
        curl_easy_cleanup(g_hCurl);
        curl_global_cleanup();
        exit(-1);
    }

    /* cleanup CURL */
    curl_easy_cleanup(g_hCurl);
    curl_global_cleanup();

    return 0;
}
