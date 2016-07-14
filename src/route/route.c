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

/* route.cgi
 * (C) John Roark <jroark@cs.usfca.edu> 2008
 *
 * Options:
 *	fmt:		(JSON | GPX | KML | WKT | JML)
 *	combine:	(1 | 0)
 *	sort:		(1 | 0)
 *	dl:		(0 | 1)
 *	link:		(1 | 0)
 *	scoord:		(lat, lon)
 *	ecoord:		(lat, lon)
 *	db:		(Database Name)
 *	tbl:		(DB Table)
 */
#include <cgi.h>
#include <libpq-fe.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define fmax(x, y) ((x > y) ? x : y)
#define fmin(x, y) ((x < y) ? x : y)
#define KMTOMI(x) (x * 0.621371192)
#define MITOFT(x) (x * 5280.00)
#define LATLNGEQUAL(x, y) ((x.lat == y.lat) && (x.lon == y.lon))

typedef enum {
    fmtGPX = 0,
    fmtKML = 1,
    fmtKMZ = 2,
    fmtSHP = 3,
    fmtJSON = 4,
    fmtWKT = 5,
    fmtJML = 6,
    fmtCODE = 7
} fmt_t;

typedef enum {
    North = 0,
    NorthEast = 1,
    East = 2,
    SouthEast = 3,
    South = 4,
    SouthWest = 5,
    West = 6,
    NorthWest = 7
} cardinalPoint_t;

typedef struct _LatLng {
    double lat;
    double lon;
} LatLng, *pLatLng;

typedef struct _PathSegment {
    char streetname[128];
    char facility_t[32];
    double distance;
    double bearing;
    int gid;
    int bikelane;
    int count;
    pLatLng pLine;
    cardinalPoint_t direction;
} PathSegment, *pPathSegment;

int g_dl = 0;   // download the file? (default to no)
int g_link = 1; // create links to path segments? (default to yes)

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

/* calculates distance in km (use KMTOMI to converto to miles) */
double distHaversine(double lat1, double lon1, double lat2, double lon2) {
    // 0.621371192 miles in a km
    double R = 6371; // earth's mean radius in km
    double dLat = deg2rad(lat1 - lat2);
    double dLon = deg2rad(lon1 - lon2);

    double a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double d = R * c;

    return d;
}

void swapLatLng(pLatLng a, int i, int j) {
    LatLng tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
}
/* quicksort for latlng array
 */
// quicksort ((*segments)[i].pLine, 0, (*segments)[i].count-1);
/* TODO: could probably write a custom heuristic for partitioning, but these
arrays are short
inline int randIndex(int i, int j)
{
        return i + rand() % (j-i+1);
}

void quicksort (pLatLng a, int left, int right, double bearing)
{
        int	last	= left, i;

        if (left >= right) return;

        swapLatLng (a, left, randIndex (left, right));
        for (i = left + 1; i <= right; i++)
                if (a[i].lon < a[left].lon)
                        swapLatLng (a, ++last, i);
        swapLatLng (a, left, last);
        quicksort (a, left, last-1, bearing);
        quicksort (a, last+1, right, bearing);
}
*/

#if _DEBUG & 0
static void printSQLResult(PGresult *res) {
    int nFields, nTuples, i, j;

    nFields = PQnfields(res);
    nTuples = PQntuples(res);

    for (i = 0; i < nFields; i++)
        fprintf(stdout, "%-15s", PQfname(res, i));
    fprintf(stdout, "\n\n");

    for (i = 0; i < nTuples; i++) {
        for (j = 0; j < nFields; j++)
            fprintf(stdout, "%-15s", PQgetvalue(res, i, j));
        fprintf(stdout, "\n");
    }
    return;
}
#else
#define printSQLResult(x)
#endif // _DEBUG

static int findNearestEdge(PGconn *conn, char *pszDBTable, LatLng latlng,
                           int target) {
    char szSQL[288];
    PGresult *res;
    int ret = -1;

    snprintf(szSQL, 288, "SELECT gid, source, target, AsText(the_geom), "
                         "distance(the_geom, GeometryFromText("
                         "\'POINT(%f %f)\', 4326)) AS dist "
                         "FROM %s"
                         " WHERE the_geom && setsrid("
                         "\'BOX3D(%f %f,%f %f)\'::box3d, 4326) "
                         "ORDER BY dist LIMIT 1;",
             latlng.lon, latlng.lat, pszDBTable, (latlng.lon - 0.02),
             (latlng.lat - 0.02), (latlng.lon + 0.02), (latlng.lat + 0.02));

    res = PQexec(conn, szSQL);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("<!--SQL command failed: %s-->\n", PQerrorMessage(conn));
        goto done;
    }

    if (target) // target @ row 0 column 2
        ret = atoi(PQgetvalue(res, 0, 2));
    else // source @ row 0 column 1
        ret = atoi(PQgetvalue(res, 0, 1));

done:
    PQclear(res);

    return ret;
}

static int findShortestPath(PGconn *conn, char *pszTable, int start, int end,
                            pPathSegment *segments) {
    char szSQL[256];
    PGresult *res;
    int count = 0;
    int i = 0;
    int j = 0;

#if _DEBUG
    // TODO: cache the results in a new table?
    snprintf(szSQL, 256, "DROP TABLE IF EXISTS astar_result;\n"
                         "CREATE TABLE astar_result(gid int4) with oids;\n"
                         "SELECT AddGeometryColumn(\'astar_result\', "
                         "\'the_geom\', \'4326\', \'MULTILINESTRING\', 2);");
    res = PQexec(conn, szSQL);
#endif
/* TODO: setup cost based on user pref
 * Maybe 10 if safest and 2 for balanced and 1 for fastest
 * UPDATE sf_streets SET cost=10 WHERE bikelane=0;
 * UPDATE sf_streets SET cost=2 WHERE bikelane=0;
 * bikelane cost should always be 1 (or maybe less for offroad paths?)
 * UPDATE sf_streets SET cost=1 WHERE bikelane=1;
 * Then overall cost would be (length * cost)
 */

/* I modified astar_sp_directeds call to shortest_path_astar to multiply cost
 * with length */
/*snprintf (szSQL, 256,
        "SELECT * FROM "
        "shortest_path_astar("
                "\'SELECT gid as id, "
                "source::integer, "
                "target::integer, "
                "(length*cost)::double precision as cost, "
                "x1, y1, x2, y2 FROM %s\', "
                "%d, %d, false, false);",
        pszTable, start, end);*/

/*snprintf (szSQL, 256,
        "SELECT gid, NumPoints(the_geom) AS npoints, AsText(the_geom) AS
   the_geom FROM dijkstra_sp(\'%s\',%d,%d);",
        DBTABLE, start, end);*/

/* use astar to find the shortest path */
#if _DEBUG
    snprintf(szSQL, 256, "SELECT gid, NumPoints(the_geom) AS npoints, "
                         "AsText(the_geom) AS the_geom FROM "
                         "astar_sp(\'%s\',%d,%d);",
             pszTable, start, end);
#else
    snprintf(szSQL, 256, "SELECT gid, NumPoints(the_geom) AS npoints, "
                         "AsText(the_geom) AS the_geom FROM "
                         "astar_sp(\'%s\',%d,%d);",
             pszTable, start, end);
#endif

    res = PQexec(conn, szSQL);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("<!--SQL command failed: %s-->\n", PQerrorMessage(conn));
        goto done;
    }

    count = PQntuples(res);

    if (count)
        (*segments) = (pPathSegment)malloc(sizeof(PathSegment) * count);

    printSQLResult(res);
    for (i = 0; i < count; i++) {
        PGresult *resInfo = NULL;
        int gid = atoi(PQgetvalue(res, i, 0));
        int npoints = atoi(PQgetvalue(res, i, 1));
        char *geom = PQgetvalue(res, i, 2);

        /*snprintf (szSQL, 256, "SELECT streetname, facility_t, length, type
         * FROM ridesf WHERE gid = %d;", gid);*/
        snprintf(szSQL, 256, "SELECT %s, %s, %s, %s FROM %s WHERE gid = %d;",
                 NAMECOL, FACILITYCOL, LENCOL, BIKELNCOL, pszTable, gid);

        resInfo = PQexec(conn, szSQL);
        if (PQresultStatus(resInfo) != PGRES_TUPLES_OK) {
            printf("<!--SQL command failed: %s-->\n", PQerrorMessage(conn));
        } else {
            printSQLResult(resInfo);
            (*segments)[i].count = npoints;
            /* streetname (col 0) type (col 3)
            snprintf ((*segments)[i].streetname, 128, "%s %s", PQgetvalue
            (resInfo, 0, 0), PQgetvalue (resInfo, 0, 3));*/
            snprintf((*segments)[i].streetname, 128, "%s",
                     PQgetvalue(resInfo, 0, 0));
            (*segments)[i].bikelane = atoi(PQgetvalue(resInfo, 0, 3));
            if ((*segments)[i].streetname[0] == 0) {
                if ((*segments)[i].bikelane)
                    strncpy((*segments)[i].streetname, "Unnamed bike path", 17);
                else
                    strncpy((*segments)[i].streetname, "Unnamed street", 14);
            }
            strncpy((*segments)[i].facility_t, PQgetvalue(resInfo, 0, 1), 32);
            (*segments)[i].pLine =
                (pLatLng)malloc((sizeof(struct _LatLng) * npoints));
            memset((*segments)[i].pLine, 0, (sizeof(struct _LatLng) * npoints));
            (*segments)[i].gid = gid;
            (*segments)[i].distance = atof(PQgetvalue(resInfo, 0, 2));
            ;

            for (j = 0; j < npoints; j++) {
                PGresult *resPoints = NULL;
                char *pszTempSQL = NULL;
                int lenSQL = 0;

                lenSQL = ((strlen(geom) * 2) + 70 + 20);
                pszTempSQL = (char *)malloc((sizeof(char) * lenSQL));
                if (pszTempSQL) {
                    memset(pszTempSQL, 0, (sizeof(char) * lenSQL));

                    snprintf(pszTempSQL, lenSQL,
                             "SELECT X(PointN(GeomFromText(\'%s\'), %d)), "
                             "Y(PointN(GeomFromText(\'%s\'), %d));",
                             geom, j + 1, geom, j + 1);
                    resPoints = PQexec(conn, pszTempSQL);
                    free(pszTempSQL);
                    pszTempSQL = NULL;
                    lenSQL = 0;
                    printSQLResult(resPoints);
                }
                if (PQresultStatus(resPoints) == PGRES_TUPLES_OK) {
                    (*segments)[i].pLine[j].lat =
                        atof(PQgetvalue(resPoints, 0, 1));
                    (*segments)[i].pLine[j].lon =
                        atof(PQgetvalue(resPoints, 0, 0));

                    if (j > 0) {
                        (*segments)[i].distance +=
                            distHaversine((*segments)[i].pLine[j - 1].lat,
                                          (*segments)[i].pLine[j - 1].lon,
                                          (*segments)[i].pLine[j].lat,
                                          (*segments)[i].pLine[j].lon);
                    }
                } else {
                    printf("<!--SQL command failed: %s-->\n",
                           PQerrorMessage(conn));
                }
                PQclear(resPoints);
            }
        }
        PQclear(resInfo);
    }

done:
    PQclear(res);

    return count;
}

static char *getDirectionString(cardinalPoint_t start, cardinalPoint_t end) {
    int delta = ((int)start - (int)end);
    static char szDir[] = "Take a slight right"; // "Take a left" "Take a slight
                                                 // left" "Continue" "Take a
                                                 // right" "Take a slight right"

    memset(szDir, 0, 20);

    if (delta == 0) {
        sprintf(szDir, "Continue");
    } else if ((delta == -1) || (delta == 7)) {
        sprintf(szDir, "Take a slight right");
    } else if ((delta == -2) || (delta == 6)) {
        sprintf(szDir, "Take a right");
    } else if ((delta == -3) || (delta == 5)) {
        sprintf(szDir, "Take a sharp right");
    } else if ((delta == -4) || (delta == 4)) {
        sprintf(szDir, "Turn around");
    } else if ((delta == -5) || (delta == 3)) {
        sprintf(szDir, "Take a sharp left");
    } else if ((delta == -6) || (delta == 2)) {
        sprintf(szDir, "Take a left");
    } else if ((delta == -7) || (delta == 1)) {
        sprintf(szDir, "Take a slight left");
    }

    return szDir;
}

static char *getCardinalString(cardinalPoint_t direction) {
    static char dir[] = "Southwest";

    switch (direction) {
    case North:
        sprintf(dir, "North");
        break;
    case NorthEast:
        sprintf(dir, "Northeast");
        break;
    case East:
        sprintf(dir, "East");
        break;
    case SouthEast:
        sprintf(dir, "Southeast");
        break;
    case South:
        sprintf(dir, "South");
        break;
    case SouthWest:
        sprintf(dir, "Southwest");
        break;
    case West:
        sprintf(dir, "West");
        break;
    case NorthWest:
        sprintf(dir, "Northwest");
        break;
    default:
        break;
    }

    return dir;
}

static cardinalPoint_t getCardinalPoint(double bearing) {

    if (((bearing >= 0) && (bearing <= 22.5)) ||
        ((bearing > 337.5) && (bearing <= 360.1))) {
        return North;
    } else if (bearing > 22.5 && bearing <= 67.5) {
        return NorthEast;
    } else if ((bearing > 67.5) && (bearing <= 112.5)) {
        return East;
    } else if (bearing > 112.5 && bearing <= 157.5) {
        return SouthEast;
    } else if ((bearing > 157.5) && (bearing <= 202.5)) {
        return South;
    } else if (bearing > 202.5 && bearing <= 247.5) {
        return SouthWest;
    } else if ((bearing > 247.5) && (bearing <= 292.5)) {
        return West;
    } else if (bearing > 292.5 && bearing <= 337.5) {
        return NorthWest;
    }

    return -1;
}

static void printHTML(pPathSegment segments, int count, double total_dist) {
    int i = 0;
    double distance = 0.0;

    printf("<table valign=\\\"top\\\" id=\\\"directions_table\\\" "
           "class=\\\"dir_table\\\">"
           "<tr><td colspan=\\\"4\\\" class=\\\"tbl_title\\\">Directions to "
           "%s</td></tr>"
           "<tr><td colspan=\\\"4\\\" class=\\\"tbl_info\\\">%.1f miles - "
           "about <b>%.0f</b> to <b>%.0f</b> minutes</td></tr>"
           "<tr><td><img src=\\\"/images/dd-start.png\\\"></td><td "
           "class=\\\"dir_point\\\" id=\\\"dir_from\\\" "
           "colspan=\\\"3\\\"></td></tr>",
           segments[count - 1].streetname, total_dist, (total_dist / 15) * 60,
           (total_dist / 8) * 60);
    for (i = 0; i < count; i++) {
        double mi = KMTOMI(segments[i].distance);
        if (i == 0) {
            printf("<tr><td>%d.</td><td>", i + 1);
            if (segments[i].bikelane)
                printf("<img src=\\\"/images/bikelane.png\\\" "
                       "title=\\\"bikelane\\\">");
            printf("</td><td>");
            if (g_link) {
                printf("<a href=\\\"javascript:showPart (%.5f, %.5f, %.5f, "
                       "%.5f);\\\">",
                       fmin(segments[i].pLine[0].lat,
                            segments[i].pLine[segments[i].count - 1].lat),
                       fmin(segments[i].pLine[0].lon,
                            segments[i].pLine[segments[i].count - 1].lon),
                       fmax(segments[i].pLine[0].lat,
                            segments[i].pLine[segments[i].count - 1].lat),
                       fmax(segments[i].pLine[0].lon,
                            segments[i].pLine[segments[i].count - 1].lon));
            }

            if (segments[i].bikelane)
                printf("<i>");
            printf("Go to <b>%s</b> and head <b>%s</b>.",
                   segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
            if (segments[i].bikelane)
                printf("</i>");
            if (g_link)
                printf("</a>");
            printf("</td><td><b>%.1f</b> %s</td></tr>",
                   (mi >= 0.10) ? mi : MITOFT(mi), (mi >= 0.10) ? "mi" : "ft");
        } else {
            printf("<tr><td>%d.</td><td>", i + 1);
            if (segments[i].bikelane)
                printf("<img src=\\\"/images/bikelane.png\\\" "
                       "title=\\\"bikelane\\\">");
            printf("</td><td>");
            if (g_link) {
                printf("<a href=\\\"javascript:showPart (%.5f, %.5f, %.5f, "
                       "%.5f);\\\">",
                       fmin(segments[i].pLine[0].lat,
                            segments[i].pLine[segments[i].count - 1].lat),
                       fmin(segments[i].pLine[0].lon,
                            segments[i].pLine[segments[i].count - 1].lon),
                       fmax(segments[i].pLine[0].lat,
                            segments[i].pLine[segments[i].count - 1].lat),
                       fmax(segments[i].pLine[0].lon,
                            segments[i].pLine[segments[i].count - 1].lon));
            }
            if (segments[i].bikelane)
                printf("<i>");
            printf("%s onto <b>%s</b> and head <b>%s</b>.",
                   getDirectionString(
                       getCardinalPoint(segments[i - 1].bearing),
                       getCardinalPoint(getBearing(segments[i].pLine[0].lat,
                                                   segments[i].pLine[0].lon,
                                                   segments[i].pLine[1].lat,
                                                   segments[i].pLine[1].lon))),
                   segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
            if (segments[i].bikelane)
                printf("</i>");
            if (g_link)
                printf("</a>");
            printf("</td><td><b>%.1f</b> %s</td></tr>",
                   (mi >= 0.10) ? mi : MITOFT(mi), (mi >= 0.10) ? "mi" : "ft");
        }
        distance += segments[i].distance;
    }

    printf("<tr><td><img src=\\\"/images/dd-end.png\\\"></td><td "
           "class=\\\"dir_point\\\" id=\\\"dir_to\\\" "
           "colspan=\\\"3\\\"></td></tr></table>");
}

static void printKML(pPathSegment segments, int count) {
    int i = 0, j = 0;
    time_t tim;
    struct tm *now;

    if (g_dl) {
        printf("Content-type: application/vnd.google-earth.kml+xml\r\n"
               "Content-Disposition: attachment;filename=\"%s-%s.kml\"\r\n\r\n",
               segments[0].streetname, segments[count - 1].streetname);
    } else {
        printf("Content-type: application/vnd.google-earth.kml+xml\r\n\r\n");
    }

    tim = time(NULL);
    now = localtime(&tim);
    printf(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<kml xmlns=\"http://earth.google.com/kml/2.0\">\n<Document>\n"
        "<name>rideSF.com Bicycle Path</name>\n"
        "<description>Bicycle Path from %s to %s.</description>\n"
        "<TimeStamp>\n<when>%d-%02d-%02dT%02d:%02d:%02dZ</when>\n</TimeStamp>\n"
        "<Style id=\"bikeroute\">\n<LineStyle>\n"
        "<color>%s</color>\n"
        "<width>%d</width>\n"
        "</LineStyle>\n</Style>\n",
        segments[0].streetname, segments[count - 1].streetname,
        now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour,
        now->tm_min, now->tm_sec, "7fff0000", 2);

    printf("<Style id=\"street\">\n<LineStyle>\n"
           "<color>%s</color>\n<width>%d</width>\n"
           "</LineStyle>\n</Style>\n",
           "7f0000ff", 2);

    for (i = 0; i < count; i++) {
        printf("<Placemark>\n"
               "<name>%s</name>\n",
               segments[i].streetname);
        printf("<description>");
        if (i == 0) {
            printf("Go to %s and head %s.", segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
        } else {
            printf("%s onto %s and head %s.",
                   getDirectionString(
                       getCardinalPoint(segments[i - 1].bearing),
                       getCardinalPoint(getBearing(segments[i].pLine[0].lat,
                                                   segments[i].pLine[0].lon,
                                                   segments[i].pLine[1].lat,
                                                   segments[i].pLine[1].lon))),
                   segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
        }
        printf("</description>\n");

        printf("<styleUrl>%s</styleUrl>\n"
               "<LineString>\n<coordinates>\n",
               segments[i].bikelane ? "#bikeroute" : "#street");

        for (j = 0; j < segments[i].count; j++) {
            // don't print empty coords
            if (segments[i].pLine[j].lat && segments[i].pLine[j].lon) {
                printf("%f,%f,0\n", segments[i].pLine[j].lon,
                       segments[i].pLine[j].lat);
            } else {
                printf("<!-- Error: Lat Long %d:%d were 0 -->\n", i, j);
            }
        }

        printf("</coordinates>\n</LineString>\n</Placemark>\n");
    }
    printf("</Document>\n</kml>\n\n");
}

static void printGPX(pPathSegment segments, int count, LatLng start,
                     LatLng end) {
    int i = 0, j = 0;
    time_t tim;
    struct tm *now;

    if (g_dl) {
        printf("Content-type: text/xml\r\n"
               "Content-Disposition: attachment;filename=\"%s-%s.gpx\"\r\n\r\n",
               segments[0].streetname, segments[count - 1].streetname);
    } else {
        printf("Content-type: text/xml\r\n\r\n");
    }

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<gpx\n"
           " version=\"1.1\"\n"
           " creator=\"RideSF.com\"\n"
           " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
           " xmlns=\"http://www.topografix.com/GPX/1/1\"\n"
           " xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 "
           "http://www.topografix.com/GPX/1/1/gpx.xsd\">\n");
    tim = time(NULL);
    now = localtime(&tim);
    printf("<metadata>\n"
           "<name>Bicycle path from %s to %s</name>\n"
           "<desc>from %f,%f to %f,%f</desc>\n"
           "<author>\n<name>ridesf.com</name>\n</author>\n"
           "<copyright author=\"ridesf.com\">\n<year>%d</year>\n</copyright>\n"
           "<link "
           "href=\"http://ridesf.com/cgi-bin/"
           "route.cgi?scoord=%f,%f&amp;ecoord=%f,%f&amp;fmt=GPX\"></link>\n"
           "<time>%d-%02d-%02dT%02d:%02d:%02dZ</time>\n"
           "<keywords>Bicycle, Bike, San Francisco, Bike Lane, Safety, "
           "ridesf.com</keywords>\n"
           "<bounds minlat=\"%f\" minlon=\"%f\" maxlat=\"%f\" maxlon=\"%f\"/>\n"
           "</metadata>\n",
           segments[0].streetname, segments[count - 1].streetname, start.lon,
           start.lat, end.lon, end.lat, now->tm_year + 1900, start.lon,
           start.lat, end.lon, end.lat, now->tm_year + 1900, now->tm_mon + 1,
           now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec,
           fmin(start.lat, end.lat), fmin(start.lon, end.lon),
           fmax(start.lat, end.lat), fmax(start.lon, end.lon));

    for (i = 0; i < count; i++) {
        printf("<rte>\n"
               "<name>%s</name>\n",
               segments[i].streetname);

        printf("<desc>");
        if (i == 0) {
            printf("Go to %s and head %s.", segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
        } else {
            printf("%s onto %s and head %s.",
                   getDirectionString(
                       getCardinalPoint(segments[i - 1].bearing),
                       getCardinalPoint(getBearing(segments[i].pLine[0].lat,
                                                   segments[i].pLine[0].lon,
                                                   segments[i].pLine[1].lat,
                                                   segments[i].pLine[1].lon))),
                   segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
        }
        printf("</desc>\n");
        printf("<number>%d</number>\n", i);

        for (j = 0; j < segments[i].count; j++) {
            // don't print empty coords
            if (segments[i].pLine[j].lat && segments[i].pLine[j].lon) {
                printf("<rtept lat=\"%f\" lon=\"%f\"/>\n",
                       segments[i].pLine[j].lat, segments[i].pLine[j].lon);
            } else {
                printf("<!-- Error: Lat Long %d:%d were 0 -->\n", i, j);
            }
        }
        printf("</rte>\n");
    }
    printf("</gpx>\n\n");
}

void printJML(pPathSegment segments, int count) {
    int i = 0, j = 0;

    if (g_dl) {
        printf("Content-type: text/xml\r\n"
               "Content-Disposition: attachment;filename=\"%s-%s.jml\"\r\n\r\n",
               segments[0].streetname, segments[count - 1].streetname);
    } else {
        printf("Content-type: text/xml\r\n\r\n");
    }

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<JCSDataFile xmlns:gml=\"http://www.opengis.net/gml\" "
           "xmlns:xsi=\"http://www.w3.org/2000/10/XMLSchema-instance\" >\n"
           "<JCSGMLInputTemplate>\n"
           "<CollectionElement>featureCollection</CollectionElement>\n"
           "<FeatureElement>feature</FeatureElement>\n"
           "<GeometryElement>geometry</GeometryElement>\n"
           "<ColumnDefinitions>\n"
           "<column>\n"
           "<name>streetname</name>\n"
           "<type>STRING</type>\n"
           "<valueElement elementName=\"property\" attributeName=\"name\" "
           "attributeValue=\"streetname\"/>\n"
           "<valueLocation position=\"body\"/>\n"
           "</column>\n"
           "<column>\n"
           "<name>directions</name>\n"
           "<type>STRING</type>\n"
           "<valueElement elementName=\"property\" attributeName=\"name\" "
           "attributeValue=\"directions\"/>\n"
           "<valueLocation position=\"body\"/>\n"
           "</column>\n"
           "<column>\n"
           "<name>bikelane</name>\n"
           "<type>INTEGER</type>\n"
           "<valueElement elementName=\"property\" attributeName=\"name\" "
           "attributeValue=\"bikelane\"/>\n"
           "<valueLocation position=\"body\"/>\n"
           "</column>\n"
           "<column>\n"
           "<name>length</name>\n"
           "<type>STRING</type>\n"
           "<valueElement elementName=\"property\" attributeName=\"name\" "
           "attributeValue=\"length\"/>\n"
           "<valueLocation position=\"body\"/>\n"
           "</column>\n"
           "<column>\n"
           "<name>FID</name>\n"
           "<type>INTEGER</type>\n"
           "<valueElement elementName=\"property\" attributeName=\"name\" "
           "attributeValue=\"FID\"/>\n"
           "<valueLocation position=\"body\"/>\n"
           "</column>\n"
           "</ColumnDefinitions>\n"
           "</JCSGMLInputTemplate>\n\n"
           "<featureCollection>\n");

    for (i = 0; i < count; i++) {
        printf("<feature>\n"
               "<geometry>\n"
               "<gml:LineString>\n"
               "<gml:coordinates>\n");

        for (j = 0; j < segments[i].count; j++) {
            // don't print empty coords
            if (segments[i].pLine[j].lat && segments[i].pLine[j].lon) {
                printf("%f,%f\n", segments[i].pLine[j].lon,
                       segments[i].pLine[j].lat);
            } else {
                printf("<!-- Error: Lat Long %d:%d were 0 -->\n", i, j);
            }
        }
        printf("</gml:coordinates>\n"
               "</gml:LineString>\n"
               "</geometry>\n"
               "<property name=\"streetname\">%s</property>\n"
               "<property name=\"bikelane\">%d</property>\n"
               "<property name=\"length\">%f</property>\n"
               "<property name=\"FID\">%d</property>\n",
               segments[i].streetname, segments[i].bikelane,
               segments[i].distance, i + 1);
        printf("<property name=\"directions\">");
        if (i == 0) {
            printf("Go to %s and head %s.", segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
        } else {
            printf("%s onto %s and head %s.",
                   getDirectionString(
                       getCardinalPoint(segments[i - 1].bearing),
                       getCardinalPoint(getBearing(segments[i].pLine[0].lat,
                                                   segments[i].pLine[0].lon,
                                                   segments[i].pLine[1].lat,
                                                   segments[i].pLine[1].lon))),
                   segments[i].streetname,
                   getCardinalString(getCardinalPoint(getBearing(
                       segments[i].pLine[0].lat, segments[i].pLine[0].lon,
                       segments[i].pLine[1].lat, segments[i].pLine[1].lon))));
        }
        printf("</property>\n");
        printf("</feature>\n");
    }
    printf("\n</featureCollection>\n"
           "</JCSDataFile>\n\n");
}

void printWKT(pPathSegment segments, int count) {
    int i = 0, j = 0;
    int first = 0;

    if (g_dl) {
        printf("Content-type: text/plain\r\n"
               "Content-Disposition: attachment;filename=\"%s-%s.wkt\"\r\n\r\n",
               segments[0].streetname, segments[count - 1].streetname);
    } else {
        printf("Content-type: text/plain\r\n\r\n");
    }

    for (i = 0; i < count; i++) {
        printf("LINESTRING(");
        for (j = 0; j < segments[i].count; j++) {
            if (segments[i].pLine[j].lat && segments[i].pLine[j].lon) {
                if (!first) {
                    printf("%.6f %.6f", segments[i].pLine[j].lat,
                           segments[i].pLine[j].lon);
                    first = 1;
                } else {
                    printf(",%.6f %.6f", segments[i].pLine[j].lat,
                           segments[i].pLine[j].lon);
                }
            }
        }
        first = 0;
        printf(")\n");
    }
}

void printJSON(pPathSegment segments, int count, LatLng start, LatLng end) {
    int i = 0, j = 0;
    LatLng prev = {0, 0};
    double dist = 0;
    int first = 0;

    if (g_dl) {
        printf(
            "Content-type: text/x-json\r\n"
            "Content-Disposition: attachment;filename=\"%s-%s.json\"\r\n\r\n",
            segments[0].streetname, segments[count - 1].streetname);
    } else {
        printf("Content-type: text/x-json\r\n\r\n");
    }

    printf("{\"bounds\": [\"%f,%f\", \"%f,%f\"], \"path\": [",
           fmin(start.lat, end.lat), fmin(start.lon, end.lon),
           fmax(start.lat, end.lat), fmax(start.lon, end.lon));
    for (i = 0; i < count; i++) {
        for (j = 0; j < segments[i].count; j++) {
            if (segments[i].pLine[j].lat && segments[i].pLine[j].lon) {
                if (!LATLNGEQUAL(segments[i].pLine[j], prev)) {
                    prev = segments[i].pLine[j];
                    if (!first) {
                        printf("\"%f,%f\"", segments[i].pLine[j].lat,
                               segments[i].pLine[j].lon);
                        first = 1;
                    } else {
                        printf(", \"%f,%f\"", segments[i].pLine[j].lat,
                               segments[i].pLine[j].lon);
                    }
                }
            }
        }
        dist += segments[i].distance;
    }
    printf("], \"distance\": \"%f miles\"", KMTOMI(dist));
    printf(", \"directions\": \"");
    printHTML(segments, count, KMTOMI(dist));
    printf("\"}");
}

void printSHP(pPathSegment segments, int count) {
    printf("Content-type: application/gzip\r\n"
           "Content-Disposition: attachment;filename=\"%s-%s.tar.gz\"\r\n\r\n",
           segments[0].streetname, segments[count - 1].streetname);
}

void printKMZ(pPathSegment segments, int count) {
    printf("Content-type: application/vnd.google-earth.kmz\r\n"
           "Content-Disposition: attachment;filename=\"%s-%s.zip\"\r\n\r\n",
           segments[0].streetname, segments[count - 1].streetname);
}
#if _DEBUG
void printC(pPathSegment segments, int count, pLatLng pStart) {
    int i, j;

    printf("typedef struct _LatLng {\n"
           "        double  lat;\n"
           "        double  lon;\n"
           "} LatLng, *pLatLng;\n\n");

    printf("typedef struct _PathSegment {\n"
           "        int     index;\n"
           "        char    streetname[128];\n"
           "        int     count;\n"
           "        LatLng  pLine[128];\n"
           "} PathSegment, *pPathSegment;\n\n");

    printf("#define\tCOUNT\t%d\n"
           "LatLng startPt = {%f,%f};\n"
           "PathSegment segments[COUNT] = {\n",
           count, pStart->lat, pStart->lon);

    for (i = 0; i < count; i++) {
        printf("{%d, \"%s\", %d, {", i, segments[i].streetname,
               segments[i].count);
        for (j = 0; j < segments[i].count; j++) {
            // don't print empty coords
            if (segments[i].pLine[j].lat && segments[i].pLine[j].lon) {
                if (j != 0)
                    printf(", {%f,%f}", segments[i].pLine[j].lat,
                           segments[i].pLine[j].lon);
                else
                    printf("{%f,%f}", segments[i].pLine[j].lat,
                           segments[i].pLine[j].lon);
            } else {
                printf("/* Error: Lat Long %d:%d were 0 */\n", i, j);
            }
        }
        if (i != (count - 1))
            printf("}},\t/* %d */\n", i);
        else
            printf("}}};\t/* %d */\n\n\n", i);
    }
}
#endif // _DEBUG

static void reversePath(pLatLng path, int count) {
    int i = 0;

    for (i = 0; i < (count / 2); i++)
        swapLatLng(path, i, (count - i - 1));
}

static pPathSegment combineCommonStreets(pPathSegment segments, int *count) {
    int i = 0, j = 0;
    int lcount = 0;
    char *prevName = NULL;
    int newSize = 0;
    pPathSegment retSegments = NULL;

    for (i = 0; i < (*count); i++) {
        if ((prevName == NULL) ||
            strncmp(segments[i].streetname, prevName, 128)) {
            newSize++;
            prevName = segments[i].streetname;
            retSegments = (pPathSegment)realloc(
                retSegments, (sizeof(struct _PathSegment) * newSize));
            memset(&retSegments[newSize - 1], 0, sizeof(struct _PathSegment));
            strncpy(retSegments[newSize - 1].streetname, segments[i].streetname,
                    128);
            strncpy(retSegments[newSize - 1].facility_t, segments[i].facility_t,
                    32);
        }

        lcount = retSegments[newSize - 1].count;
        retSegments[newSize - 1].distance += segments[i].distance;
        retSegments[newSize - 1].count += segments[i].count;
        retSegments[newSize - 1].bearing = segments[i].bearing;
        retSegments[newSize - 1].bikelane = segments[i].bikelane;
        retSegments[newSize - 1].pLine = (pLatLng)realloc(
            retSegments[newSize - 1].pLine,
            (sizeof(struct _LatLng) * retSegments[newSize - 1].count));
        memset(&retSegments[newSize - 1].pLine[lcount], 0,
               (sizeof(struct _LatLng) * segments[i].count));

        for (j = 0; j < segments[i].count; j++)
            retSegments[newSize - 1].pLine[lcount + j] = segments[i].pLine[j];
    }

    (*count) = newSize;

    return retSegments;
}

static int findClosestEdge(pPathSegment segments, int count, pLatLng pStart) {
    int i = 0;
    int iClosest = -1;
    double distance = 0;
    double shortest = 1000.00;
    unsigned short bReverse = 0;

    for (i = 0; i < count; i++) {
        int iLast = segments[i].count - 1;

        distance =
            distHaversine(pStart->lat, pStart->lon, segments[i].pLine[0].lat,
                          segments[i].pLine[0].lon);
        if (distance < shortest) {
            shortest = distance;
            iClosest = i;
            bReverse = 0;
        }

        distance = distHaversine(pStart->lat, pStart->lon,
                                 segments[i].pLine[iLast].lat,
                                 segments[i].pLine[iLast].lon);
        if (distance < shortest) {
            shortest = distance;
            iClosest = i;
            bReverse = 1;
        }
    }

    if (bReverse && (iClosest >= 0)) {
        reversePath(segments[iClosest].pLine, segments[iClosest].count);
    }

    return iClosest;
}

static void swapEdge(pPathSegment segments, int a, int b) {
    PathSegment tmp;

    tmp = segments[a];
    segments[a] = segments[b];
    segments[b] = tmp;
}

static int sortPath(pPathSegment segments, int count, pLatLng pStart) {
    int i = 0;
    int iEdge = 0;
    LatLng s = {0, 0};

    if (pStart)
        s = (*pStart);
    else
        return 0;

    for (i = 0; i < count; i++) {
        iEdge = findClosestEdge(&segments[i], count - i, &s);
        if (((iEdge + i) != i) && (iEdge >= 0))
            swapEdge(segments, i, (iEdge + i));

        s = segments[i].pLine[(segments[i].count - 1)];
    }

    return i;
}

static void printError(fmt_t fmt, char *error) {
    switch (fmt) {
    case fmtCODE:
        break;
    case fmtKMZ:
        break;
    case fmtSHP:
        break;
    case fmtJSON:
        printf("Content-type: text/x-json\r\n\r\n");
        printf("{ \"error\": \"%s\" }\n", error);
        break;
    case fmtWKT:
        break;
    /* all xml */
    case fmtGPX:
    case fmtKML:
    case fmtJML:
    default:
        printf("Content-type: text/xml\r\n\r\n");
        printf("<!-- Error: %s -->\n", error);
        break;
    }
}

static void calculateBearings(pPathSegment segments, int count) {
    int i;

    for (i = 0; i < count; i++) {
        int npoints = segments[i].count;
        /* get the bearing of the last leg of this line segment */
        /* TODO: maybe this should be the overall bearing of the entire line
         * segment? */
        segments[i].bearing = getBearing(segments[i].pLine[npoints - 2].lat,
                                         segments[i].pLine[npoints - 2].lon,
                                         segments[i].pLine[npoints - 1].lat,
                                         segments[i].pLine[npoints - 1].lon);
    }
}

int main(int argc, char *argv[]) {
    char conninfo[128] = {0};
    PGconn *conn = NULL;
    LatLng startPt = {0, 0};
    LatLng endPt = {0, 0};
    int startEdge = -1;
    int endEdge = -1;
    char *pszSCoord = NULL;
    char *pszECoord = NULL;
    char *pszFMT = NULL;
    char szDB[32] = {0};
    char szDBTable[32] = {0};
    fmt_t fmt = fmtJSON; // default to JSON output
    char *pszTmp = NULL;
    int combine = 1; // default combine to true
    int sort = 1;    // default sort to true

    cgi_init();
    cgi_process_form();

    // getenv("REQUEST_METHOD");
    // "GET"
    // getenv ("QUERY_STRING");
    // ?scoord=-122.393898,37.776323&ecoord=-122.424369,37.745483&fmt=JSON
    pszSCoord = cgi_param("scoord");
    if (pszSCoord) {
        char *tmp = strchr(pszSCoord, ',');
        if (tmp)
            *tmp = 0;
        startPt.lon = atof(pszSCoord);
        startPt.lat = atof(++tmp);
    }
    pszECoord = cgi_param("ecoord");
    if (pszECoord) {
        char *tmp = strchr(pszECoord, ',');
        if (tmp)
            *tmp = 0;
        endPt.lon = atof(pszECoord);
        endPt.lat = atof(++tmp);
    }

    /* get the DB name and DB Table name or use defaults */
    /* db and tbl should be validated */
    pszTmp = cgi_param("db");
    if (pszTmp != NULL)
        PQescapeString(szDB, pszTmp, 31);
    else
        strncpy(szDB, DBNAME, 31);
    szDB[31] = '\0';

    pszTmp = cgi_param("tbl");
    if (pszTmp != NULL)
        PQescapeString(szDBTable, pszTmp, 31);
    else
        strncpy(szDBTable, DBTABLE, 31);
    szDBTable[31] = '\0';

    pszFMT = cgi_param("fmt");
    if (pszFMT) {
        if (!strncmp("GPX", pszFMT, 3))
            fmt = fmtGPX;
        else if (!strncmp("KML", pszFMT, 3))
            fmt = fmtKML;
        else if (!strncmp("KMZ", pszFMT, 3))
            fmt = fmtKMZ;
        else if (!strncmp("SHP", pszFMT, 3))
            fmt = fmtSHP;
        else if (!strncmp("JSON", pszFMT, 4))
            fmt = fmtJSON;
        else if (!strncmp("WKT", pszFMT, 3))
            fmt = fmtWKT;
        else if (!strncmp("JML", pszFMT, 3))
            fmt = fmtJML;
#if _DEBUG
        else if (!strncmp("CODE", pszFMT, 4))
            fmt = fmtCODE;
#endif // _DEBUG
        else
            fmt = fmtJSON;
    }
    pszTmp = cgi_param("combine");
    if (pszTmp)
        combine = atoi(pszTmp);

    pszTmp = cgi_param("sort");
    if (pszTmp)
        sort = atoi(pszTmp);

    pszTmp = cgi_param("dl");
    if (pszTmp)
        g_dl = atoi(pszTmp);

    pszTmp = cgi_param("link");
    if (pszTmp)
        g_link = atoi(pszTmp);

    /* we are done with cgi parameter processing */
    cgi_end();

    if (!startPt.lon || !startPt.lat || !endPt.lon || !endPt.lat) {
        // TODO: check for start & end
        // and geocode the address strings
        // output == (xml, kml, csv)
        // http://maps.google.com/maps/geo?q=1600+Amphitheatre+Parkway,+Mountain+View,+CA&output=xml&key=abcdefg
        printError(fmt, "Coordinates are zero!");
        goto done;
    }

    snprintf(conninfo, 128, "host=%s dbname=%s user=%s", DBHOST, szDB, DBUSER);
    conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        printError(fmt, PQerrorMessage(conn));
        goto done;
    }

    startEdge = findNearestEdge(conn, szDBTable, startPt, 1);
    endEdge = findNearestEdge(conn, szDBTable, endPt, 0);

    if ((startEdge >= 0) && (endEdge >= 0)) {
        pPathSegment segments = NULL, segs = NULL;
        int segCount = 0, count = 0;
        int i = 0;

        segCount =
            findShortestPath(conn, szDBTable, startEdge, endEdge, &segments);

        /* we are finished with the db connection */
        PQfinish(conn);
        conn = NULL;

        if (segCount && segments) {
            count = segCount;
            if (sort) {
                // orgainize path segments by implicit traversal order
                sortPath(segments, count, &startPt);
            }

            if (combine) {
                // join path segments by common streetnames
                segs = combineCommonStreets(segments, &count);
                for (i = 0; i < segCount; i++) {
                    free(segments[i].pLine);
                }
                free(segments);
            } else {
                segs = segments;
            }

            // re-compute bearing
            calculateBearings(segs, count);
        } else
            printError(fmt, "");

        if (segs && count) {
            switch (fmt) {
            case fmtKML:
                printKML(segs, count);
                break;
            case fmtKMZ:
                printKMZ(segs, count);
                break;
            case fmtGPX:
                printGPX(segs, count, startPt, endPt);
                break;
            case fmtJSON:
                printJSON(segs, count, startPt, endPt);
                break;
            case fmtSHP:
                printSHP(segs, count);
                break;
            case fmtJML:
                printJML(segs, count);
                break;
#if _DEBUG
            case fmtCODE:
                printC(segs, count, &startPt);
                break;
#endif // _DEBUG
            case fmtWKT:
            default:
                printWKT(segs, count);
                break;
            }
            /* cleanup the malloc'd mem */
            for (i = 0; i < count; i++) {
                free(segs[i].pLine);
            }
            free(segs);
        }
    }
done:
    if (conn)
        PQfinish(conn);
    return 0;
}
