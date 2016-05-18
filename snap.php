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

$id = $argv[1];

sleep(10);

$file = "/var/cos/cvm/cosconfig.xml";
$xml = simplexml_load_file($file);
$cosip = $xml->cos;
//echo "$cosip\n";
$data = "<cvm><type>snap</type><action>reset</action><vmid>$id</vmid></cvm>";
$url = "http://$cosip/cos/service/cloudvm/snap.php";
//echo "$url\n";

//echo "$data\n";
$recv = QueryData($url, $data);
//echo $recv;

?>
