<?php 
$path = "/home/pi/rpilogger";

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
    <title>Realtime Viewer</title>
    <script language="javascript" type="text/javascript">
        function showIFrame() {  
            var btn = document.getElementById("iframe1btn");    
            var ph = document.getElementById("iframe1ph");    
            var ifrm = document.getElementById('iframe1');
            if (ifrm == null){
                ifrm = document.createElement('iframe');
                ifrm.setAttribute('id', 'iframe1'); // assign an id
                btn.parentNode.insertBefore(ifrm, ph); // to place before another page element
                btn.value="Hide";
                ifrm.setAttribute('src', 'http://geodata.ggki.hu/pid/plotm.html');          // assign url
                ifrm.setAttribute('frameborder', '1');
                ifrm.setAttribute('scrolling', 'no');
                ifrm.setAttribute('width', '900px');
                ifrm.setAttribute('height', '550px');
                console.log('iframe1 created');
            }
            else {
                btn.value="View";
                ifrm.remove();
                console.log('iframe1 removed');
            }
        }  
    </script>
    <!-- script type="text/javascript" src="jquery-2.1.3.js"></script--> 
    <script type="text/javascript">
      function add(text){
        var tBox = document.getElementById("tBox");
        tBox.value = tBox.value + text;
        tBox.scrollTop = 99999;
      }
    $(function() {
        var ws = new WebSocket("ws://192.168.1.125:50000");
        var tBox = document.getElementById("tBox");
        tBox.value="";
        ws.onmessage = function(evt) {
            add(evt.data);  
        }  
        ws.onopen = function(evt) {
            add("connected\r\n"); //$('#conn_status').html('<span style=\'background-color:Chartreuse;\'><b>Connected</b></span>');
        }
        ws.onerror = function(evt) {
            add("websocket error!\r\n");//$('#conn_status').html('<span style=\'background-color:Orange;\'><b>Websocket error</b></span>');
        }
        ws.onclose = function(evt) {
            add("websocket closed!\r\n");//$('#conn_status').html('<span style=\'background-color:OrangeRed;\'><b>Closed</b></span>');
        }
    });    
    </script>
</head>

<body>
<?php //========================================================================================================================
    if (isset($_GET['msg'])){
        echo '<p class="mono">'.urldecode($_GET['msg']).'</p>';
        unset($_GET['msg']);
        echo '<script type="text/javascript">window.history.pushState("object", "title", "'.$_SERVER['SCRIPT_NAME'].'");</script>';
    }
?> 

<h3>process monitor</h3>
<div><form method="post" action="auth.php">
    <table id="proc_table"> 
        <?php // alternative way: /etc/init.d/lighttp status
            test_proc("init","system");
            test_proc("ntp","network time service");
            //test_proc("postgres", "database");
            test_proc("vsftpd", "ftp service");
            test_proc("tailf-server","websocket server");
            test_proc("ads", "data logger");
        ?>
    <tr class="info">
        <td colspan="3"><?php
            $str = exec("uptime");
            $s1=explode(" user", $str);
            echo sprintf("system: %s",substr($s1[0],0,-4)) . '<br />'.PHP_EOL;
            echo sprintf("ads uptime %s",exec("ps -p $(pgrep ads) -o etime h")). '<br />'.PHP_EOL;
            echo sprintf("%s",substr($s1[1],3,-1)) . '<br />'.PHP_EOL; #load avg
            $bytes = disk_free_space(".");
            $si_prefix = array( 'B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB');
            $base = 1000;
            $class = min((int)log($bytes , $base) , count($si_prefix) - 1);
            echo sprintf('free disk: %1.2f' , $bytes / pow($base,$class)) . ' ' . $si_prefix[$class] . '<br />'.PHP_EOL;
            //echo sprintf('%s', exec('/opt/vc/bin/vcgencmd measure_temp')) . ' <br />';
            //echo sprintf('ads: %%CPU/%%MEM %s', exec('ps -p $(pgrep ads) -o %cpu,%mem')) . '<br />';
            echo "<a href=\"rpilogger/checker.txt\" target=\"_blank\">ads checker.log</a> &emsp;"; //."<br />".PHP_EOL;
            echo "<a href=\"rpilogger/log\" target=\"_blank\">all ads logs</a>"."<br />".PHP_EOL;
            echo "<a href=\"rpilogger/catnc.txt\" target=\"_blank\">catnc.log</a>"."<br />".PHP_EOL;
            //echo "<a href=\"rpilogger/tailf-server.txt\" target=\"_blank\">tailf-server.log</a>"."<br />".PHP_EOL;
        ?></td>
    </tr>
    </table>
    <textarea rows="15" cols="85" id="tBox" wrap="off" readonly></textarea>
<input type="reset" value="Clear"></input>
</form></div>
<hr />

<!-- script type="text/javascript" src="jquery-2.1.4.min.js"></script>
<script type="text/javascript"> 
    function loadpage(clicked_id){
        if (clicked_id =="datasetview"){
            console.log('loading dataset') 
            $("div#phdataset").load("test.php");
        }
    }
</script>
<div id="phdataset">click view dataset</div>
<button id="datasetview" type="button" onclick="loadpage(this.id)">view dataset</button-->
<input type="button" class="button" onclick="window.open('view_dataset.php'); return false;" value="view dataset" />


<?php /* hr />
<h3>realtime viewer </h3>
<input type="button" id="iframe1btn" onclick="showIFrame()" value="View"></input>
<br />
<p id="iframe1ph"></p>

<--iframe src="http://geodata.ggki.hu/pid/plotm.html" frameborder="1" scrolling="no" id="iframe1" width="900px" height="500px" style="visibility:hidden;">
  <p>Your browser does not support iframes.</p>
</iframe--> */ ?>



</body>
</html>

