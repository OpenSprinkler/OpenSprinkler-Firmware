const char ap_home_html[] PROGMEM = R"(<head><title>OpenSprinkler WiFi Config</title><meta name='viewport' content='width=device-width, initial-scale=1'></head>
<body>
<style> table, th, td { padding:4px; border-collapse: collapse;}
.ipt {font-size:12pt;height:28px;}
</style>
<caption><b>OpenSprinkler WiFi Config</caption><br><br>
<table border=1 id='rd'>
<tr><td align='center'><b>Detected SSID</b></td><td align='center'><b>BSSID</b></td><td align='center'><b>Signal</b></td>
<td><b>Ch.</b></td></tr><tr><td>(Scanning...)</td></tr>
</table>
<br>
<table>
<tr><td><b>SSID</b>:</td><td><input type='text' id='ssid' class='ipt' size=18></td></tr>
<tr><td><b>Password</b>:</td><td><input type='password' id='pass' class='ipt' size=18></td></tr>
<tr><td><b>BSSID</b>:</td><td><input type='text' id='bssid' class='ipt' size=18 placeholder='(optional)'></td></tr>
<tr><td><b>Channel</b>:<td><input type='text' id='channel' class='ipt' size=18 placeholder='(optional)'></td></tr>
<tr><td colspan=2><p id='msg'></p></td></tr>
<tr><td></td></td><td><button type='button' id='butt' onclick='sf();' style='height:48px;width:180px;font-size:16px'><b>Submit</b></button></td></tr>
</table>
<script>
function id(s) {return document.getElementById(s);}
function sel(i) {id('ssid').value=ssids[i][0];id('bssid').value=ssids[i][1];id('channel').value=ssids[i][3];}
var tci, ssids;
function tryConnect() {
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.ip==0) return;
var ip=''+(jd.ip%256)+'.'+((jd.ip/256>>0)%256)+'.'+(((jd.ip/256>>0)/256>>0)%256)+'.'+(((jd.ip/256>>0)/256>>0)/256>>0);
id('msg').innerHTML='<b><font color=green>Connected! Device IP: '+ip+'</font></b><br>Device is rebooting. Switch back to<br>the above WiFi network, and then<br>click the button below to redirect.'
id('butt').innerHTML='Go to '+ip;id('butt').disabled=false;
id('butt').onclick=function rd(){window.open('http://'+ip);}
clearInterval(tci);
}
}
xhr.open('GET', 'jtap', true); xhr.send();
}
function sf() {
id('msg').innerHTML='';
if(id('ssid').value.length==0) {alert("SSID is empty!"); return;}
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==1) { id('butt').innerHTML='Connecting...';
id('msg').innerHTML='<font color=gray>Connecting, please wait...</font>';
tci=setInterval(tryConnect, 1000); return; }
id('msg').innerHTML='<b><font color=red>Error code: '+jd.result+', item: '+jd.item+'</font></b>';
id('butt').innerHTML='Submit';
id('butt').disabled=false;id('ssid').disabled=false;id('pass').disabled=false;
id('bssid').disabled=false;id('channel').disabled=false;
}
};
var comm='ccap?ssid='+encodeURIComponent(id('ssid').value)+'&pass='+encodeURIComponent(id('pass').value);
if(id('bssid').value.length>0) {
if(id('channel').value.length>0) comm+='&extra='+encodeURIComponent(id('bssid').value+'@'+id('channel').value);
else comm+='&extra='+encodeURIComponent(id('bssid').value+'@0');
}
xhr.open('POST', comm, true); xhr.send();
id('butt').disabled=true;id('ssid').disabled=true;id('pass').disabled=true;
id('bssid').disabled=true;id('channel').disabled=true;
}
function truncateStr(s,n) {if(s.length<=n) return s; else return s.slice(0,n)+'...';}
function loadSSIDs() {
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText), i;
if(typeof jd.scanned !== 'object') return;
ssids = jd.scanned;
ssids.sort((a,b) => {return b[2]-a[2];}); // sort by signal strength
id('rd').deleteRow(1);
for(i=0;i<ssids.length;i++) {
var strength= ssids[i][2]>-71?'Ok':(ssids[i][2]>-81?'Weak':'Poor');
var row=id('rd').insertRow(-1);
row.innerHTML ="<tr><td><input type='radio' name='ssids' onclick=sel("+i+")>"
+truncateStr(ssids[i][0],15)+"</td><td>"+ssids[i][1]+"</td><td>"+ssids[i][2]+" dbm ("+strength+")</td><td align='center'>"+ssids[i][3]+"</td></tr>";
}
};
}
xhr.open('GET','jsap',true); xhr.send();
}
setTimeout(loadSSIDs, 1000);
</script>
</body>
)";
const char ap_update_html[] PROGMEM = R"(<head>
<title>OpenSprinkler Firmware Update</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
</head>
<body>
<div id='page_update'>
<div><h3>OpenSprinkler AP-mode Firmware Update</h3></div>
<div>
<form method='POST' action='/update' id='fm' enctype='multipart/form-data'>
<table cellspacing=4>
<tr><td><input type='file' name='file' accept='.bin' id='file'></td></tr>
<tr><td><b>Device password: </b><input type='password' name='pw' size=36 maxlength=36 id='pw'></td></tr>
<tr><td><label id='msg'></label></td></tr>
<tr><td style='width:460px'><b>IMPORTANT:</b> firmware update across major revisions (e.g. from 2.2.0 to 2.2.1) will trigger a factory reset. Save a copy of your current configurations before proceeding, so you can import and recover them after the upgrade. </td></tr>
</table>
<button id='btn_submit' style='height:48px;'>Submit</a>
</form>
</div>
</div>
<script>
function id(s) {return document.getElementById(s);}
function clear_msg() {id('msg').innerHTML='';}
function show_msg(s,t,c) {
id('msg').innerHTML=s.fontcolor(c);
if(t>0) setTimeout(clear_msg, t);
}
id('btn_submit').addEventListener('click', function(e){
e.preventDefault();
var files= id('file').files;
if(files.length==0) {show_msg('Please select a file.',2000,'red'); return;}
if(id('pw').value=='') {
if(!confirm('You did not input a device key. Are you sure?')) return;
}
show_msg('Uploading. Please wait...',10000,'green');
var fd = new FormData();
var file = files[0];
fd.append('file', file, file.name);
fd.append('pw', id('pw').value);
var xhr = new XMLHttpRequest();
xhr.onreadystatechange = function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==1) {
show_msg('Update is successful. Rebooting.',0,'green');
id('pw').value='';
} else if (jd.result==2) {
show_msg('Check device key and try again.', 10000, 'red');
} else {
show_msg('Update failed.',0,'red');
}
}
};
xhr.open('POST', '//' + window.location.hostname + ':8080' + window.location.pathname, true);
xhr.send(fd);
});
</script>
</body>
)";
const char sta_update_html[] PROGMEM = R"(<head>
<title>OpenSprinkler Firmware Update</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel='stylesheet' href='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'>
<script src='http://code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script>
<script src='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script>
</head>
<body>
<div data-role='page' id='page_update'>
<div data-role='header'><h3>OpenSprinkler Firmware Update</h3></div>
<div data-role='content'>
<form method='POST' action='/update' id='fm' enctype='multipart/form-data'>
<table cellspacing=4>
<tr><td><input type='file' name='file' accept='.bin' id='file'></td></tr>
<tr><td><b>Device password: </b><input type='password' name='pw' size=36 maxlength=36 id='pw'></td></tr>
<tr><td><label id='msg'></label></td></tr>
<tr><td style='width:400px'><b>IMPORTANT:</b> firmware update across major revisions (e.g. from 2.2.0 to 2.2.1) will trigger a factory reset. Save a copy of your current configurations before proceeding, so you can import and recover them after the upgrade. </td></tr>
</table>
<a href='#' data-role='button' data-inline='true' data-theme='b' id='btn_submit'>Submit</a>
</form>
</div>
<div data-role='footer' data-theme='c'>
<p style='font-weight:normal;'>&copy; OpenSprinkler (<a href='http://www.opensprinkler.com' target='_blank' style='text-decoration:none'>www.opensprinkler.com</a>)</p>
</div>
</div>
<script>
function id(s) {return document.getElementById(s);}
function clear_msg() {id('msg').innerHTML='';}
function show_msg(s,t,c) {
id('msg').innerHTML=s.fontcolor(c);
if(t>0) setTimeout(clear_msg, t);
}
$('#btn_submit').click(function(e){
var files= id('file').files;
if(files.length==0) {show_msg('Please select a file.',2000,'red'); return;}
if(id('pw').value=='') {
if(!confirm('You did not input a device password. Are you sure?')) return;
}else{beforeSubmit();}
show_msg('Uploading. Please wait...',0,'green');
var fd = new FormData();
var file = files[0];
fd.append('file', file, file.name);
fd.append('pw', id('pw').value);
var xhr = new XMLHttpRequest();
xhr.onreadystatechange = function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==1) {
show_msg('Update is successful. Rebooting.',10000,'green');
id('pw').value='';
} else if (jd.result==2) {
show_msg('Check device password and try again.', 10000, 'red');
} else {
show_msg('Update failed.',0,'red');
}
}
};
xhr.open('POST', '//'+window.location.hostname+':8080'+window.location.pathname, true);
xhr.send(fd);
});
</script>
<script src=https://ui.opensprinkler.com/js/hasher.js></script>
</body>
)";
