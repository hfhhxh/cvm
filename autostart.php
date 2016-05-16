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
function getip(){
    $ss = exec('/sbin/ifconfig br0 | sed -n \'s/^ *.*addr:\\([0-9.]\\{7,\\}\\) .*$/\\1/p\'',$arr);
    $ret = $arr[0];
    return $ret;
}

$ip = getip();
$file = "/var/cos/cvm/cosconfig.xml";
$xml = simplexml_load_file($file);
$cosip = $xml->cos;
//echo "$cosip\n";
$data = "<cvm><type>autostart</type><ip>$ip</ip></cvm>";
//echo "$data\n";
$url = "http://$cosip/cos/service/cloudvm/autostart.php";
//echo "$url\n";
$data = QueryData($url, $data);
//echo "$data\n";
?>
