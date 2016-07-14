====================
1. Required packages 
====================
Install these if you haven't already (listed in order of dependency):

PostgreSQL           - http://www.kyngchaos.com/software:postgres
                     - http://www.postgresql.org/
GEOS/PROJ frameworks - http://www.kyngchaos.com/software:frameworks
                     - http://trac.osgeo.org/geos/
                     - http://trac.osgeo.org/proj/
PostGIS              - http://www.kyngchaos.com/software:postgres
                     - http://postgis.refractions.net/
pgRouting            - http://www.kyngchaos.com/software:postgres
                     - http://pgrouting.postlbs.org/

#.bash changes for psql:
export PATH=/usr/local/pgsql/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/pgsql/lib:$LD_LIBRARY_PATH
export C_INCLUDE_PATH=/usr/local/pgsql/include:$C_INCLUDE_PATH
export MANPATH=/usr/local/pgsql/share:$MANPATH

====================
2. Setting up the DB
====================

See http://www.postgresql.org/docs/7.4/interactive/install-post.html for details on psql setup
See http://postgis.refractions.net/documentation/manual-1.4/ch02.html#PGInstall for PostGIS db creation afterwards

# Start psql:
sudo su -l postgres
/usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data
/usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data -l logfile start 

# Create db and configure for postgis:
createdb sf_streets # create our particular db
createlang plpgsql sf_streets # enable programmatic plsql so we can import postgis funcs
psql -d sf_streets -f /usr/local/pgsql/share/contrib/postgis.sql # import postgis funcs
psql -d sf_streets -f /usr/local/pgsql/share/contrib/spatial_ref_sys.sql # permits transforms or something

Note: my postgres user is messed, need to debug and fix- switch home dir etc
See: various google hits for "dscl postgres" etc

# Create plsql with defs for sf_streets and import
cd ~amarpai/ridesf/data
shp2pgsql -s 4326 -i -I sf_streets.shp sf_streets > ~amarpai/ridesf/data/sf_streets.sql
psql -d sf_streets -f ~amarpai/sf-bike-planner/branches/ridesf/data/sf_streets.sql

# patch the routing core wrappers and install the pgRouting functions
# NOTE: the path /usr/share/postlbs may not be correct for all installations
psql -U postgres -f /usr/share/postlbs/routing_core.sql sf_streets
cp ridesf/data/routing_core_wrappers.sql.diff /usr/share/postlbs
cd /usr/share/postlbs
patch -p0 -i routing_core_wrappers.sql.diff
psql -U postgres -f /usr/share/postlbs/routing_core_wrappers.sql sf_streets
psql -U postgres -f /usr/share/postlbs/routing_topology.sql sf_streets

# prepare the db for routing
psql -U postgres sf_streets	# start psql interactively
# from the psql interactive shell
ALTER TABLE sf_streets ADD COLUMN source integer;
ALTER TABLE sf_streets ADD COLUMN target integer;
SELECT assign_vertex_id('sf_streets', 0.00001, 'the_geom', 'gid');
CREATE INDEX source_idx ON ways(source);
CREATE INDEX target_idx ON ways(target);
CREATE INDEX geom_idx ON sf_streets USING GIST(the_geom GIST_GEOMETRY_OPS);

# add lat/lon columns
ALTER TABLE sf_streets ADD COLUMN x1 double precision;
ALTER TABLE sf_streets ADD COLUMN y1 double precision;
ALTER TABLE sf_streets ADD COLUMN x2 double precision;
ALTER TABLE sf_streets ADD COLUMN y2 double precision;

# calculate their values
UPDATE sf_streets SET x1 = x(startpoint(the_geom));
UPDATE sf_streets SET y1 = y(startpoint(the_geom));
UPDATE sf_streets SET x2 = x(endpoint(the_geom));
UPDATE sf_streets SET y2 = y(endpoint(the_geom));


# add cost column and fill it with values
ALTER TABLE sf_streets ADD COLUMN cost integer;
UPDATE sf_streets SET cost=10 WHERE bikelane=0;		# make non bikelanes costly
UPDATE sf_streets SET cost=1 WHERE bikelane=1;		# bikelanes are prefered

=======================
3. Building the app
=======================
# install libcgi (http://sourceforge.net/projects/libcgi/)
wget http://downloads.sourceforge.net/project/libcgi/libcgi/1.0/libcgi-1.0.tar.gz
tar xzvf libcgi-1.0.tar.gz
cd libcgi-1.0
./configure
make
sudo make install

# build the sql (see above "Setting up the DB")
cd ridesf/data
shp2pgsql -s 4326 -i -I sf_streets.shp sf_streets > sf_streets.sql

# build the route.cgi
cd ridesf/src/route
make
./test.sh

==============================
3. Test the DB routing locally
==============================
# edit ridesf/src/route/test.sh setting scoord and ecoord to your start and
# end points and fmt to KML or GPX to view the output in google earth
./test.sh > path.kml

=============================
3. How to inspect bike map DB
=============================
Install qGIS - http://www.kyngchaos.com/software:qgis
or Install uDig - http://udig.refractions.net/

=======
4. TODO
=======
- requirements for web app? apache etc? can it be run purely locally?
- additional build steps?

