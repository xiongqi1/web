function Cedric( iswidth ) {
var progress_bar_width = iswidth;
var get_status_speed;
var default_status_speed = 500;
var get_status_url;
var get_data_loop = false;
var seconds = 0;
var minutes = 0;
var hours = 0;
var initial_time = 0;
var info_width = 0;
var info_bytes = 0;
var last_bytes_read = 0;
var info_time_width = 500;
var info_time_bytes = 15;
var cedric_progress_bar = 0;
var cedric_hold = true;
var total_upload_size = 0;
var total_kbytes = 0;
var thisObject = this;
var reqObject;
var title;
var httpRetryInitial = 3;
var httpRetryCount = 3;
var prvProgressWidth = 0;
var prv_bytes_read = 0;

this.setProgressStyle = function(idName, styleParam, valueParam) {
	document.getElementById(idName).style[styleParam] = valueParam;
};

this.setProgressWidth = function(w) {
	if( w >= prvProgressWidth ) {
	    this.setProgressStyle('upload_status', 'width', w + 'px');
	    prvProgressWidth = w;
	}
	else if( (prvProgressWidth-w) > progress_bar_width*0.5 ) {
	    prvProgressWidth = 0;
	    prv_bytes_read = 0; 
	}
};
	
this.setProgressInfo = function(field, value) {
	document.getElementById(field).innerHTML = value;
};

// Hide the progress bar
this.showProgressDiv = function(info, bar) { 
	this.setProgressStyle('progress_info', 'display', info ? '' : 'none');
	this.setProgressStyle('progress_bar', 'display', bar ? '' : 'none');
};

// Reset the progress bar
this.zeroProgressBar = function() {
	get_status_url = "";
	get_data_loop = false;
	seconds = 0;
	minutes = 0;
	hours = 0;
	info_width = 0;
	info_bytes = 0;
	cedric_hold = true;
	total_upload_size = 0;
	prvProgressWidth = 0;
	title = "";

	httpRetryCount = httpRetryInitial;
	this.setProgressWidth(0);
	this.setProgressInfo('percent', '0%');
	this.setProgressInfo('current', 0);
	this.setProgressInfo('total_kbytes', 0);
	this.setProgressInfo('time', 0);
	this.setProgressInfo('speed', 0);
	this.setProgressInfo('remain', 0);
};

// Make the progress bar smooth
this.smoothCedricStatus = function() {
	if(info_width < progress_bar_width && !cedric_hold) {
		info_width++;
		this.setProgressWidth(info_width);
	}

	if(get_data_loop) { 
		setTimeout(function(){thisObject.smoothCedricStatus();}, info_time_width);
	}
};

// Make the bytes uploaded smooth
this.smoothCedricBytes = function() {
	if(info_bytes < total_kbytes && !cedric_hold) {
		info_bytes++;
		this.setProgressInfo('current', info_bytes);
	}
	if(get_data_loop) {
		setTimeout(function(){thisObject.smoothCedricBytes();}, info_time_bytes);
	}
};

// Calculate and display upload stats
this.setProgressStatus = function(bytes_read, lapsed_time) {
	var byte_speed = 0;
	var time_remaining = 0;

	if(lapsed_time > 0){ byte_speed = bytes_read / lapsed_time; }
	if(byte_speed > 0){ time_remaining = Math.round((total_upload_size - bytes_read) / byte_speed); }

	if(cedric_progress_bar == 1){
		if(byte_speed !== 0){
			info_time_width = Math.round(total_upload_size * 1000 / (byte_speed * progress_bar_width));
			info_time_bytes = Math.round(1024000 / byte_speed);
		}
		else{
			info_time_width = 200;
			info_time_bytes = 15;
		}
	}

	if (bytes_read > total_upload_size)
		bytes_read = total_upload_size;
	// Calculate percent finished
	var percent_float = bytes_read / total_upload_size;
	var percent = Math.round(percent_float * 100);
	var progress_bar_status = Math.round(percent_float * progress_bar_width);

	// Calculate time remaining
	var remaining_sec = (time_remaining % 60); 
	var remaining_min = (((time_remaining - remaining_sec) % 3600) / 60);
	var remaining_hours = ((((time_remaining - remaining_sec) - (remaining_min * 60)) % 86400) / 3600);

	if(remaining_sec < 10){ remaining_sec = '0' + remaining_sec; }
	if(remaining_min < 10){ remaining_min = '0' + remaining_min; }
	if(remaining_hours < 10){ remaining_hours = '0' + remaining_hours; }

	var time_remaining_f = remaining_hours + ':' + remaining_min + ':' + remaining_sec; 

	byte_speed = Math.round(byte_speed / 1024);
	bytes_read = Math.round(bytes_read / 1024);
	if(bytes_read < prv_bytes_read )
	    bytes_read = prv_bytes_read;
	else
	    pre_bytes_read = bytes_read;
	if(cedric_progress_bar == 1){
		cedric_hold = false;
		info_width = progress_bar_status;
		info_bytes = bytes_read;
	}
	else{
		this.setProgressWidth(progress_bar_status);
		this.setProgressInfo('current', bytes_read);
	}
	this.setProgressInfo('percent', isNaN(percent)? 0:percent + ' %');
	this.setProgressInfo('remain', time_remaining_f);
	this.setProgressInfo('speed', isNaN(byte_speed)? 0:byte_speed);
};

// Calculate the time spent uploading
this.getElapsedTime = function(){
	seconds++;

	if(seconds == 60){
		seconds = 0;
		minutes++;
	}

	if(minutes == 60){
		minutes = 0;
		hours++;
	}

	var hr = "" + ((hours < 10) ? "0" : "") + hours;
	var min = "" + ((minutes < 10) ? "0" : "") + minutes;
	var sec = "" + ((seconds < 10) ? "0" : "") + seconds;

	this.setProgressInfo('time', hr + ":" + min + ":" + sec);

	if(get_data_loop) {
		setTimeout(function() { thisObject.getElapsedTime();} , 1000);
	}
};

this.runRabbit = function(my_url) {
	var now = new Date();
	//get_status_url = my_url;
	get_status_speed = default_status_speed;
	cedric_progress_bar = 1;
	last_bytes_read = 0;
	get_data_loop = true;
	this.zeroProgressBar();
	this.startProgressStatus(my_url);
} ;


// Stop the upload
this.stopUpload = function() {
	get_data_loop = false;
	try {
		window.stop();
	}
	catch(e) {
		try {
			document.execCommand('Stop');
		} catch(e){} 
	}
};

// Create the AJAX request
this.createRequestObject = function (){
	if (reqObject)
		return reqObject;
	if(window.XMLHttpRequest){
		reqObject = new XMLHttpRequest();
		// if(reqObject.overrideMimeType){ reqObject.overrideMimeType('text/xml'); }
	}
	else if(window.ActiveXObject){
		try { reqObject = new ActiveXObject("Msxml2.XMLHTTP"); }
		catch(e) {
			try{ reqObject = new ActiveXObject("Microsoft.XMLHTTP"); }
			catch(e){}
		}
	}
	
	if(!reqObject) {
		this.setProgressInfo('phase',"Error: Your browser does not support AJAX");
		return false;
	}
	else { 
		return reqObject;
	}
};

this.placeRequest = function() {
	if(reqObject){
		reqObject.open("GET", get_status_url + "&rnd_id=" + Math.random(), true);
		reqObject.onreadystatechange = function(){ thisObject.reqResponse(); };
		reqObject.send(null);
	}
};

this.reqResponse = function() {
	if(reqObject.readyState == 4) {
		if(reqObject.status == 200) {
			
			if(reqObject.responseText.length>0) {
				var respObj = eval('(' + reqObject.responseText + ')');
				var now = new Date();
				var lapsed_time = now.getTime() - initial_time;
				var newTitle = "";
				httpRetryCount = httpRetryInitial;
				if (respObj.colour) {
					this.setProgressStyle('upload_status', 'backgroundColor', respObj.colour);
				}
				if (respObj.errors) {
					this.setProgressInfo('errors', respObj.errors.join("<br />"));
				}
				if (respObj.messages) {
					this.setProgressInfo('messages', respObj.messages.join("<br />"));
				}
				if (respObj.action) {
					switch(respObj.action) {
					case "info":
						break;
					case "start":
					case "continue":
						cedric_hold = false;
						this.showProgressDiv(1,1);
						break;
					case "stop":
						cedric_hold = true;
						this.stopUpload();
						break;
					default:
						break;
					}
				}
				if (!total_upload_size) {
					total_upload_size = respObj.total_bytes;
					if (!respObj.get_data_speed)
						get_status_speed = default_status_speed;
					else
						get_status_speed = respObj.get_data_speed;
					get_data_loop = true;
					newTitle = "Upload In Progress";
					total_kbytes = Math.round(total_upload_size / 1024);
					this.setProgressInfo('total_kbytes', "" + total_kbytes);
					this.getElapsedTime();
					if (cedric_progress_bar) {
						this.smoothCedricBytes();
						this.smoothCedricStatus();
					}
				}
				if (respObj.title) {
					newTitle = respObj.title;
				}
				if (newTitle && newTitle !== title) {
					title = newTitle;
					this.setProgressInfo('phase', title);
				}
				if (!total_upload_size) {
					this.setProgressInfo('phase', "Waiting for progress feedback");
				//	get_data_loop = false;
				}
				if( newTitle=="Finished" ) {
					last_bytes_read = total_upload_size;
					this.setProgressWidth(progress_bar_width);
					this.setProgressInfo('percent', '100%');
					this.setProgressInfo('current', total_kbytes );
					window.location = window.location;
					return;
				}
				last_bytes_read = parseInt(respObj.last_bytes_read);
				if (respObj.total_bytes > 0 && total_upload_size > 0) {
					if (last_bytes_read >= total_upload_size) {
						last_bytes_read = total_upload_size;
						this.setProgressWidth(progress_bar_width);
						this.setProgressInfo('current', total_kbytes );
						cedric_hold = true;
					}
					if(last_bytes_read > 0)
						this.setProgressStatus(last_bytes_read, lapsed_time / 1000);
				}
			} 
			else {
				reqObject = null;
				this.createRequestObject();
			}
		}
		else {
			/*
			if (!!httpRetryCount) {
				this.setProgressInfo('phase', "Error: " + reqObject.status + " " + reqObject.statusText);
				this.stopUpload();
			} else {
				this.setProgressInfo('phase', "Error: " + reqObject.status + " " + reqObject.statusText + " retry " + httpRetryCount);
				httpRetryCount--;
				reqObject = null;
				this.createRequestObject();
			}
			*/
			
			reqObject = null;
			this.createRequestObject();
		}
		if (get_data_loop && get_status_speed) {
			setTimeout(function() {thisObject.placeRequest();}, get_status_speed);
		}
	}
};

this.startNetProgressStatus = function(my_url) {
	var now = new Date();
	initial_time = now.getTime();
	httpRetryCount = httpRetryInitial;
	this.createRequestObject();
	this.showProgressDiv(1,1);
	total_upload_size = 0;
	get_status_url = my_url;    //"/cgi-bin/allpages.cgi?g=CdcsProgress";
	if(reqObject) {
		title = "Initializing Progress Bar";
		this.setProgressInfo('phase', title);
		this.placeRequest();
	}
};

this.startProgressStatus = this.startNetProgressStatus;
}
