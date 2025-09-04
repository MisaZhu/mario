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
	print("for loop: " + i);
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
print(a);

arr = [1];
arr[10] = "hhh";
arr[11] = {
  foobar: 10
};
print(arr);

//var and let
cc1 = "cc1";
cc2 = "cc2";
{
	let cc1 = 1;
	var cc2 = 2;
	print(cc1);
	print(cc2);
}
print(cc1);
print(cc2);

//callback
function f(callback, s) {
	callback(s);
}

f(function(x) { print(x); }, "callback test");

const x = "aaa";
x = "bbb";
print(x);

