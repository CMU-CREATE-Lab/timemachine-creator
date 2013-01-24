var system = require('system');
var template_path = system.args[1];
var frame = system.args[2];
var destination = system.args[3];
var width = system.args[4];
var height = system.args[5];

var page = require('webpage').create();
var url = template_path + '#' + frame;
page.viewportSize = {width:width, height:height};
page.clipRect = {top:0, left:0, width:width, height:height};

page.open(url, function (status) {
    console.log('Rendering ' + url + ' to ' + destination);
    page.render(destination);
    phantom.exit();
});

