<?php include ('ridesf_conf.php'); ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>RideSF.com - Bike Mapper for iPhone</title>
<meta name="viewport" content="width=device-width; initial-scale=1.0; maximum-scale=1.0; user-scalable=0;" />
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
<meta name="apple-mobile-web-app-capable" content="yes" />
<style type="text/css" media="screen">@import "iui/iui.css"; @import "iridesf.css";</style>
<script type="application/x-javascript" src="iui/iui.js"></script>
<script src="http://maps.google.com/?file=api&amp;v=3.x&amp;key=<?php echo $google_maps_key;?>" type="text/javascript"></script>
<script src="/GReverseGeoCoder.js" type="text/javascript"></script>
<script src="/iridesf.js" type="text/javascript"></script>
<link rel="apple-touch-icon" href="iRideSF_icon.png"/>
<script type="text/javascript">
//<![CDATA[
function initialize () {
	var rideSFApp = new RideSFApp (
				<?php if (!empty($_GET['zoom']) && is_numeric($_GET['zoom'])) echo $_GET['zoom']; else echo "13"; ?>,
                                <?php echo $min_lat; ?>,
                                <?php echo $min_lon; ?>,
                                <?php echo $max_lat; ?>,
                                <?php echo $max_lon; ?>);
}
//]]>
</script>
</head>
<body onload="initialize()" onunload="GUnload()">
<div class="toolbar" style="padding-bottom: 0" id="toolbar">
	<h1 id="pageTitle"></h1>
	<a id="backButton" class="button" href="#" onclick="toggleTBBtn(0);"></a> 
	<a class="button" href="#about" id="tb_btn" onclick="toggleTBBtn(1);">About</a>
</div>
<form style="padding-top: 0;" selected="true" class="panel" title="iRideSF">
<div style="padding:6px 0;" class="msg" id="instructions">
Enter a start and end address.<br>You don't need to include city, state, or zipcode.
</div>
	<fieldset class="search-form">
		<div class="row">
			<label>From:</label>  <input type="text" size="25" id="fromAddress" name="from" value="28th St. & Dolores St." />
		</div>

		<div class="row">
			<label>To:</label>  <input type="text" size="25" id="toAddress" name="to" value="4th St. & King" />
		</div>
	</fieldset>
	<a href="#map" class="whiteButton" onclick="getDirections(null);">Find Route</a>
<script type="text/javascript">
//<![CDATA[
var admob_vars = {
 pubid: 'a14901f48fec26b', // publisher id
 bgcolor: '000000', // background color (hex)
 text: 'FFFFFF', // font-color (hex)
 test: false // test mode, set to false if non-test mode
};
//]]>
</script>
<p style="margin:110px 0 0 -12px;"><script type="text/javascript" src="http://mm.admob.com/static/iphone/iadmob.js"></script></p>
</form>
<div id="ads">
</div>

<div id="about" title="About" class="panel">
<h2>iRideSF: Mobile Bike Mapper</h2>
Developed by <a href="mailto:jroark@ridesf.com">John Roark</a> for iPhone and iPod Touch. 
</div>

<div id="map" style="width: 320px; height: 360px" title="Map" class="panel">
</div>

<div id="directions" title="Directions" class="panel">
</div>

<div id="status" title="Status" class="panel"></div>

</body>
</html>

