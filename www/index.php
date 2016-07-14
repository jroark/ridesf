<?php

$user_agent = $_SERVER['HTTP_USER_AGENT'];

if (strpos ($user_agent, 'iPhone') || strpos ($user_agent, 'iPod')) {
	include "iphone.php";
	return;
} else if (strpos ($user_agent, 'BlackBerry') !== false) {
	include "blkberry.php";
	return;
} else if (strpos ($user_agent, 'Opera Mini') !== false) {
	include "operamini.php";
	return;
} else {
	//include "page.php";
	include "ridesf.php";
	return;
}

?>

