
function addMessage(m)
{
	console.log(m);
	$("#console").append(m+"</br>");
}

function sendCommand(c)
{
	$.ajax({
		url:"post",
		type:"POST",
		data:{cmd: c},
		dataType:"html",
		success:function(data) {
			addMessage('Cmd: ' + data);
		}
	});
}

function startEvents() 
{
	var es = new EventSource('/events');
	es.onopen = function(e) {
		addMessage("Events Opened");
	};
	es.onerror = function(e) {
		if (e.target.readyState != EventSource.OPEN) {
			addMessage("Events Closed");
		}
	};
	es.onmessage = function(e) {
		addMessage("Event: " + e.data);
	};
	es.addEventListener('cmd', function(e) {
		addMessage("Event[cmd]: " + e.data);
		if (e.data == "OK") {
			//sendCommand();
		}
	}, false);
}

$(document).ready(function()
{
	startEvents();
	$("#but_left").click(function(){ sendCommand("MR,1000,-1");});
	$("#but_right").click(function(){ sendCommand("MR,1000,1");});
	$("#but_left5").click(function(){ sendCommand("MR,3000,-5");});
	$("#but_right5").click(function(){ sendCommand("MR,3000,5");});
	$("#but_left10").click(function(){ sendCommand("MR,6000,-10");});
	$("#but_right20").click(function(){ sendCommand("MR,12000,20");});
	$("#but_left20").click(function(){ sendCommand("MR,12000,-20");});
	$("#but_right10").click(function(){ sendCommand("MR,6000,10");});
	$("#but_stop").click(function(){ sendCommand("STP");});
	$("#but_home").click(function(){ sendCommand("MH");});
	$("#but_send").click(function(){ sendCommand($("#cmd").val());});
});