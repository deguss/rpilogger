<?php
header( 'Content-type: text/html; charset=utf-8' );
echo "<link href=\"style.css\" type=\"text/css\" rel=\"stylesheet\">".PHP_EOL;
echo "<h3>dataset</h3>".PHP_EOL;
echo "These are the 1-hour binary files (stored in netcdf4 format).<br />".PHP_EOL;
echo "<table class=\"tb1\">";
  
    $connection = ssh2_connect('aero.nck.ggki.hu', 22);
    include 'pwd.php';
    if (!ssh2_auth_password($connection, 'pid', $users['pid']))
        die('<p class="mono">SSH authentication failed...</p>');
    $stream = ssh2_exec($connection, 'ls lemi-data -R');
    stream_set_blocking($stream, true);
    $stream_out = ssh2_fetch_stream($stream, SSH2_STREAM_STDIO); //SSH2_STREAM_STDIO); //SSH2_STREAM_STDERR
    $str_all = stream_get_contents($stream);
    if (!empty($str_all)){
        preg_match_all('/20[0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9].nc/', $str_all, $treffer); #2015-03-13T00.nc
        $cutyear = function( $element ){   return substr($element,0,4); };
        $cutmon = function( $element ){    return substr($element,0,7); };
        $cutday = function( $element ){    return substr($element,0,10); };

        $years = array_values(array_unique(array_map( $cutyear, $treffer[0]))); 
        $mons = array_values(array_unique(array_map( $cutmon, $treffer[0]))); 
        $days = array_values(array_unique(array_map( $cutday, $treffer[0]))); 
        foreach ($years as &$year) {
            echo    "<tr>".PHP_EOL.
                    "   <th colspan=3>".$year."</th>".PHP_EOL.
                    "</tr>".PHP_EOL;
            flush();                      
            foreach ($mons as &$mon) {
                ob_start();                
                $days_in_month = array_values(array_filter($days, function($item) use($mon) {
                    if(!strcmp(substr($item,0,7), $mon)) 
                        return TRUE;
                    return FALSE;
                }));
                $c_days = count($days_in_month);
                for($i=0;$i<$c_days;$i++){
                    echo    "<tr>".PHP_EOL;
                    if ($i==0)
                        echo"   <td rowspan=".$c_days.">".$mon[5].$mon[6]."</td>".PHP_EOL;
                    $dd=$days_in_month[$i];
                    echo "  <td><a href=\"http://aero.nck.ggki.hu/lemi-data/".substr($dd,0,4)."/".substr($dd,5,2)."/".substr($dd,8,2)."/catnc.txt\" target=\"_blank\">".$dd[8].$dd[9]."</a>".PHP_EOL;
                    //echo  "   <td>".$dd[8].$dd[9]."</td>".PHP_EOL;                
                    $hours_a_day = array_values(array_filter($treffer[0], function($item) use($dd) {
                        if(!strcmp(substr($item,0,10), $dd)) 
                            return TRUE;
                        return FALSE;
                    }));
                    echo    "   <td><code>";
                    $hourd=[];
                    foreach ($hours_a_day as &$hour)
                        array_push($hourd, "".$hour[11].$hour[12]);
                    asort($hourd);
                    for($j=0;$j<24;$j++){
                        if(in_array(sprintf("%02d",$j),$hourd))
                            echo "<a href=\"http://aero.nck.ggki.hu/lemi-data/".substr($dd,0,4)."/".substr($dd,5,2)."/".substr($dd,8,2)."/".substr($dd,0,10)."T".sprintf("%02d",$j).".nc"."\" target=\"_blank\">".sprintf("%02d",$j)."</a> ";
                        else
                            echo "&nbsp;&nbsp; ";
                    }
                    echo                "</code></td>".PHP_EOL.
                            "</tr>".PHP_EOL;
                }
                ob_end_flush();
                                                          
            }
        }

    }
    
    flush();   
echo "</table>";
?>
