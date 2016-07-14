// JavaScript Document
// <![CDATA[
var geocoder = null;
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

function displayJSONPath () {
	if (request.readyState == 4) {
		var	doc	= request.responseText;
		var	obj	= eval ('(' + doc + ')');

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
			delete doc;
			delete obj;
		}
	}
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
	
	var zoomlat		= minlat+(maxlat-minlat)/2;
	var zoomlon		= minlon+(maxlon-minlon)/2;
	
	map.setCenter (new GLatLng (zoomlat, zoomlon), zoom);
	
	map.disableDoubleClickZoom();
	var ClickListener = GEvent.bind (map, "dblclick", this, SetStartOrStop);
	//document.getElementById ('directions').style.display = "none";
}

function getDirections (point) {
	if (geocoder) {
		if (point == null) {
			if (geostage == 0) {
				document.getElementById ('status').innerHTML = " ";
				document.getElementById ('directions').innerHTML = " ";
				//document.getElementById ('directions').style.display = "none";
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
				var URL = "/cgi-bin/route.cgi?scoord="+start.x+","+start.y+"&ecoord="+end.x+","+end.y+"&fmt=JSON&combine=1&sort=1&link=0";
						request.onreadystatechange = displayJSONPath;
						request.open("GET", URL, true);
						request.send (null);
				changeTBBtn (1);
				//document.getElementById ('directions').style.display = "block";
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

function reportRoute () {
	// TODO: do something with start and end pts
}

function showPart (minLat, minLon, maxLat, maxLon) {
	bounds	= new GLatLngBounds (new GLatLng (maxLat, minLon), new GLatLng (minLat, maxLon));
	center	= bounds.getCenter ();
	zoom	= map.getBoundsZoomLevel (bounds);

	map.setCenter (center, zoom);
}

function changeTBBtn (tbstate) {
	if (tbstate == 1) {
		document.getElementById ('tb_btn').innerHTML = "Directions";
		document.getElementById ('tb_btn').href = "#directions";
	} else if (tbstate == 2) {
		document.getElementById ('tb_btn').innerHTML = "Map";
		document.getElementById ('tb_btn').href = "#map";
	}
}

function toggleTBBtn (hide) {
		if (hide) {
			document.getElementById ('tb_btn').style.display = "none";
		} else {
			document.getElementById ('tb_btn').style.display = "block";
		}
}

// ]]>

