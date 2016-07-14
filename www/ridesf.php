<?php include ('ridesf_conf.php'); ?>
<?php
$mtime=filemtime($_SERVER["SCRIPT_FILENAME"])-date("Z");
$gmt_mtime = date('D, d M Y H:i:s', $mtime) . ' GMT';
header("Last-Modified: ".$gmt_mtime);
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta name="verify-v1" content="vzYzFemR4be1Yk87VRmWmwg1o5Q08scjOb6lX3k286s=" />
<script src="http://maps.google.com/?file=api&amp;v=3.x&amp;key=<?php echo $google_maps_key; ?>" type="text/javascript"></script>
<script src="/GReverseGeoCoder.js" type="text/javascript"></script>
<script src="ridesf.js" type="text/javascript"></script>
<title>rideSF.com - San Francisco Bicycle Directions</title>
<link href="ridesf.css" rel="stylesheet" type="text/css" />
<link rel="icon" href="/favicon.ico" type="image/x-icon"/>
<script type="text/javascript">
//<![CDATA[
		   function initialize () {
			   var rideSFApp = new RideSFApp (
				<?php if (!empty($_GET['zoom']) && is_numeric($_GET['zoom'])) echo $_GET['zoom']; else echo "12"; ?>,
				<?php echo $min_lat; ?>,
				<?php echo $min_lon; ?>,
				<?php echo $max_lat; ?>,
				<?php echo $max_lon; ?>);
		   }
// ]]>
</script>
</head>
<body class="twoColElsRtHdr" onload="javascript:initialize();" onunload="javascript:GUnload();" id="tab1">
<div id="container">
  <div id="header">
  <?php if (!empty($_GET['opt']) && ($_GET['opt'] == 'yes')) {
    echo "<div id=\"user_opts\"><a href=\"javascript:\">Sign in</a>/<a href=\"javascript:\">Register</a></div>\n";
  } ?>
    <h1>rideSF.com</h1>
    <div id="status">Enter a start and end address or double click the map for start and end markers.<br/>You do not need to enter City, State, or Zipcode.</div>
    <ul id="tabnav">
      <li class="tab1"><a href="javascript:changeFunction (0);">Directions</a></li>
      <?php if (!empty($_GET['opt']) && ($_GET['opt'] == 'yes')) {
      echo "<li class=\"tab2\"><a href=\"javascript:changeFunction (1);\">Thefts</a></li>\n";
      echo "<li class=\"tab3\"><a href=\"javascript:changeFunction (2);\">Accidents</a></li>\n";
      echo "<li class=\"tab4\"><a href=\"javascript:changeFunction (3);\">Rate</a></li>\n";
	  } ?>
    </ul>
  </div>
  <div id="sidebar1">
    <!--    <div id="slider">
    </div>-->
    <div id="findroute">
      <form action="" onsubmit="javascript:{map.clearOverlays(); getDirections(null); return false;}">
        <table border="0" align="center">
          <tr>
            <th align="right">From:</th>
            <td align="left"><input type="text" id="fromAddress" class="addrInput" value=""/></td>
          </tr>
          <tr>
            <th align="right">To:</th>
            <td align="left"><input type="text" id="toAddress" class="addrInput" value="" /></td>
          </tr>
          <tr>
            <td align="center" colspan="2"><table border="0">
                <tr>
                  <td><input type="submit" value="Find route"/></td>
                  <td><div id="d_options">
                      <?php if (!empty($_GET['opt']) && ($_GET['opt'] == 'yes')) { echo "<a href=\"javascript:toggleOptions (1);\">&nbsp;[+]&nbsp;Options</a>\n"; } ?>
                    </div></td>
                </tr>
              </table></td>
          </tr>
        </table>
        <div id="options" style="display: none;">
          <table border="0" align="center">
            <tr>
              <th align="right">Maximum grade:</th>
              <td align="left"><input type="text" class="gradeInput" id="grade" value="25" />
                %</td>
            </tr>
            <tr>
              <td align="center" colspan="2"><select id="r_type">
                  <option value="1">Safest route</option>
                  <option value="2">Balanced route</option>
                  <option value="3">Fastest route</option>
                </select></td>
            </tr>
            <tr>
              <th align="right">Include user ratings:</th>
              <td><input type="checkbox" id="ratings_c" name="rate" value="rate" /></td>
            </tr>
          </table>
        </div>
      </form>
    </div>
    <div id="busy" style="display: none;" align="center">
      <img src="images/wait_lg.gif" alt="Please wait"/>
    </div>
    <div id="directions" style="display: none;"></div>
    <div id="thefts" style="display: none;">
      <form action="" onsubmit="javascript:">
        <table border="0">
          <tr>
            <th>Address:</th>
            <td><input type="text" style="width: 14em;" id="t_addr" /></td>
          </tr>
          <tr>
            <th>Type:</th>
            <td align="left"><select id="t_type">
                <option value="1">Entire bicycle</option>
                <option value="2">Both wheels</option>
                <option value="3">Front wheel</option>
                <option value="4">Rear wheel</option>
                <option value="5">Seat-post/Saddle</option>
                <option value="6">Handle bar/Stem</option>
              </select></td>
          </tr>
          <tr>
            <th>Lock:</th>
            <td align="left"><select id="lock">
                <option value="1">None</option>
                <option value="2">U-Lock</option>
                <option value="3">Chain</option>
                <option value="4">Cable</option>
                <option value="5">Locking skewer</option>
              </select></td>
          </tr>
          <tr>
            <td></td>
            <td><input type="submit" value="Submit" name="Submit" /></td>
          </tr>
        </table>
      </form>
    </div>
    <div id="accidents" style="display: none;">
      <form action="" onsubmit="javascript:">
        <table border="0">
          <tr>
            <th>Address:</th>
            <td align="left"><input type="text" style="width: 14em;" id="a_addr" /></td>
          </tr>
          <tr>
            <th>Type:</th>
            <td align="left"><select id="a_type">
                <option value="1">Automobile</option>
                <option value="2">Motorcycle/Scooter</option>
                <option value="3">Other Cyclist</option>
                <option value="4">Pedestrian</option>
              </select></td>
          </tr>
          <tr>
            <td></td>
            <td><input type="submit" value="Submit" name="Submit" /></td>
          </tr>
        </table>
      </form>
    </div>
    <div id="ratings" style="display: none;"> </div>
  </div>
  <div id="map"><div id="loading" align="center"><img src="images/wait_dg.gif" alt="Please wait"/></div></div>
  <br class="clearfloat" />
  <div id="footer">
    <table border="0" width="100%">
      <tr>
        <td align="left"><div id="copyright">&#169;2008 rideSF.com </div></td>
        <td align="center"><a href="http://validator.w3.org/check?uri=http://ridesf.com">Valid XHTML 1.0</a></td>
        <td align="right"><div id="feedback"><a href="mailto:feedback@ridesf.com?subject=RideSF.com Feedback">Feedback</a>/<a href="about.html">About</a></div></td>
      </tr>
    </table>
  </div>
</div>
<script type="text/javascript">
//<![CDATA[
google_ad_client = "pub-1915071773486729";
/* 728x90, created 11/17/08 */
google_ad_slot = "3172018915";
google_ad_width = 728;
google_ad_height = 90;
//]]>
</script>
<script type="text/javascript" src="http://pagead2.googlesyndication.com/pagead/show_ads.js"></script>
<!--<iFrame src="http://sfbc.exygy.com" width="150px" height="181px" frameborder="0" scrolling="no" marginheight="0" marginwidth="0">The browser doesn't support IFrames.</iFrame>-->
<script type="text/javascript">
//<![CDATA[
var gaJsHost = (("https:" == document.location.protocol) ? "https://ssl." : "http://www.");
document.write(unescape("%3Cscript src='" + gaJsHost + "google-analytics.com/ga.js' type='text/javascript'%3E%3C/script%3E"));
//]]>
</script>
<script type="text/javascript">
//<![CDATA[
var pageTracker = _gat._getTracker("UA-6244651-1");
pageTracker._trackPageview();
//]]>
</script>
</body>
</html>
