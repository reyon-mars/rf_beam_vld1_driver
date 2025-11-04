/*
 * vld1_config_form.hpp
 * Description: Page layout for serving Dynamic web content using formatted buffer print.
 *  Created on: Nov 03, 2025
 *      Author: Reyon Rai
 */

const char g_vld1_doc[] =
    "<!DOCTYPE html><html style=\"background-color:#F0F0F0\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" charset=\"utf-8\">"
    "<title>VLD1 Radar Configuration</title>";

const char g_style_start[] = "<style>";

const char g_style[] =
    "*{margin:0;padding:0;border:0;box-sizing:border-box;}html{height:100%;}body{position:relative;background:#f7f7f7;min-height:100%;color:#67727A;font-family:'Alegreya',sans-serif;margin:0;}input:-webkit-autofill{"
    "-webkit-text-fill-color:#67727A !important;-webkit-box-shadow: 0 0 0 30px white inset;}.name{font-size:30px;font-weight:bold;float:left;display:block;padding:20px;}"
    ".loginName{font-size:30px;font-weight:bold;text-align:center;display:block;padding:25px;}"
    "header{background-color:#f0f0f0;width:100%;height:86px;}#header-inner{margin:0 auto;}nav{float:right;padding: 20px 20px 0 0;}#menu-icon{position:absolute;"
    "top:30px;right:30px;display:none;float:right;font-size:30px;color:#800040;text-decoration:none;}input[type=password],select,textarea{width:100%;"
    "padding:12px;border: 1px solid #ccc;border-radius:4px;resize:vertical;color:#67727A;}#select{width:50%;}"
    "a:hover#menu-icon{border-radius: 4px 4px 0 0;}ul{list-style-type:none;}nav ul li{font-family:'Alegreya Sans',sans-serif;";

const char g_style2[] =
    "font-size:150%;display:inline-block;float:left;padding:5px;}nav ul li a{color:black;font-size:18px;padding:12px;"
    "border-radius:36px;text-decoration:none;}nav ul li a:hover{background-color:#ddd;color:black;}nav ul li a.active{"
    "background-color:#00b3b3;color:white;}input[type=text],select,textarea{width:100%;padding:10px;border:1px solid #ccc;border-radius:4px;resize:vertical;color:#67727A;}"
    "input[type=number],select,textarea{width:100%;padding:12px;border:1px solid #ccc;border-radius:4px;resize:vertical;color:#67727A;}"
    "label{padding: 12px 12px 12px 0;display:inline-block;font-weight:bold;}input[type=submit],input[type=button]{background-color:#4CAF50;color:white;padding: 12px 20px;"
    "border:none;border-radius:4px;cursor:pointer;display:block;margin: 20px auto;}input[type=submit]:hover,input[type=button]:hover{background-color:#45a049;"
    "}.footer{position:fixed;right:0;bottom:0;left:0;text-align:center;background-color:#f0f0f0;padding:10px 0 10px 0;}";

const char g_style3[] =
    ".container{border-radius:5px;background-color: #F7F7F7;padding: 40px 300px 40px 300px;}.col-25{float:left;width:25%;margin-top:6px;}"
    ".col-75{float:left;width:75%; margin-top:6px;}.row{padding: 15px 0 15px 0;}.row:after{content:\"\";display:table;clear:both;}"
    "@media screen and (max-width:1160px){header{position:inherit;height:100px;}.row{padding: 5px 0 5px 0;}.name{text-align:center;padding: 37px 80px 0 20px;font-size:25px;font-weight:bold;}"
    "nav ul li{padding:15px;}.container{padding:50px 20px 50px 20px;}.col-25,.col-75{width:100%;margin-top:0;}input[type=submit],input[type=button]{width:100%;margin-top:20px;}"
    "#menu-icon{display:block;}#select{width:70%;}nav ul,nav:active ul{display:none;z-index:1000;position:absolute;padding:20px;"
    "background:#F1F1F1;right:20px;top:60px;border:3px solid #FFF;border-radius:10px 0 2px 2px;width:70%;}nav:hover ul{display:block;}"
    "nav li{text-align:left;width:100%;padding:1px 0;}nav ul li a{font-size:60%;}}";

const char g_style_end[] = "</style></head>";

const char g_vld1_header[] =
    "<body><header><div id=\"header-inner\"><h2 class=\"name\">VLD1 Radar Configuration</h2></div></header>";

const char g_vld1_container[] =
    "<div class=\"container\"><form id=\"configForm\" method=\"post\">";

const char g_vld1_firmware[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Firmware Version</label></div><div class=\"col-75\"><input type=\"text\" value=\"%s\" name=\"firmware_version\" maxlength=\"19\" readonly></div></div>";

const char g_vld1_unique_id[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Unique ID</label></div><div class=\"col-75\"><input type=\"text\" value=\"%s\" name=\"unique_id\" maxlength=\"12\" readonly></div></div>";

const char g_vld1_distance_range[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Distance Range</label></div><div class=\"col-75\"><select name=\"distance_range\" %s>"
    "<option value=\"0\" %s>20m</option><option value=\"1\" %s>50m</option></select></div></div>";

const char g_vld1_threshold[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Threshold Offset (dB)</label></div><div class=\"col-75\"><input type=\"number\" value=\"%u\" name=\"threshold_offset\" min=\"20\" max=\"90\" required %s></div></div>";

const char g_vld1_min_range[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Min Range Filter (bin)</label></div><div class=\"col-75\"><input type=\"number\" value=\"%u\" name=\"min_range_filter\" min=\"1\" max=\"510\" required %s></div></div>";

const char g_vld1_max_range[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Max Range Filter (bin)</label></div><div class=\"col-75\"><input type=\"number\" value=\"%u\" name=\"max_range_filter\" min=\"2\" max=\"511\" required %s></div></div>";

const char g_vld1_avg_count[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Distance Avg Count</label></div><div class=\"col-75\"><input type=\"number\" value=\"%u\" name=\"distance_avg_count\" min=\"1\" max=\"255\" required %s></div></div>";

const char g_vld1_target_filter[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Target Filter</label></div><div class=\"col-75\"><select name=\"target_filter\" %s>"
    "<option value=\"0\" %s>Strongest</option><option value=\"1\" %s>Nearest</option><option value=\"2\" %s>Farthest</option></select></div></div>";

const char g_vld1_precision[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Distance Precision</label></div><div class=\"col-75\"><select name=\"distance_precision\" %s>"
    "<option value=\"0\" %s>Low</option><option value=\"1\" %s>High</option></select></div></div>";

const char g_vld1_tx_power[] =
    "<div class=\"row\"><div class=\"col-25\"><label>TX Power</label></div><div class=\"col-75\"><input type=\"number\" value=\"%u\" name=\"tx_power\" min=\"0\" max=\"31\" required %s></div></div>";

const char g_vld1_chirp_count[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Chirp Integration Count</label></div><div class=\"col-75\"><input type=\"number\" value=\"%u\" name=\"chirp_integration_count\" min=\"1\" max=\"100\" required %s></div></div>";

const char g_vld1_short_range[] =
    "<div class=\"row\"><div class=\"col-25\"><label>Short Range Filter</label></div><div class=\"col-75\"><select name=\"short_range_distance_filter\" %s>"
    "<option value=\"0\" %s>Disabled</option><option value=\"1\" %s>Enabled</option></select></div></div>";

const char g_vld1_script[] =
    "<script>function submitForm(){const form=document.getElementById('configForm');const data={};"
    "for(let i=0;i<form.elements.length;i++){const e=form.elements[i];if(e.name)data[e.name]=isNaN(e.value)?e.value:Number(e.value);}"
    "fetch('/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
    ".then(r=>r.json()).then(resp=>{if(resp.status==='success'){alert('Configuration updated successfully!');}else{alert('Update failed: '+resp.message);}}).catch(err=>alert('Error: '+err));}</script>";

const char g_vld1_form_end[] =
    "<div class=\"row\"><input type=\"button\" value=\"Submit\" onclick=\"submitForm()\"></div></form></div>";

const char g_footer[] =
    "<footer class=\"footer\">&#169 2025 Real Time Solutions Pvt. Ltd.</footer></body></html>";