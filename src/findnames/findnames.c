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
#include <libxml/parser.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

#include "shapefil.h"

#define RGEOCODE_URL                                                           \
    "http://ws.geonames.org/findNearbyStreets?lat=%.6f&lng=%.6f&style=full"
#define FIELDNAME (const char *)"stname"

typedef struct _RGeocodeData {
    unsigned char *data;
    size_t size;

    xmlChar **ppStNames; /* double array of NULL term'd st names */
    int iStNamesCount;   /* count of st names */
    int *piStNameOccurs; /* array of counts of st name occurances */
    double *pfDistance;  /* array of total distances */
} RGeocodeData, *pRGeocodeData;

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

inline double deg2rad(double deg) { return ((deg * M_PI) / 180); }

inline double rad2deg(double rad) { return ((rad * 180) / M_PI); }

inline double rad2brng(double rad) { return fmod((rad2deg(rad) + 360), 360); }

inline double getBearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = deg2rad(lon2) - deg2rad(lon1);
    double x = cos(deg2rad(lat1)) * sin(deg2rad(lat2)) -
               sin(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(dLon);
    double y = sin(dLon) * cos(deg2rad(lat2));

    return rad2brng(atan2(y, x));
}

static size_t sRGeoFetchCallback(void *ptr, size_t size, size_t nmemb,
                                 void *data) {
    size_t realsize = size * nmemb;
    pRGeocodeData pRGData = (pRGeocodeData)data;

    pRGData->data = realloc(pRGData->data, pRGData->size + realsize + 1);
    if (pRGData->data) {
        memcpy(&(pRGData->data[pRGData->size]), ptr, realsize);
        pRGData->size += realsize;
        pRGData->data[pRGData->size] = 0;
    }
    return realsize;
}

/* parse string of form "lon1 lat1,lon2 lat2,...,lonN latN"
   returns a double array of lon/lat pairs (0 index is count)
 */
static double *sParseLine(xmlChar *pszLine) {
    char *pszTmp = (char *)pszLine;
    char *comma = NULL;
    char *space = NULL;
    double lat = 0, lon = 0, *coords = NULL;
    int count = 0;

    coords = (double *)malloc(sizeof(double));
    if (!coords)
        return NULL;

    count = 1;

    while ((comma = strchr(pszTmp, ',')) != NULL) {
        *comma = 0;
        if ((space = strchr(pszTmp, ' ')) != NULL) {
            *space = 0;
            lon = atof(pszTmp);
            lat = atof(space + 1);

            if (lon && lat) {
                count += 2;
                coords = (double *)realloc(coords, sizeof(double) * count);
                if (coords) {
                    coords[count - 1] = lat;
                    coords[count - 2] = lon;
                }
            }

            lat = lon = 0;
        }
        pszTmp = comma + 1;
    }

    coords[0] = (double)count;
    return coords;
}

double *midPoint(double lat1, double lon1, double lat2, double lon2) {
    static double mid[2] = {0.0, 0.0};
    double Bx, By, lat3, lon3;
    double dLon = deg2rad(lon2 - lon1);

    lat1 = deg2rad(lat1);
    lat2 = deg2rad(lat2);

    Bx = cos(lat2) * cos(dLon);
    By = cos(lat2) * sin(dLon);

    lat3 = atan2(sin(lat1) + sin(lat2),
                 sqrt((cos(lat1) + Bx) * (cos(lat1) + Bx) + By * By));
    lon3 = deg2rad(lon1) + atan2(By, cos(lat1) + Bx);

    mid[0] = rad2deg(lat3);
    mid[1] = rad2deg(lon3);

    return mid;
}

static int sParseRGeocodeXML(pRGeocodeData pgcData, double brng) {
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL;
    xmlNodePtr tmp = NULL;
    int ret = 0;

    xmlKeepBlanksDefault(0);
    doc = xmlParseMemory((const char *)pgcData->data, pgcData->size);

    if (doc) {
        root = xmlDocGetRootElement(doc);
        if (root && !xmlStrcmp(root->name, (const xmlChar *)"geonames")) {
            double shortDist = 1024.123456;
            xmlChar *pszShortName = NULL;

            tmp = root->children;
            while (tmp &&
                   !xmlStrcmp(tmp->name, (const xmlChar *)"streetSegment")) {
                xmlChar *pszStName = NULL;
                xmlChar *pszDist = NULL;
                xmlNodePtr distNode = tmp->children;
                double dist = 0.000000;
                double bearing = 0.000000;
                /* find the name & distance nodes */
                while (distNode) {
                    if (!xmlStrcmp(distNode->name, (const xmlChar *)"line")) {
                        double *coords = NULL;
                        xmlChar *pszLine = xmlNodeGetContent(distNode);

                        if (pszLine) {
                            /* parse the line <line>lon1 lat1,lon2 lat2,...,lonN
                             * latN</line>*/
                            coords = sParseLine(pszLine);
                            /* get the bearing */
                            /* compare bearing to source bearing */
                            if (coords) {
                                int count = (int)coords[0];
                                /* simple case */
                                if (coords[0] == 5) {
                                    bearing = getBearing(coords[2], coords[1],
                                                         coords[4], coords[3]);
                                }
                                free(coords);
                            }
                            xmlFree(pszLine);
                        }
                    } else if (!xmlStrcmp(distNode->name,
                                          (const xmlChar *)"name"))
                        pszStName = xmlNodeGetContent(distNode);
                    else if (!xmlStrcmp(distNode->name,
                                        (const xmlChar *)"distance")) {
                        pszDist = xmlNodeGetContent(distNode);
                        if (pszDist) {
                            dist = atof((char *)pszDist);
                            xmlFree(pszDist);
                            pszDist = NULL;
                        }
                    }
                    distNode = distNode->next;
                }
                printf(
                    "\n%s is %.4f from our point and has a bearing of %.4f\n",
                    pszStName, dist, bearing);

                /* keep track of the closest street */
                if (dist < shortDist) {
                    shortDist = dist;
                    pszShortName = pszStName;
                } else {
                    xmlFree(pszStName);
                    pszStName = NULL;
                }

                /* add the st name to the pgcData stucture and keep a count of
                 * each name */
                if (pszStName) {
                    int i = 0;
                    int found = 0;

                    /* store the name */
                    for (i = 0; i < pgcData->iStNamesCount; i++) {
                        if (!xmlStrcmp(pgcData->ppStNames[i], pszStName)) {
                            pgcData->piStNameOccurs[i]++;
                            pgcData->pfDistance[i] += dist;
                            found++;
                            xmlFree(pszStName);
                            pszStName = NULL;
                            ret = 1;
                            break;
                        }
                    }

                    if (!found) {
                        pgcData->ppStNames = (xmlChar **)realloc(
                            pgcData->ppStNames,
                            (sizeof(xmlChar *) * (pgcData->iStNamesCount + 1)));
                        pgcData->piStNameOccurs = (int *)realloc(
                            pgcData->piStNameOccurs,
                            (sizeof(int) * (pgcData->iStNamesCount + 1)));
                        pgcData->pfDistance = (double *)realloc(
                            pgcData->pfDistance,
                            (sizeof(double) * (pgcData->iStNamesCount + 1)));
                        if (pgcData->ppStNames && pgcData->piStNameOccurs &&
                            pgcData->pfDistance) {
                            int count = pgcData->iStNamesCount;

                            pgcData->iStNamesCount++;
                            pgcData->ppStNames[count] = pszStName;
                            pgcData->piStNameOccurs[count] = 1;
                            pgcData->pfDistance[count] = dist;
                            pszStName = NULL;
                            ret = 1;
                        }
                    }
                }

                tmp = tmp->next;
            }
        }

        xmlFreeDoc(doc);
    }

    return ret;
}

static int sRGeoCodePolyline(SHPObject *polyline, int index, DBFHandle hDBF,
                             DBFHandle hDBFOut, int field, CURL *hCurl) {
    static RGeocodeData gcData;
    char szUrl[256];
    int i = 0;

    gcData.data = NULL;
    gcData.size = 0;
    gcData.ppStNames = NULL;
    gcData.piStNameOccurs = NULL;
    gcData.pfDistance = NULL;
    gcData.iStNamesCount = 0;

    /* set the options */
    curl_easy_setopt(hCurl, CURLOPT_WRITEFUNCTION, sRGeoFetchCallback);
    curl_easy_setopt(hCurl, CURLOPT_WRITEDATA, (void *)&gcData);
    curl_easy_setopt(hCurl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    g_stage = 1; /* */
    for (i = 0; i < (polyline->nVertices - 1); i++) {
        /* use the center point */
        /* this should help us avoid intersections */
        double *mid = midPoint(polyline->padfY[i], polyline->padfX[i],
                               polyline->padfY[i + 1], polyline->padfX[i + 1]);
        double brng =
            getBearing(polyline->padfY[i], polyline->padfX[i],
                       polyline->padfY[i + 1], polyline->padfX[i + 1]);

        memset(szUrl, 0, 256);
        snprintf(szUrl, 255, RGEOCODE_URL, mid[0], mid[1]);

        printf("Reverse GeoCoding %.6f, %.6f with a bearing of %.4f... ",
               mid[0], mid[1], brng);

        curl_easy_setopt(hCurl, CURLOPT_URL, szUrl);

        /* retrive the xml */
        curl_easy_perform(hCurl);

        /* parse xml */
        if (gcData.data) {
            if (!sParseRGeocodeXML(&gcData, brng))
                ;
            /*	printf (" failed!\n\n");
            else
                    printf (" done\n");*/

            free(gcData.data);
            gcData.data = NULL;
            gcData.size = 0;
        }
    }

    /* gcData should contain a list of possible streetnames */
    if (gcData.ppStNames && gcData.piStNameOccurs && gcData.pfDistance) {
        int j = 0;
        int guess = -1;
        double shortDist = 1024.123456;

        for (j = 0; ((j < gcData.iStNamesCount) && gcData.piStNameOccurs[j] &&
                     gcData.ppStNames[j] && gcData.pfDistance[j]);
             j++) {
            if ((double)(gcData.pfDistance[j] / gcData.piStNameOccurs[j]) <
                shortDist) {
                guess = j;
                shortDist =
                    (double)(gcData.pfDistance[j] / gcData.piStNameOccurs[j]);
            }
        }

        if (guess != -1) {
            int k = 0, iFields = 0;

            iFields = DBFGetFieldCount(hDBF);
            for (k = 0; k < iFields; k++) {
                const char *string = NULL;
                int integer = 0;
                double dbl = 0;
                DBFFieldType type = DBFGetFieldInfo(hDBF, k, NULL, NULL, NULL);

                if (k == field)
                    continue;

                switch (type) {
                case FTString:
                    string = DBFReadStringAttribute(hDBF, index, k);
                    DBFWriteStringAttribute(hDBFOut, index, k, string);
                    break;
                case FTDouble:
                    dbl = DBFReadDoubleAttribute(hDBF, index, k);
                    DBFWriteDoubleAttribute(hDBFOut, index, k, dbl);
                    break;
                case FTInteger:
                    integer = DBFReadIntegerAttribute(hDBF, index, k);
                    DBFWriteIntegerAttribute(hDBFOut, index, k, integer);
                    break;
                case FTInvalid:
                default:
                    break;
                }
            }

            printf("Guessing %s (average distance of %.6f miles)\n\n",
                   gcData.ppStNames[guess], shortDist);
            DBFWriteStringAttribute(hDBFOut, index, field,
                                    (char *)gcData.ppStNames[guess]);
        } else {
            /* failed to find a st name? */
            printf("Failed to find a St. Name!\n\n");
        }

        for (j = 0; j < gcData.iStNamesCount; j++)
            xmlFree(gcData.ppStNames[j]);

        free(gcData.ppStNames);
        free(gcData.pfDistance);
        free(gcData.piStNameOccurs);
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
        fprintf(stderr, "\nReads in a shapefile of polylines and reverse "
                        "geocodes the streetnames\n");
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
        g_hDBFOut = DBFCreate("streetnames.dbf");

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
        } else {
            fprintf(stderr, "Error: Unable to create streetnames.dbf!\n");
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
        int streets = 0;
        int i = 0;
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
            int field = 0;

            field = DBFGetFieldIndex(g_hDBFOut, FIELDNAME);
            if (field == -1)
                field = DBFAddField(g_hDBFOut, FIELDNAME, FTString, 48, 0);

            /* read in all the entries */
            for (i = 0; i < nEntities; i++) {
                pSHPObject = SHPReadObject(g_hSHP, i);
                if (sRGeoCodePolyline(pSHPObject, i, g_hDBF, g_hDBFOut, field,
                                      g_hCurl))
                    streets++;
                SHPDestroyObject(pSHPObject);
                pSHPObject = NULL;
            }
        }

        printf("Wrote %d streetnames to %s.dbf\n", streets, argv[2]);

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
