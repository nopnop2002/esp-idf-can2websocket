//document.getElementById("datetime").innerHTML = "WebSocket is not connected";

var websocket = new WebSocket('ws://'+location.hostname+'/');
var maxRow = 10;

function changeRow() {
	console.log('changeRow');
	var text = document.getElementById("maxrow");
	console.log('text.value=', text.value);
	console.log('typeof(text.value)=', typeof(text.value));
	maxRow = parseInt(text.value, 10);
	console.log('maxRow=', maxRow);
	
	let tableElem = document.getElementById('targetTable');
	var row = targetTable.rows.length;
	console.log("row=%d maxRow=%d", row, maxRow);
	if (row > maxRow) {
		for (var i=maxRow;i<row;i++) {
			tableElem.deleteRow(1);
		}
	}
}

function clipboardWrite() {
	console.log('clipboardWrite');
	var text = "hoge";
	if(navigator.clipboard){
		console.log("clipboard.writeText");
		navigator.clipboard.writeText(str);
	} else {
		alert("This browser is not supported");
	}
}

function sendText(name) {
	console.log('sendText');
/*
	var array = ["11", "22", "33"];
	var data = {};
	//data["foo"] = "abc";
	data["foo"] = array;
	var array = ["aa"];
	data["bar"] = array;
	data["hoge"] = 100;
	json_data = JSON.stringify(data);
	console.log(json_data);
*/

	var data = {};
	data["id"] = name;
	console.log('data=', data);
	json_data = JSON.stringify(data);
	console.log('json_data=' + json_data);
	websocket.send(json_data);
}

websocket.onopen = function(evt) {
	console.log('WebSocket connection opened');
	var data = {};
	data["id"] = "init";
	console.log('data=', data);
	json_data = JSON.stringify(data);
	console.log('json_data=' + json_data);
	websocket.send(json_data);
	//document.getElementById("datetime").innerHTML = "WebSocket is connected!";
}

websocket.onmessage = function(evt) {
	var msg = evt.data;
	console.log("msg=" + msg);
	var values = msg.split('\4'); // \4 is EOT
	console.log("values=" + values);
	switch(values[0]) {
		case 'BUTTON':
			console.log("BUTTON values[1]=" + values[1]);
			console.log("BUTTON values[2]=" + values[2]);
			console.log("BUTTON values[3]=" + values[3]);

			let buttonElem = document.getElementById(values[1]);
			if (values[2] == "class") {
				console.log("class=", values[3]);
				buttonElem.className = values[3];
			} else if (values[2] == "visibility") {
				console.log("visible=", values[3]);
				buttonElem.style.visibility = values[3];
			} else if (values[2] == "display") {
				console.log("block=", values[3]);
				buttonElem.style.display = values[3];
			}
			break;

		case 'RELOAD':
			console.log("RELOAD values[1]=" + values[1]);
			console.log("RELOAD values[2]=" + values[2]);
			console.log("RELOAD values[3]=" + values[3]);
			window.location.reload();

		case 'DATA':
			console.log("ADD values.length=" + values.length);
			for(var i=1;i<values.length;i++) {
				console.log("ADD values[%d]=[%s]", i, values[i]);
			}
			//console.log("ADD values[2]=" + values[2]);
			//console.log("ADD values[3]=" + values[3]);

			let tableElem = document.getElementById('targetTable');
			var row = targetTable.rows.length;
			console.log("row=%d maxRow=%d", row, maxRow);
			if (row > maxRow) {
				for (var i=maxRow;i<row;i++) {
					tableElem.deleteRow(1);
				}
			}

			let newRow = tableElem.insertRow();
			for(var i=1;i<values.length;i++) {
				let newCell = newRow.insertCell();
				let newText = document.createTextNode(values[i]);
				newCell.appendChild(newText);
			}

/*
			let newCell = newRow.insertCell();
			let newText = document.createTextNode(values[1]);
			newCell.appendChild(newText);

			newCell = newRow.insertCell();
			newText = document.createTextNode(values[2]);
			newCell.appendChild(newText);
*/

		default:
			break;
	}
}

websocket.onclose = function(evt) {
	console.log('Websocket connection closed');
	//document.getElementById("datetime").innerHTML = "WebSocket closed";
}

websocket.onerror = function(evt) {
	console.log('Websocket error: ' + evt);
	//document.getElementById("datetime").innerHTML = "WebSocket error????!!!1!!";
}
