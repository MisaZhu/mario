var i = 0;
while(true) {
	if(i == 10) {
		break;
	}
	else {
		print("while loop: " + i);
	}
	i++;
}

for(i=0;i<10;i++) {
	debug("for loop: " + i);
}


//Object.
var a = {
	"name": "misa",
	'naxx': 'misa',
	'age': 18,
	'working': { on: 'mario' }
};

a.name = "xx";
a.age = 24;
debug(a);

arr = [1];
arr[10] = "hhh";
arr[11] = {
  foobar: 10
};
debug(arr);

//var and let
cc1 = "cc1";
cc2 = "cc2";
{
	let cc1 = 1;
	var cc2 = 2;
	debug(cc1);
	debug(cc2);
}
debug(cc1);
debug(cc2);

//callback
function f(callback, s) {
	callback(s);
}

f(function(x) { debug(x); }, "callback test");

const x = "aaa";
x = "bbb";
debug(x);

