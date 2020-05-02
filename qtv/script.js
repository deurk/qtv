/*
 * example script
 * parse extended hostname information from html and display on load?
 */

//document.addEventListener('DOMContentLoaded', qtv_display_hostname_info, false);

function qtv_display_hostname_info(){
	var items = document.getElementsByClassName('hostname');
	var itemsList = Array.prototype.slice.call(items);

	itemsList.forEach(function(item){
		if (item.innerHTML.includes('live:')){
			var split=item.innerHTML.split("live: ");
			split=split[1].split(" @");
			item.innerHTML = split[0];
		} else {
			var split=item.innerHTML.split(" @ ");
			item.innerHTML = split[0];
		}
		item.style.opacity = "1";
		item.style.width = "auto";
		item.style.display = "block";
	});
}
