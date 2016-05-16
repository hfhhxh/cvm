#!/usr/bin/php
<?php
function QueryData($url, $data){
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
    curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");
    curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
    $response = curl_exec($ch);
    curl_close($ch);
    return $response;
}

function  GetData(){
    $data = file_get_contents('php://input');
    return $data;
}

$file = "/var/cos/cvm/cosconfig.xml";
$xml = simplexml_load_file($file);
$cosip = $xml->cos;
//echo "$cosip\n";
$data = "<cvm><state>online</state>";
$url = "http://$cosip/cos/service/cloudvm/online.php";
//echo "$url\n";

$pipes = array();
$pwd = "/var/cos/cvm/";
$command = "./cvm";
$desc = array(array('pipe', 'r'), array('pipe', 'w'), array('pipe', 'w'));
$handle = proc_open($command, $desc, $pipes, $pwd);
stream_set_blocking($pipes[0], 1);
stream_set_blocking($pipes[1], 1);
stream_set_blocking($pipes[2], 1);

fputs($pipes[0], "online\n");
while(true) {
	$id = rtrim(fgets($pipes[1]), "\n");
	if($id == "-1") break;
	$state = rtrim(fgets($pipes[1]), "\n");
	$data .= "<vm><id>$id</id><state>$state</state></vm>";
}	
$data .= "</cvm>";
//echo "$data\n";
QueryData($url, $data);
?>
