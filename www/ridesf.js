// JavaScript Document
// <![CDATA[
var geocoder = null;
var rgeocoder = null;
var geostage = 0;
var map = null;
var counter = 0;
var rgstage = 0;
var clicked = 0;
var start;
var end;
var request = null;
var from;
var to;
var page_func;

function RGeoCoderLoad (placemark) {
	var addr = rgeocoder.getPlacemarkProperty (placemark, "ThoroughfareName");
	addr += ", ";
	addr += rgeocoder.getPlacemarkProperty (placemark, "LocalityName");
	addr += ", ";
	addr += rgeocoder.getPlacemarkProperty (placemark, "AdministrativeAreaName");
	if (rgstage == 0) {
		document.getElementById('fromAddress').value = addr;
		rgstage = 1;
		document.getElementById('dir_from').innerHTML = addr;
		updateStatus (" done", 0);
		updateStatus ("Reverse Geocoding end point...", 1);
		rgeocoder.reverseGeocode (end);
	} else if (rgstage == 1) {
		document.getElementById('toAddress').value = addr;
		document.getElementById('dir_to').innerHTML = addr;
		updateStatus (" done", 0);
		rgstage = 0;
	}
}

function displayPath () {
	if (request.readyState == 4) {
		doc = request.responseText;

		updateStatus (" done", 0);
		updateStatus ("<a href=\"/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=XXX\">route</a>", 1);

		if (doc) {
			points	= doc.split (",");
			len	= points.length;
			latlngs = [];

			for (var i = 2; i < len; i++)
			{
				point = points[i].split(" ", 2);
				latlng = new GLatLng (point[0], point[1]);
				delete point;
				latlngs.push (latlng);
				delete latlng;
			}
			delete points;
			delete doc;
			polyline = new GPolyline (latlngs, "#ff0000", 3, 0.5);
			delete latlngs;
			map.addOverlay (polyline);
			delete polyline;
		}
	}
}

function displayJSONPath () {
	if (request.readyState == 4) {
		var	doc	= request.responseText;
		var	obj	= eval ('(' + doc + ')');

		if (obj.error)
			updateStatus ("error", 0);
		else
			updateStatus (" done", 0);

		gpxURL = "http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=GPX&combine=1&sort=1&dl=1";
		kmlURL = "http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=KML&combine=1&sort=1&dl=1";
		jmlURL = "http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=JML&combine=1&sort=1&dl=1";

		updateStatus ("Download this path as <a href=\""+gpxURL+"\" target=\"_new\">GPX</a>, <a href=\""+kmlURL+"\" target=\"_new\">KML</a>, or <a href=\""+jmlURL+"\" target=\"_new\">JML</a>", 1);

		if (clicked == 1) {
			rgstage = 0;
			updateStatus ("Reverse Geocoding start point...", 1);
			rgeocoder.reverseGeocode (start);
			clicked = 0;
		}

		if (doc && obj) {
			latlngs	= [];
			len	= obj.path.length;

			for (var i = 0; i < len; i++) {
				point	= obj.path[i].split(',');
				latlng	= new GLatLng (parseFloat (point[0]), parseFloat (point[1]));
				delete point;
				latlngs.push (latlng);
				delete latlng;
			}

			polyline	= new GPolyline (latlngs, "#4012FE", 3, 0.7);
			delete latlngs;

			bounds	= polyline.getBounds ();
			zoom	= map.getBoundsZoomLevel (bounds);
			center	= bounds.getCenter (); 
			map.addOverlay (polyline);
			map.setCenter (center, zoom);
			delete polyline;

			updateDirections (obj.directions);
			if (clicked == 0) {
				document.getElementById('dir_to').innerHTML = to;
				document.getElementById('dir_from').innerHTML = from;
			}
			displayDirections (0);
			delete doc;
			delete obj;
		}
	}
}

function displayGPXPath () {
	if (request.readyState == 4) {
		var gpxDoc = request.responseXML;

		updateStatus (" done", 0);
		if (!gpxDoc) {
			updateStatus (" error", 0);
			updateStatus ("Unable to parse <a href=\"http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=GPX&combine=1&sort=1&dl=1\">GPX</a> Document", 1);
		} else if (!gpxDoc.documentElement) {
			updateStatus (" error", 0);
			updateStatus ("Document was not recognized by the XML loader", 1);
		} else if (gpxDoc.documentElement.childNodes.length < 3) {
			updateStatus (" error", 0);
			updateStatus ("The XML loader could not parse document", 1);
		} else {
			gpxURL = "http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=GPX&combine=1&sort=1&dl=1";
			kmlURL = "http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=KML&combine=1&sort=1&dl=1";
			jmlURL = "http://ridesf.com/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=JML&combine=1&sort=1&dl=1";

			updateStatus ("Download this path as <a href=\""+gpxURL+"\" target=\"_new\">GPX</a>, <a href=\""+kmlURL+"\" target=\"_new\">KML</a>, or <a href=\""+jmlURL+"\" target=\"_new\">JML</a>", 1);

			if (clicked == 1) {
				rgstage = 0;
				updateStatus ("Reverse Geocoding start point...", 1);
				rgeocoder.reverseGeocode (start);
				clicked = 0;
			}
			//
			//	Parse document <bounds>.  Open map.  Guess at zoom level.
			//
			minlat 		= 0;
			minlon 		= 0;
			maxlat 		= 0;
			maxlon 		= 0;
			bounds		= gpxDoc.documentElement.getElementsByTagName("bounds");
			if( bounds && bounds.length ) {
				minlat = parseFloat(bounds[0].getAttribute("minlat"));
				minlon = parseFloat(bounds[0].getAttribute("minlon"));
				maxlat = parseFloat(bounds[0].getAttribute("maxlat"));
				maxlon = parseFloat(bounds[0].getAttribute("maxlon"));
			}
			zoomlat		= minlat+(maxlat-minlat)/2;
			zoomlon		= minlon+(maxlon-minlon)/2;
			zoomlevel	= map.getBoundsZoomLevel(new GLatLngBounds (
									new GLatLng (maxlat, minlon), 
									new GLatLng (minlat, maxlon)));
			map.setCenter(new GLatLng (zoomlat,zoomlon),zoomlevel);
			bounds = [];

			//	Parse routes in <rte></rte>
			//
			var rte;
			rte = gpxDoc.documentElement.getElementsByTagName ("rte");
			if (rte) {
				// Process each route
				//
				var directions = "<table valign=\"top\" class=\"dir_table\"><tr><td><img src=\"/images/dd-start.png\"></td><td class=\"dir_point\" id=\"dir_from\">"+from+"</td></tr>";
				//var directions = " ";
				var prevName	= null;
				var count = 1;
				for (var i = 0; i < rte.length; i++) {
					tcolor="#4012FE";
					twidth=3;
					topacity=.7;

					// Extract <name></name> tag
					tname = "";
					element = rte[i].getElementsByTagName ("name");
					if (element.length) {
						if (element[0].firstChild) {
							tname = element[0].firstChild.nodeValue;
							if (prevName == null) {
								//directions = "<div class=\"tableYellow\">Go to "+tname+"</div>";
								directions += "<tr><td colspan=\"2\">"+count+". Go to "+tname+"</td></tr>";
								prevName = tname;
							} else if (tname != prevName) {
								//directions += "<div class=\"tableGrey\">Take "+prevName+" to "+tname+"</div>";
								count++;
								directions += "<tr><td colspan=\"2\">"+count+". Take "+prevName+" to "+tname+"</td></tr>";
								prevName = tname;
							}
						}
					}

					// Process each point of each route, string 
					// them into a single polyline
					//
					var points = [];
					var rtept = rte[i].getElementsByTagName ("rtept");
					for (var k = 0; k < rtept.length; k++) {
						var latlng	= new GLatLng (
                                              parseFloat (rtept[k].getAttribute ("lat")),
                                              parseFloat (rtept[k].getAttribute ("lon")));
						points.push (latlng);
						delete latlng;
					}
					var polyline	= new GPolyline (points, tcolor, twidth, topacity);
					map.addOverlay (polyline);
					delete polyline;
					delete points;
				}
				directions += "<tr><td><img src=\"/images/dd-end.png\"></td><td class=\"dir_point\" id=\"dir_to\">"+to+"</td></tr></table>";
				updateDirections (directions);
				displayDirections (0);
				rte = []
			}
		} // gpxDoc
	} // readyState
}

function SetStartOrStop (overlay, latlng) { 
	if (latlng) {
		clicked = 1;
		if (counter == 0) {
			start = latlng;
			map.clearOverlays ();
			var endIcon = new GIcon(G_DEFAULT_ICON);
			endIcon.image = "http://ridesf.com/images/dd-start.png";
			//endIcon.iconSize = new GSize (33,29);
			// Set up our GMarkerOptions object
			markerOptions = { icon:endIcon };
								
			var marker = new GMarker (start, markerOptions);
			map.addOverlay (marker);
			counter = 1;
			document.getElementById('fromAddress').value = start.y+","+start.x;
		} else { 
			end = latlng;
			var endIcon = new GIcon(G_DEFAULT_ICON);
			endIcon.image = "http://ridesf.com/images/dd-end.png";
			//endIcon.iconSize = new GSize (33,29);
			// Set up our GMarkerOptions object
			markerOptions = { icon:endIcon };
								
			var marker = new GMarker (end, markerOptions);
			map.addOverlay (marker);
			counter = 0;
			geostage = 2;
			document.getElementById('toAddress').value = end.y+","+end.x;
			document.getElementById ('status').innerHTML = " ";
			document.getElementById ('directions').innerHTML = " ";
			getDirections (latlng);
		}
	}
}

function RideSFApp (zoom, minlat, minlon, maxlat, maxlon) {
	counter = 0;
	geostage = 0;
	rgstage = 0;
	clicked = 0;
	map = new GMap2 (document.getElementById("map"));
	map.addMapType (G_PHYSICAL_MAP) ;
	map.addControl (new GSmallMapControl ());
	geocoder = new GClientGeocoder ();
	rgeocoder = new GReverseGeocoder (map);
	GEvent.addListener (rgeocoder, "load", RGeoCoderLoad);
	
	var zoomlat		= minlat+(maxlat-minlat)/2;
	var zoomlon		= minlon+(maxlon-minlon)/2;
	
	map.setCenter (new GLatLng (zoomlat, zoomlon), zoom);
	
	map.disableDoubleClickZoom();
	var ClickListener = GEvent.bind (map, "dblclick", this, SetStartOrStop);

	document.getElementById ('directions').style.display = "none";
}

function getDirections (point) {
	if (geocoder) {
		if (point == null) {
			if (geostage == 0) {
				document.getElementById ('status').innerHTML = " ";
				document.getElementById ('directions').innerHTML = " ";
				document.getElementById ('directions').style.display = "none";
				geostage = 1;
				from = document.getElementById('fromAddress').value;
				lfrom = from.toLowerCase ();
				if (lfrom.indexOf('san francisco, ca') == -1)
					from += " San Francisco, CA";

				updateStatus ("Geocoding start address \"" + from + "\"...", 1);
				geocoder.getLatLng (from, getDirections);
			} else {
				updateStatus ("Error geocoding addresses", 1);
				geostage = 0;
				document.getElementById ('status').innerHTML = " ";
				document.getElementById ('directions').innerHTML = " ";
			}
		} else {
			if (geostage == 1) {
				start = point;
				updateStatus (" done", 0);
				geostage = 2;

				to = document.getElementById('toAddress').value;
				lto = to.toLowerCase ();
				if (lto.indexOf('san francisco, ca') == -1)
					to += " San Francisco, CA";

				updateStatus ("Geocoding end address \"" + to + "\"...", 1);
				geocoder.getLatLng (to, getDirections); 

				var startIcon = new GIcon(G_DEFAULT_ICON);
				startIcon.image = "http://ridesf.com/images/dd-start.png";
				//startIcon.iconSize = new GSize (33,29);
				// Set up our GMarkerOptions object
				markerOptions = { icon:startIcon };
									
				var marker = new GMarker (start, markerOptions);
				map.addOverlay (marker);
			} else if (geostage == 2) {
				end = point;

				var endIcon = new GIcon(G_DEFAULT_ICON);
				endIcon.image = "http://ridesf.com/images/dd-end.png";
				//endIcon.iconSize = new GSize (33,29);
				// Set up our GMarkerOptions object
				markerOptions = { icon:endIcon };
									
				var marker = new GMarker (end, markerOptions);
				map.addOverlay (marker);

				if (!clicked)
					updateStatus (" done", 0);
				updateStatus ("Finding shortest path...", 1);
				geostage = 0;
				// add GPX overlay
				request = GXmlHttp.create();
				var URL = "/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=JSON&combine=1&sort=1";
						request.onreadystatechange = displayJSONPath;
						request.open("GET", URL, true);
						request.send (null);
			}
		}
	}
}
function updateStatus (txt, nl)
{
	if (nl)
		document.getElementById ('status').innerHTML += "<br>";

	document.getElementById ('status').innerHTML += txt;
}

function updateDirections (txt)
{
	document.getElementById ('directions').innerHTML += txt;
}

function displayDirections (showInput)
{
	if (showInput) {
		document.getElementById ('directions').style.display = "none";
		document.getElementById('findroute').style.display = "block";
	} else {
		document.getElementById ('directions').style.display = "block";
		document.getElementById('findroute').style.display = "none";
	}
}

function toggleOptions (showOptions) {
	if (showOptions) {
		document.getElementById ('d_options').innerHTML = "<a href=\"javascript:toggleOptions (0);\">&nbsp;[-]&nbsp;Options</a>";
		document.getElementById ('options').style.display = "block";
	} else {
		document.getElementById ('d_options').innerHTML = "<a href=\"javascript:toggleOptions (1);\">&nbsp;[+]&nbsp;Options</a>";
		document.getElementById ('options').style.display = "none";
	}
}

function changeFunction (func) {
	page_func	= func;
	switch (func) {
		case 0:	// directions
			document.body.id = "tab1";
			document.getElementById ('findroute').style.display = "block";
			document.getElementById ('thefts').style.display = "none";
			document.getElementById ('accidents').style.display = "none";
			document.getElementById ('ratings').style.display = "none";
			document.getElementById ('status').innerHTML = "Enter a start and end address or double click the map for start and end markers.<br>You do not need to enter City, State, or Zipcode.";
			break;
		case 1:	// thefts
			document.body.id = "tab2";
			document.getElementById ('findroute').style.display = "none";
			document.getElementById ('thefts').style.display = "block";
			document.getElementById ('accidents').style.display = "none";
			document.getElementById ('ratings').style.display = "none";
			document.getElementById ('status').innerHTML = "Enter an address or click the map to report a bicycle related theft.";
			break;
		case 2:	// accidents
			document.body.id = "tab3";
			document.getElementById ('findroute').style.display = "none";
			document.getElementById ('thefts').style.display = "none";
			document.getElementById ('accidents').style.display = "block";
			document.getElementById ('ratings').style.display = "none";
			document.getElementById ('status').innerHTML = "Enter an address or click the map to report a bicycle related accident.";
			break;
		case 3:	// ratings
			document.body.id = "tab4";
			document.getElementById ('findroute').style.display = "none";
			document.getElementById ('thefts').style.display = "none";
			document.getElementById ('accidents').style.display = "none";
			document.getElementById ('ratings').style.display = "block";
			document.getElementById ('status').innerHTML = "Click on the street segment you would like to rate.";
			break;
	}
}

function reportRoute () {
	// TODO: do something with start and end pts
}

function showPart (minLat, minLon, maxLat, maxLon) {
	bounds	= new GLatLngBounds (new GLatLng (maxLat, minLon), new GLatLng (minLat, maxLon));
	center	= bounds.getCenter ();
	zoom	= map.getBoundsZoomLevel (bounds);

	map.setCenter (center, zoom);
}

// ]]>

