<?php
$host=exec("hostname");
$shost=str_replace('rpi-','',$host);

function test_proc($name,$desc=''){
    $pids=0;
    exec('pgrep '.$name, $pids);
    echo '<tr';
    echo $pids[0] ? '' : ' class="stopped"';
    echo '>'.
            '<td>'.$name.'</td>'.
            '<td><a href="#" class="tooltips"><img src="info_sign.gif" alt="?"><span>'.$desc.' ';
    if ($pids[0]){
        echo 'running: ';
        foreach ($pids as $pid)
            echo $pid.' ';
    } else
    echo '<b>not running!!!</b>';
    echo '</span></a></td>';
    echo '<td><button type="submit" name="submit" value="submit_'.$name.'">(re)start</button></td>';
    echo '</tr>'.PHP_EOL;

}
?><!DOCTYPE html>
<html>
<head>
    <meta content="text/html;charset=utf-8" http-equiv="Content-Type">
    <link href="style.css" type="text/css" rel="stylesheet">
    <meta http-equiv="Pragma" content="no-cache">
    <meta http-equiv="Cache-Control" content="no-cache">
    <title>Diagnostics for <?php print $host; ?></title>
    <script type="text/javascript" src="jquery-2.1.4.min.js"></script>
    <script type="text/javascript">
      function add(text){
        var tBox = document.getElementById("tBox");
        tBox.value = tBox.value + text;
        tBox.scrollTop = 99999;
      }
    $(function() {
        var ws = new WebSocket("ws://<?php echo $_SERVER["HTTP_HOST"]; ?>:50000");
        var tBox = document.getElementById("tBox");
        tBox.value="";
        ws.onmessage = function(evt) {
            add(evt.data);  
        }  
        ws.onopen = function(evt) {
            add("connected\r\n"); 
            tBox.style.backgroundColor = "Chartreuse";
        }
        ws.onerror = function(evt) {
            add("websocket error!\r\n");
            tBox.style.backgroundColor = "#FFCCCC";
        }
        ws.onclose = function(evt) {
            add("websocket closed!\r\n");
            tBox.style.backgroundColor = "#FFCCCC";            
        }
    });    
    </script>
</head>

<body>
<?php 
    if (isset($_GET['msg'])){
        echo '<p class="mono">'.urldecode($_GET['msg']).'</p>';
        unset($_GET['msg']);
        echo '<script type="text/javascript">window.history.pushState("object", "title", "'.$_SERVER['SCRIPT_NAME'].'");</script>';
    }
?> 

<h3><?php print $host; ?></h3>
<div><form method="post" action="auth.php">
    <table id="proc_table"> 
        <?php // alternative way: /etc/init.d/lighttp status
            test_proc("init","system");
            test_proc("ntp","network time service");
            //test_proc("postgres", "database");
            test_proc("vsftpd", "ftp service");
            test_proc("ads", "data logger");
        ?>
    <tr class="info">
        <td colspan="3"><?php
            echo exec("date").'<br />'.PHP_EOL;
            
            $uptime = shell_exec("cut -d. -f1 /proc/uptime");
            $days = floor($uptime/60/60/24);
            $hours = $uptime/60/60%24;
            $mins = $uptime/60%60;
            echo sprintf("system uptime: %d days %02d:%02d",$days,$hours,$mins).'<br />'.PHP_EOL;
            
            $uptime=intval(shell_exec("ps -p $(pgrep ^ads$) -o etimes h"));
            $days = floor($uptime/60/60/24);
            $hours = $uptime/60/60%24;
            $mins = $uptime/60%60;
            echo sprintf("&emsp;&emsp;&emsp;ads uptime: %d days %02d:%02d",$days,$hours,$mins).'<br />'.PHP_EOL;
            echo sprintf('&emsp;&emsp;&emsp;ads (%%CPU %%MEM) %s', exec('ps -p $(pgrep ads) -o %cpu,%mem')) . '<br />'.PHP_EOL;
            
            $bytes = disk_free_space(".");
            $si_prefix = array( 'B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB');
            $base = 1000;
            $class = min((int)log($bytes , $base) , count($si_prefix) - 1);
            echo sprintf('free disk: %1.2f' , $bytes / pow($base,$class)) . ' ' . $si_prefix[$class] . '<br />'.PHP_EOL;
            //echo sprintf('%s', exec('/opt/vc/bin/vcgencmd measure_temp')) . ' <br />'.PHP_EOL;
            echo sprintf('internal temp. %d°C', intval(exec('cat /sys/class/thermal/thermal_zone0/temp'))/1000 ) . ' <br />'.PHP_EOL;
            
            //echo "<a href=\"rpilogger/checker.txt\" target=\"_blank\">ads checker.log</a> &emsp;"; //."<br />".PHP_EOL;
            //echo "<a href=\"rpilogger/log\" target=\"_blank\">all ads logs</a>"."<br />".PHP_EOL;
            //echo "<a href=\"rpilogger/catnc.txt\" target=\"_blank\">catnc.log</a>"."<br />".PHP_EOL;
            //echo "<a href=\"rpilogger/tailf-server.txt\" target=\"_blank\">tailf-server.log</a>"."<br />".PHP_EOL;
        ?></td>
    </tr>
    </table>
    <textarea rows="15" cols="85" id="tBox" wrap="off" readonly></textarea>
<input type="reset" value="Clear"></input>
</form></div>

<ul>
    <li><?php print "<a href=\"http://tesla.ggki.hu/data/".$shost."/last/\">plots</a>".PHP_EOL; ?></li>
    <li><?php print "<a href=\"http://aero.nck.ggki.hu/".$shost."-data/\">data</a>".PHP_EOL; ?></li>
</ul>

</body>
</html>

