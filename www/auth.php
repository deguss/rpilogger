<?php

// function to parse the http auth header
function http_digest_parse($txt) {
    // protect against missing data
    $needed_parts = array('nonce'=>1, 'nc'=>1, 'cnonce'=>1, 'qop'=>1, 'username'=>1, 'uri'=>1, 'response'=>1);
    $data = array();
    $keys = implode('|', array_keys($needed_parts));

    preg_match_all('@(' . $keys . ')=(?:([\'"])([^\2]+?)\2|([^\s,]+))@', $txt, $matches, PREG_SET_ORDER);

    foreach ($matches as $m) {
        $data[$m[1]] = $m[3] ? $m[3] : $m[4];
        unset($needed_parts[$m[1]]);
    }

    return $needed_parts ? false : $data;
} 

// function to write to ini file
function write_ini_file($assoc_arr, $path, $has_sections=FALSE) { 
    $c = file_get_contents($path);
    $a = explode('[default]',$c);
    $content = $a[0]; 
    if ($has_sections) { 
        foreach ($assoc_arr as $key=>$elem) { 
            $content .= "[".$key."]\n"; 
            foreach ($elem as $key2=>$elem2) { 
                if(is_array($elem2)) 
                { 
                    for($i=0;$i<count($elem2);$i++) 
                    { 
                        $content .= $key2."[] = ".$elem2[$i]."\n"; 
                    } 
                } 
                else if($elem2=="") $content .= $key2." = \n"; 
                else $content .= $key2." = ".$elem2."\n"; 
            } 
        } 
    } 
    else { 
        foreach ($assoc_arr as $key=>$elem) { 
            if(is_array($elem)) 
            { 
                for($i=0;$i<count($elem);$i++) 
                { 
                    $content .= $key."[] = \"".$elem[$i]."\"\n"; 
                } 
            } 
            else if($elem=="") $content .= $key." = \n"; 
            else $content .= $key." = \"".$elem."\"\n"; 
        } 
    } 

    if (!$handle = fopen($path, 'w')) { 
        return false; 
    }

    $success = fwrite($handle, $content);
    fclose($handle); 

    return $success; 
}



$realm = 'Restricted area';

if (empty($_SERVER['PHP_AUTH_DIGEST'])) { //(cancel button)
    header('HTTP/1.1 401 Unauthorized');
    header('WWW-Authenticate: Digest realm="'.$realm.'",qop="auth",nonce="'.uniqid().'",opaque="'.md5($realm).'"');

    die('access denied'); 
}


//user => password
//$users = array('' => '');
include 'pwd.php';

// analyze the PHP_AUTH_DIGEST variable (empty form)
if (!($data = http_digest_parse($_SERVER['PHP_AUTH_DIGEST'])) || !isset($users[$data['username']]))
    die('access denied');


// generate the valid response
$A1 = md5($data['username'] . ':' . $realm . ':' . $users[$data['username']]);
$A2 = md5($_SERVER['REQUEST_METHOD'].':'.$data['uri']);
$valid_response = md5($A1.':'.$data['nonce'].':'.$data['nc'].':'.$data['cnonce'].':'.$data['qop'].':'.$A2);

if ($data['response'] != $valid_response) //(wrong user/pwd)
    die('access denied');

// ok, valid username & password
//echo 'You are logged in as: ' . $data['username'] . '<br>';

if (isset($_POST['submit'])){ 
    $path="/home/pi/rpilogger";
    $ans="";
    $connection = ssh2_connect('127.0.0.1', 22);

    if (!ssh2_auth_password($connection, $data['username'], $users[$data['username']]))
        die('<font color=red>SSH authentication failed...</font>');
    if ($_POST['submit']=="submit_init"){
        sleep(1);
        $stream = ssh2_exec($connection, '\sudo \shutdown -r +5 "http: reboot in 5min" &'); //no return value reading possible
        $ans.='<br />'.'System will reboot shortly';
    }
    elseif ($_POST['submit']=="submit_ads"){
        $stream = ssh2_exec($connection, 'sudo /etc/init.d/opendatalogger restart'); 
		stream_set_blocking($stream, true);
		$stream_out = ssh2_fetch_stream($stream, SSH2_STREAM_STDERR);
        $ans.='<br />'.$stream_out;
    }

    elseif ($_POST['submit']=="submit_ntp"){
        $stream = ssh2_exec($connection, 'sudo /etc/init.d/ntp restart'); 
        stream_set_blocking($stream, true);
        $stream_out = ssh2_fetch_stream($stream, SSH2_STREAM_STDERR); //SSH2_STREAM_STDIO); //SSH2_STREAM_STDERR

        $ans.='<br />'.stream_get_contents($stream);
    }

    elseif ($_POST['submit']=="submit_vsftpd"){
        $stream = ssh2_exec($connection, 'sudo /etc/init.d/vsftpd restart'); 
        stream_set_blocking($stream, true);
        $stream_out = ssh2_fetch_stream($stream, SSH2_STREAM_STDERR); //SSH2_STREAM_STDIO); //SSH2_STREAM_STDERR

        $ans.='<br />'.stream_get_contents($stream);
    }


}

$server_dir = $_SERVER['HTTP_HOST'] . rtrim(dirname($_SERVER['PHP_SELF']), '/\\') . '/';
header('Location: http://' . $server_dir . 'index.php?msg='.urlencode($ans));
die();
?>
