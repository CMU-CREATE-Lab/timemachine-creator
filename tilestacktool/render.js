var system = require('system');
var template_path = system.args[1];
var frame = system.args[2];
var destination = system.args[3];

var page = require('webpage').create();
var url = template_path + '#' + frame;
console.log('Loading ' + url);
page.open(url, function (status) {
    console.log('Loaded.');
    console.log('Rendering to ' + destination);
    page.render(destination);
    phantom.exit();
});

