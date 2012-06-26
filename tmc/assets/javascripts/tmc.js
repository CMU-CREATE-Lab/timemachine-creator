var definitionPath = '';
var projectPath = '';
var projectName = '';
var inProgress = false;
var projectModified = false;
var captureTimeParserPath = api.getRootAppPath()+"/ct/extract_exif_capturetimes.rb";
var undoArray = new Array();
var redoArray = new Array();
var filmstripIndex = 0;
var activeData = new Array();
var filmstripMode = false;
var data = new Array();
var scale = 64;
var min = 0;
var view = "thumbnails";
var max;
var selectedImages = new Array();
var shiftAlreadyClicked = true;
var tileSize = 512;

function setOutputSettings() {
    var outputData = {}
    outputData["source"] = {}
    outputData["source"]["type"] = "images";
    outputData["source"]["images"] = activeData["images"];
    outputData["source"]["capture_times"] = activeData["capture_times"];
    outputData["source"]["capture_time_parser"] = captureTimeParserPath;
    outputData["source"]["tilesize"] = tileSize;

    outputData["videosets"] = []

    var videoSet = {}
    videoSet["label"] = "Small";
    videoSet["type"] = "h.264";
    videoSet["size"] = "small";
    videoSet["compression"] = $("#compression").slider_x("value");
    videoSet["fps"] = $("#fps").spinner("value");

    outputData["videosets"].push(videoSet);

    videoSet = {};
    videoSet["label"] = "Large";
    videoSet["type"] = "h.264";
    videoSet["size"] = "large";
    videoSet["compression"] = $("#compression").slider_x("value");
    videoSet["fps"] = $("#fps").spinner("value");

    outputData["videosets"].push(videoSet);

    projectModified = false;
    saveAction(false);

    return outputData;
}

function saveAs() {
  var startDirectory = '';
  var tmpPath = api.saveAsDialog('Save Time Machine Definition', startDirectory, '*.tmc');

  if (!tmpPath) {
    alert('Save canceled');
  } else {

    var startIndex = tmpPath.lastIndexOf("/");
    var endIndex = tmpPath.lastIndexOf(".tmc");
    if (endIndex == -1) endIndex = tmpPath.length;
    projectName = tmpPath.substring(startIndex+1,endIndex);
    projectPath = tmpPath.substring(0,endIndex);
    var tmcPath = projectPath.substring(0,startIndex)+"/"+projectName+"/"+projectName+".tmc";

    if (!api.makeFullDirectoryPath(tmcPath)) {
      alert('Error creating directory ' + tmcPath);
      return;
    }

    definitionPath = tmcPath + '/definition.tmc';

    var outputData = setOutputSettings();

    if (!api.writeFile(definitionPath, JSON.stringify(outputData, undefined, 2))) {
      alert('Error creating ' + definitionPath);
      return;
    }
    alert('Project created and saved.');
  }
}

function save() {
  var outputData = setOutputSettings();

  if(!definitionPath) {
    saveAs();
    return;
  }else if (!api.writeFile(definitionPath, JSON.stringify(outputData, undefined, 2))) {
    alert('Error creating ' + definitionPath);
    return;
  }
  //alert('Project Saved');
}

function newProject() {
  if (projectModified) {
    $("#dialog-confirm").data('recentProjectPath', null);
    $("#dialog-confirm").data('clearProject', true);
    $("#dialog-confirm").dialog("open");
    $('.ui-dialog :button').blur();  
  }else {
    clearProjectData();
  }
}

function clearProjectData() {
  definitionPath = '';
  projectPath = '';
  projectName = '';
  projectModified = false;
  undoArray.length = 0;
  redoArray.length = 0;
  activeData.length = 0;
  data.length = 0;
  selectedImages.length = 0;
  shiftAlreadyClicked = false;
  
  $("#numFrames").text(data.length + " Frames")
  $('#zoom').hide();
  $('#scroll').css({'overflow':'hidden'});

  api.setUndoMenu(false);
  api.setRedoMenu(false);
  saveAction(false);  

  reactivateUI();
  
  rescale();
  refresh();
}

function openData(startDirectory) {
  if (typeof startDirectory == 'undefined') file = api.readFileDialog('Open Time Machine Definition', '', '*.tmc');
  else file = api.readFile(startDirectory);

  if (!file) return;
  tmd = JSON.parse(file);
  if (!tmd) return;

  $("#btnToggle").button("enable");

  if (filmstripMode) {
    $("#filmstripControls").show();
    $('#scroll').css({'overflow':'auto'});
  } else {
    $('#zoom').show();
    $('#scroll').css({'overflow':'auto'});
  }

  // load data
  data = new Array();
  for (c = 0; c < tmd["source"]["images"].length; c++) {
    //console.log(c);
    col = new Array();
    for (r = 0; r < tmd["source"]["images"][c].length; r++) {
      h = {};
      h['filename'] = tmd["source"]["images"][c][r];
      if (tmd["source"]["capture_times"] == undefined) {
        h['time'] = api.exifTime(h['filename']);
      } else {
        h['time'] = tmd["source"]["capture_times"][c][r];
      }
      h['row'] = r;
      h['col'] = c;
      h['deleted'] = false;
      col.push(h);
    }
    data.push(col);
  }

  // load video settings
  $("#compression").slider_x("value", tmd["videosets"][0]["compression"]);
  $("#fps").spinner("value", tmd["videosets"][0]["fps"]);
  updateActiveData();
  rescale();
  refresh();
  $("#numFrames").text(data[0].length + " Frames");

  definitionPath = api.getOpenedProjectPath();

  var endIndex = definitionPath.lastIndexOf(".tmc/");
  var tmpPath = definitionPath.substring(0,endIndex);
  endIndex = tmpPath.lastIndexOf("/");
  projectPath = tmpPath.substring(0,endIndex);
  var startIndex = projectPath.lastIndexOf("/");
  projectName = projectPath.substring(startIndex+1);
  projectModified = false;

  if (api.fileExists(projectPath+"/"+projectName+".timemachine/COMPLETE")) {
    $("#render_export_btn").hide();
    $("#status_window").html("");
    $("#status_window").append("<p><font color='green'>Time Machine completed.</font></p>");
    $("#status_window").append("<br/><button id='change_settings_btn'>Change settings for new render/export</button><br/>");
      $("#change_settings_btn").button().click(function () {
       reactivateUI();
    });
    $("#status_window").append("<br/><button id='view_timemachine'><b>View Time Machine</b></button><br/>");
    $("#view_timemachine").button().click(function () {
      openInBrowser(projectPath+"/"+projectName+".timemachine/view.html");
    });
  } else {
    $("#render_export_btn").show();
    $("#status_window").html("");
  }
}

function undoAction() {
  var itemsToUndo = undoArray.pop();
  redoArray.push(itemsToUndo);
  //console.log(redoArray);
  for (i = 0; i < itemsToUndo.length; i++) {
    data[itemsToUndo[i].col][itemsToUndo[i].row].deleted = false;
  }
  api.setRedoMenu(true); // enables the redo menu
  if (undoArray.length == 0) api.setUndoMenu(false); // disables the undo menu
  updateActiveData();
  $("#numFrames").text(activeData["images"][0].length + " Frames");
  rescale();
  refresh();
}

function redoAction() {
  var itemsToRedo = redoArray.pop();
  undoArray.push(itemsToRedo);
  //console.log(undoArray);
  for (i = 0; i < itemsToRedo.length; i++) {
    data[itemsToRedo[i].col][itemsToRedo[i].row].deleted = true;
  }
  api.setUndoMenu(true); // enables the undo menu
  if (redoArray.length == 0) api.setRedoMenu(false); // disables the undo menu
  $("#numFrames").text(activeData["images"][0].length + " Frames");
  updateActiveData();
  rescale();
  refresh();
}

// called by the add images/directories menu
function imagesDropped() {
  $('body').css( 'cursor', 'wait' );
  addDroppedFiles();
}

// enable or disable open project menu
function openProjectAction(state) {
  api.setOpenProjectMenu(state);
}

// enable or disable save menu
function saveAction(state) {
  api.setSaveMenu(state);
}

// enable or disable saveAs menu
function saveAsAction(state) {
  api.setSaveAsMenu(state);
}

// enable or disable add images menu
function addImagesAction(state) {
  api.setAddImagesMenu(state);
}

// enable or disable add folder menu
function addFoldersAction(state) {
  api.setAddFoldersMenu(state);
}

// enable or disable recently added menu
function recentlyAddedAction(state) {
  api.setRecentlyAddedMenu(state);
}

// enable or disable new project menu
function newProjectAction(state) {
  api.setNewProjectMenu(state);
}

// called when the user tries to exit the app
function isSafeToClose() {
  //console.log("issafetoclose");
  if (projectModified) {
    $("#dialog-confirm").data('recentProjectPath', null).dialog("open");
    $('.ui-dialog :button').blur();
    return false;
  } else {
    return true;
  }
}

// open recent project
function openRecentProject(path) {
  if (projectModified) {
    $("#dialog-confirm").data('recentProjectPath', path).dialog("open");
    $('.ui-dialog :button').blur();
  } else {
    openData(path);
  }
}

// open url in a new browser
function openInBrowser(url) {
  api.openBrowser(url);
}

function getTime(pct) {
  var scroll = document.getElementById('scroll').scrollTop;
  return min + (scroll + $("#canvas").height() * pct) / scale;
}

function setTime(ctr) {
  document.getElementById('scroll').scrollTop = scale * (ctr - min) - $("#canvas").height() * 0.5;
}

function format2(n) {
  var ret = "" + n;
  return (ret.length == 1) ? "0" + ret : ret;
}

function isNumber(n) {
  return !isNaN(parseFloat(n)) && isFinite(n);
}

function jumpTo() {
  if (isNumber($("#jumpToFrame").val())) filmstripIndex = $("#jumpToFrame").val() - 1;
}

// Finds the index of the closest element in column b to element a in column 0 of data
function findClosest(a, b) {
  dummyOffset = 0; // we must have one for each column 1-end
  l = 0;
  r = data[b].length - 1;

  while (r - l > 1) {
    m = Math.floor((r + l) / 2);
    if (data[b][m].time + dummyOffset < data[0][a].time) l = m;
    else r = m;
  }

  if (Math.abs(data[b][l].time + dummyOffset - data[0][a].time) < Math.abs(data[b][r].time + dummyOffset - data[0][a].time)) return l;
  return r;
}

function computeDeltas() {
  // Deltas in first column are from previous image
  var col = data[0];
  for (var r = 1; r < col.length; r++) {
    col[r].delta = Math.round((col[r].time - col[r - 1].time) * 100) / 100;
  }
  // Deltas in other colums are relative to first column
  for (var c = 1; c < data.length; c++) {
    var col = data[c];
    var reference = data[0];
    var ref_r = 0;
    for (var r = 0; r < col.length; r++) {
      while (ref_r < reference.length && reference[ref_r].time < col[r].time) ref_r++;
      var reftime;
      if (ref_r == 0) reftime = reference[ref_r].time;
      else if (ref_r == reference.length) reftime = reference[ref_r - 1].time;
      else if (Math.abs(col[r].time - reference[ref_r - 1].time) < Math.abs(col[r].time - reference[ref_r].time)) {
        reftime = reference[ref_r - 1].time;
      } else {
        reftime = reference[ref_r].time;
      }
      col[r].delta = Math.round((col[r].time - reftime) * 100) / 100;
    }
  }
}

function updateActiveData() {
  activeData = {};
  activeData["images"] = new Array();
  activeData["capture_times"] = new Array();

  for (var i = 0; i < data.length; i++) {
    activeData["images"].push(new Array());
    activeData["capture_times"].push(new Array());
    for (var j = 0; j < data[i].length; j++) {
      if (data[i][j].deleted == false) {
        activeData["images"][i].push(data[i][j].filename);
        activeData["capture_times"][i].push(data[i][j].time);
      }
    }
  }
  //console.log(activeData);
}

function refresh() {
  //console.log('refresh');
  if (activeData.length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) {
    var canvas = document.getElementById('canvas');
    canvas.width = canvas.width;
    var context = canvas.getContext('2d');
    context.fillStyle = 'black';
    context.textBaseline = 'top';
    context.font = '16px sans-serif';
    context.fillText("No images loaded.", ($("#canvas").width()/2 - 135), 100);
    $("#btnToggle").button("disable");
    $("#zoom").hide();
    return;
  }

  if (filmstripMode) {

    if (activeData.length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) return;

    $("#btnPrevImage").removeAttr('disabled');
    $("#btnNextImage").removeAttr('disabled');
    if (filmstripIndex <= 0) {
      filmstripIndex = 0;
      $("#btnPrevImage").attr('disabled', 'disabled');
    }
    if (filmstripIndex >= activeData["images"][0].length - 1) {
      filmstripIndex = activeData["images"][0].length - 1;
      $("#btnNextImage").attr('disabled', 'disabled');
    }

    $("#jumpToFrame").val(filmstripIndex+1)

    $("#filmstripText").text(activeData["images"][0].length);

    var imageObj = new Image();
    var prepend = (navigator.appVersion.indexOf("Win")!=-1) ? "file:///" : "file://"
    imageObj.src = prepend+activeData["images"][0][filmstripIndex];          

    var maxWidth = $("#canvas").width(); // Max width for the image
    var maxHeight = $("#canvas").height() - 140;    // Max height for the image
    var ratio = 0;  // Used for aspect ratio
    var Awidth = 3648;    // Current image width
    var Aheight = 2736;  // Current image height

    // Check if the current width is larger than the max
    if(Awidth > maxWidth){
      ratio = maxWidth / Awidth;   // get ratio for scaling image
      Aheight = Aheight * ratio;    // Reset height to match scaled image
      Awidth = Awidth * ratio;    // Reset width to match scaled image
    }

    // Check if current height is larger than max
    if(Aheight > maxHeight){
      ratio = maxHeight / Aheight; // get ratio for scaling image
      Aheight = Aheight * ratio;    // Reset height to match scaled image
      Awidth = Awidth * ratio;    // Reset width to match scaled image
    }

    var canvas = document.getElementById("canvas");
    var context = canvas.getContext("2d");
    var x = 0;
    var y = 55;
    var width = Awidth;
    var height = Aheight

    imageObj.onload = function() {
      canvas.width = canvas.width
      context.drawImage(imageObj, x, y, width, height);
      context.fillText(activeData["images"][0][filmstripIndex],x,height+y+8);
      var date = new Date(1000 * activeData["capture_times"][0][filmstripIndex]);
      var text = date.toLocaleDateString() + " " + date.toLocaleTimeString(); 
      context.fillText(text,x,y-2);
    };
    return;
  }

  $("#zoom").show();

  var mintime = getTime(-1);
  var toptime = getTime(0);
  var maxtime = getTime(1);

  var ticktimes = [.1, .2, .5, 1, 2, 5, 10, 30, 60, 60 * 2, 60 * 5, 60 * 15, 60 * 30, 3600, 2 * 3600, 4 * 3600, 12 * 3600, 86400];
  // We want to display time axis labels no closer than every N pixels
  var tickPixels = 100;
  var ticktime = tickPixels / scale;
  for (var i = 0; i < ticktimes.length; i++) {
    if (ticktimes[i] >= ticktime) {
      ticktime = ticktimes[i];
      break;
    }
  }

  var canvas = document.getElementById('canvas');
  canvas.width = canvas.width;
  var context = canvas.getContext('2d');

  context.fillStyle = 'black';
  context.textBaseline = 'top';
  context.font = '9px sans-serif';

  var loading = false;

  if (activeData["images"].length > 0 && activeData["images"][0].length > 0) {
    for (var i = Math.floor(mintime / ticktime); i * ticktime <= maxtime; i++) {
      var time = i * ticktime;
      var y = scale * (time - toptime);
      var date = new Date(time * 1000);
      var label = date.getFullYear() + "-" + format2(date.getMonth() + 1) + "-" + format2(date.getDay()) + " " + format2(date.getHours()) + ":" + format2(date.getMinutes()) + ":" + format2(date.getSeconds()) + "." + Math.floor(date.getMilliseconds() / 100);
      context.fillText(label, 1, y);
    }
  }
  var left_margin = 120;

  for (var c = 0; c < data.length; c++) {
    var col = data[c];
    for (var r = 0; r < col.length; r++) {
      var elt = col[r];
      if (elt.deleted) continue;
      if (elt.time >= mintime && elt.time <= maxtime) {
        var y = scale * (elt.time - toptime);
        var legend = elt.filename.replace(".time", "");
        if (elt.delta != undefined) legend += " " + elt.delta;
        if (view == "filenames") {
          context.fillText(legend, left_margin + c * 150, y);
        } else if (view == "thumbnails") {
          if (!elt.image) {
            var thumnail;
            if ((thumbnail = api.readThumbnail(elt.filename)) == null) {
              context.fillRect(x, y, 160, 120);
            } else {
              var img = new Image;
              thumbnail.assignToHTMLImageElement(img);
              elt.image = img;
            }
          }
          var x = left_margin + c * 150;
          elt.w = 160;
          elt.h = 120;
          elt.x = x;
          elt.y = y;
          //context.save();
          //context.transform(0, -1, 1, 0, (x + 80) - (y + 80), (x + 80) + (y + 80));
          if (!elt.image.complete) loading = true;
          if (elt.image.complete && elt.image.width > 0) {
            //console.log("drawing image");
            //console.log(elt.image);
            context.drawImage(elt.image, x, y);

            if (jQuery.inArray(elt, selectedImages) >= 0) {
              //console.log(selectedImages);
              context.strokeStyle = '#CC0000';
              context.lineWidth = 2;
              context.strokeRect(elt.x, elt.y, 160, 120);
            }

          } else {
            //console.log("drawing box");
            context.fillRect(x, y, 160, 120);
          }
          //context.restore();
          context.fillText(legend, x, y + 121);
        } else {
          //console.log("drawing lines");
          context.fillRect(left_margin + c * 25, y, 25, 1);
        }
      }
    }
  }

  // Code from saman for multi-camera support
  // I have assumed that each column in "data" is sorted based on the time stamps.
  var newdata = [];
  var best = [];
  var timeTH = 2; // maximum time difference (in seconds) between images in other cameras with images in camera 1
  for (var i = 0; i < data.length - 1; i++) {
    newdata.push([]);
    best.push(0);
  }

  for (var i = 0; i < data[0].length; i++) // finding the counterpart images for each image in camera 0
  {
    var haveFound = 1;
    best[0] = i;
    for (var b = 1; b < data.length; b++) // finding the closesth image (in time) to the original image in other cameras
    {
      best[b] = findClosest(i, b);
      if (Math.abs(data[b][best[b]].time - data[0][i].time) > timeTH) // in this case, there is no way that we can use this image in camera 0
      {
        haveFound = 0;
        break;
      }
    }

    if (newdata.length > 0 && haveFound == 1) // we have found all the counterpart images. now we have to save them
    {
      for (var j = 0; j < data.length; j++)
      newdata[j].push(data[j][best[j]]);
    }
  }

  // showing the actual images
  var left_margin = 400;

  for (var c = 0; c < newdata.length; c++) {
    var col = newdata[c];
    for (var r = 0; r < col.length; r++) {
      var elt = col[r];
      if (elt.time >= mintime && elt.time <= maxtime) {
        var y = scale * (newdata[0][r].time - toptime);
        var legend = elt.filename.replace(".time", "");
        if (elt.delta != undefined) legend += " " + elt.delta;
        if (view == "filenames") {
          context.fillText(legend, left_margin + c * 150, y);
        } else if (view == "thumbnails") {
          if (!elt.image) {
            elt.image = new Image();
            elt.image.src = elt.filename.replace(".time", ".thumb");
          }
          var x = left_margin + c * 150;
          context.save();
          context.transform(0, -1, 1, 0, (x + 80) - (y + 80), (x + 80) + (y + 80));
          if (!elt.image.complete) loading = true;
          if (elt.image.complete && elt.image.width > 0) {
            context.drawImage(elt.image, x, y);
          } else {
            context.fillRect(x, y, 160, 120);
          }
          context.restore();
          context.fillText(legend, x, y + 161);
        } else {
          context.fillRect(left_margin + c * 25, y, 25, 1);
        }
      }
    }
  }
  // end of code from saman
  if (loading) setTimeout(refresh, 100);
}

function rescale() {
  $("#tabs").css({
    "height": $("body").height()-6 + "px",
    "width": $("body").width()-6 + "px",
  });        

  $("#tabs-1").css({
    "height": $("body").height()-70 + "px",
    "width": $("body").width()-46 + "px",
  });  

  $("#tabs-2").css({
    "height": $("body").height()-56 + "px",
    "width": $("body").width()-46 + "px",
  });          

  if (filmstripMode) {

    $("#scroll").css({
      "height": ($("#tabs").height() - $("#top_area").height() - 58) + "px"
    });


    $("#controls").css({
      "height": ($("#tabs").height() - 56) + "px"
    });

    $("#canvasdiv").css({
      "height": ($("#scroll").height()) + "px",

      "left": "80px"
    });

    $("#top_area").css({
      "width": ($("#tabs").width() - 197) + "px"
    });

    $("#canvas").attr("width", $("#canvasdiv").width());
    $("#canvas").attr("height", $("#canvasdiv").height());        

    return;
  }

  min = 1e+100;
  max = -1e+100;

  for (var c = 0; c < data.length; c++) {
    var col = data[c];
    for (var r = 0; r < col.length; r++) {
      var elt = col[r];
      if (elt.deleted) continue;
      if (elt.time < min) min = elt.time;
      if (elt.time > max) max = elt.time;
    }

  }

  $("#world").height((max - min) * scale + 200); // Height of a thumnail (160) + height of filename text (8) + extra padding (32)
  $("#scroll").css({
    "height": ($("#tabs").height() - $("#top_area").height() - 58) + "px"
  });

  $("#controls").css({
    "height": ($("#tabs").height() - 56) + "px"
  });    

  $("#canvasdiv").css({
    "height": ($("#controls").outerHeight(true) - $("#top_area").outerHeight(true)-3) + "px",
    "left": "200px"
  });

  $("#top_area").css({
    "width": ($("#tabs").width() - 197) + "px"
  });

  $("#canvas").attr("width", $("#canvasdiv").width());
  $("#canvas").attr("height", $("#canvasdiv").height());

  var time_per_pixel = 1 / scale;
  var msg = "1 pixel = ";
  if (time_per_pixel < 1) {
    msg += Math.round(time_per_pixel * 1000) + " milliseconds";
  } else if (time_per_pixel < 60) {
    msg += time_per_pixel + " seconds";
  } else if (time_per_pixel < 3600) {
    msg += (Math.round(time_per_pixel / 60 * 100) / 100) + " minutes";
  } else {
    msg += (Math.round(time_per_pixel / 3600 * 100) / 100) + " hours";
  }
  $("#scale").html(msg);

  if (scale < 64) view = "lines"
  else view = "thumbnails"

}

function zoom(factor) {
  if (factor == .5) $("#zoomSlider").slider("value", ($("#zoomSlider").slider("option", "value") - $("#zoomSlider").slider("option", "step")));
  else if (factor == 2) $("#zoomSlider").slider("value", ($("#zoomSlider").slider("option", "value") + $("#zoomSlider").slider("option", "step")));

  var time = getTime(0.5);

  if (scale * factor >= 2048 || scale * factor <= (1 / 128)) return;

  scale *= factor;

  rescale();
  setTime(time);
  refresh();
}

function keypress(e) {
  //var key = String.fromCharCode(e.keyCode);

  switch (e.which) {
  case 61:
    // equal/plus sign
  case 43:
    zoom(2);
    break;
  case 45:
    // minus sign
    zoom(0.5);
    break;
  case 127: // numpad delete?
  case 8: // backsapce key
  case 46: // delete key
    // delete
    doDelete();
    break;
  default:
    //console.log("key " + e.which);
  }
}

function doDelete() {
  if (selectedImages.length == 0) return;

  projectModified = true;
  saveAction(true);
  for (i = 0; i < selectedImages.length; i++) {
    // Mark as deleted, but do not actually delete. Useful for undo feature
    data[selectedImages[i].col][selectedImages[i].row].deleted = true;
  }
  undoArray.push(new Array());
  jQuery.extend(true, undoArray[undoArray.length - 1], selectedImages);
  if (undoArray.length > 0) api.setUndoMenu(true);
  selectedImages.length = 0;

  updateActiveData()
  $("#numFrames").text(activeData["images"][0].length + " Frames");
  api.setDeleteMenu(false);
  rescale();
  refresh();
}

function inBounds(elm, mx, my) {
  // Make sure mouse x,y fall in the area between the
  // shapes x and (x + height) and its y and (y + height)
  return (elm.x <= mx) && (elm.x + elm.w >= mx) && (elm.y <= my) && (elm.y + elm.h >= my);
}

function startRender() {
  inProgress = true;

  // Call save dialog window if the current project has not been saved
  if (!definitionPath) saveAs();
  else if (projectModified) save();

  $("#tabs").tabs("disable","tabs-1");
  $("#compression").slider_x("disable");
  $("#fps").spinner("disable")
  $("#num_jobs").button("disable");
  openProjectAction(false);
  saveAction(false);
  saveAsAction(false);
  addImagesAction(false);
  addFoldersAction(false);
  recentlyAddedAction(false);
  newProjectAction(false);
  api.setDeleteMenu(false);

  undoArray.length = 0;
  redoArray.length = 0;
  api.setUndoMenu(false);
  api.setRedoMenu(false);

  $("#render_export_btn").hide();
  $("#status_window").html("");
  $("#status_window").append("<p id='status_text'>In progress...</p>");
  $("#status_window").append("Destination: " + projectPath+"/"+projectName+".timemachine" + "<br/>");
  $("#status_window").append("Started: " + get_current_time_formatted() + "<br/>");
  $("#status_window").append("Progress: <span id='current_progress' style='position: absolute;margin-top: 2px;'>0.0%</span>");
  $("#status_window").append("<br/><button style='margin-top: 10px' id='cancel_render_btn'>Cancel</button>");

  $("#cancel_render_btn").button().click(function () {
    //alert('cancel render!');
    api.killSubprocess();
  });
  $("#current_progress").progressBar({ barImage: 'assets/images/progressbg_black.gif'} );
  run_ct();
}

function init() {
  console.log("loaded");

  $("#numFrames").text(data.length + " Frames")
  $('#zoom').hide();
  $('#scroll').css({'overflow':'hidden'});

  api.setDeleteMenu(false);
  api.setUndoMenu(false);
  api.setRedoMenu(false);
  saveAction(false);

  $("#btnToggle").button().css({
    'color': 'inherit',
    'text-align': 'center',
    'outline': 'none',
    'font-size': '.8em',
    'width': '95px',
    'height': '25px'
  });

  $("#btnToggle").html('Slideshow View');
  $("#btnToggle").attr('title', 'Slideshow View');
  $("#tmFormat").button().css({
    'font-size': '.8em',
    'text-align': 'center',
    'width': '70px',
    'height': '30px'
  });

  $("#btnToggle").button("disable");

  $("#render_export_btn").button().click(function () {
    startRender();
  });

  var dropbox = document.getElementById("dropbox");
  dropbox.addEventListener("dragenter", noopHandler, false);
  dropbox.addEventListener("dragexit", noopHandler, false);
  dropbox.addEventListener("dragover", noopHandler, false);
  dropbox.addEventListener("drop", dropHandler, false);
  dropbox.addEventListener("dragleave", removeDropHighlight, false);

  $(document).bind('keydown',function(e){
      if (filmstripMode) {
        if (e.which==37 || e.which==39) {
            if ($(e.target).is('input') ) { return; }
            e.preventDefault();
            if (e.which==37) {
              filmstripIndex--; 
              refresh();
            } else {
              filmstripIndex++; 
              refresh();
            }
        }
      }
  }); 

  $("#jumpToFrame").keyup(function(event){
      if(event.keyCode == 13){
        jumpTo();
        refresh();
      }
  });

  $('.ui-state-default').hover(

  function () {
    $(this).addClass('ui-state-hover');
  }, function () {
    $(this).removeClass('ui-state-hover');
  });

  $('#fps').spinner({
    min: 1,
    max: 99,
    increment: 'fast'
  });

  $('#compression').slider_x({
    min: 24,
    max: 30,
    step: 2,
    value: 26,
    tickLabelsCount: 3,
    tickMarksCount: 3,
    handleLabels: false
  });

  $("#change-format").dialog({
    autoOpen: false,
    modal: true,
    width: 550,
    resizable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });

  $("#dialog-message").dialog({
    autoOpen: false,
    modal: true,
    width: 430,
    resizable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });

  $("#fps-dialog-message").dialog({
    autoOpen: false,
    modal: true,
    width: 430,
    resizable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });        

  $("#jobs-dialog-message").dialog({
    autoOpen: false,
    modal: true,
    width: 430,
    resizable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });        

  $("#compression-dialog-message").dialog({
    autoOpen: false,
    modal: true,
    width: 430,
    resizable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });          

  $("#dialog-confirm").dialog({
    autoOpen: false,
    resizable: false,
    height:230,
    modal: true,
    buttons: {
      "Yes": function() {
        projectModified = false;
        $(this).dialog("close");
        save();
        var path = $(this).data('recentProjectPath');
        var clearProject = $(this).data('clearProject');
        if (path) openData(path);
        else if (clearProject) clearProjectData();
        else api.closeApp();
      },
      "No": function() {
        projectModified = false;
        $(this).dialog("close");
        var path = $(this).data('recentProjectPath');
        var clearProject = $(this).data('clearProject');
        if (path) openData(path);
        else if (clearProject) clearProjectData();
        else api.closeApp();
      }
    }
  });

  $('input:text, input:password').button().css({
    'color': 'inherit',
    'text-align': 'left',
    'outline': 'none',
    'cursor': 'text',
    'font-size': '.8em',
    'width': '15px',
    'height': '10px'
  });

  $("#tmFormat").click(function () {
    $("#change-format").dialog("open");
    $('.ui-dialog :button').blur();
  });

  $("#jobs_explanation").click(function () {
    $("#jobs-dialog-message").dialog("open");
  });

  $("#fps_explanation").click(function () {
    $("#fps-dialog-message").dialog("open");
  });

  $("#compression_explanation").click(function () {
    $("#compression-dialog-message").dialog("open");
  });

  $("#compression").slider_x({
     change: function(event, ui) {
       projectModified = true;
       saveAction(true);
     }
  });

  $('#fps').spinner().change(function(){
    projectModified = true;
    saveAction(true);
  });        

  $("body").keypress(keypress);

  $("#zoomSlider").slider({
    orientation: "vertical",
    range: "min",
    min: 0,
    max: 16,
    value: 12,
    slide: function (event, ui) {
      if ($(this).slider("option", "value") > ui.value) {
        zoom(.5);
        $("#zoomSlider").slider("option", "value", ($("#zoomSlider").slider("option", "value") * 0.5));
      } else {
        zoom(2);
        $("#zoomSlider").slider("option", "value", ($("#zoomSlider").slider("option", "value") * 2));
      }
    }, stop: function (event, ui) {
      var values = [0.015625, 0.03125, 0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024];
      scale = values[$("#zoomSlider").slider("option", "value")];
      rescale(1);
      refresh();
    }
  });

  $("#zoomSlider .ui-slider-handle").attr("title", "Drag to zoom");

  setupSliderHandlers();

  var mouseSelection = false;
  var originalPos;
  var originalCanvasPos;

  $('#world').mousedown(function (e) {
    if (data.length == 0 || data[0].length == 0) return;
    mouseSelection = true;
    originalPos = [e.pageX, e.pageY];
    originalCanvasPos = canvas.relMouseCoords(e);

    var coords = originalCanvasPos;
    var itemInBounds = false;

    var mintime = getTime(-1);
    var maxtime = getTime(1);

    for (i = 0; i < data[0].length; i++) {
      if ((data[0][i].deleted) || !(data[0][i].time >= mintime && data[0][i].time <= maxtime)) continue;

      if (inBounds(data[0][i], coords.x, coords.y)) {
        api.setDeleteMenu(true);
        if (!(e.ctrlKey || e.metaKey) && !e.shiftKey) selectedImages.length = 0;

        if (e.shiftKey) {

          if (selectedImages.length == 0) {
            for (j = 0; j <= i; j++) {
              selectedImages.push(data[0][j]);
            }
          } else {

            var oneClicked = data[0][i];
            var lastOneClicked = selectedImages[selectedImages.length - 1];
            var lastOneClickedIndex = $.inArray(lastOneClicked, data[0]);
            var firstOneClicked = selectedImages[0];
            var firstOneClickedIndex = $.inArray(firstOneClicked, data[0]);

            if (shiftAlreadyClicked && (oneClicked.filename == lastOneClicked.filename)) break;

            selectedImages.length = 0;
            var startIndex = shiftAlreadyClicked ? firstOneClickedIndex : lastOneClickedIndex;
            if (i > lastOneClickedIndex) {
              for (j = startIndex; j <= i; j++) {
                selectedImages.push(data[0][j]);
              }
            } else {
              for (j = startIndex; j >= i; j--) {
                selectedImages.push(data[0][j]);
              }
            }
          }
          shiftAlreadyClicked = true;
        } else {
          shiftAlreadyClicked = false;
          if ($.inArray(data[0][i], selectedImages) < 0) selectedImages.push(data[0][i]);
          else selectedImages.splice($.inArray(data[0][i], selectedImages), 1);
        }
        itemInBounds = true;
        break;
      }
    }

    if (!itemInBounds) {
      api.setDeleteMenu(false);
      if (!(e.ctrlKey || e.metaKey) && !e.shiftKey) selectedImages.length = 0;
    }
    refresh();

  });

  $('body').mousemove(function (e) {
    if (mouseSelection) {

      if ($('#selection_area').length == 0) {
        var div = $('<div id="selection_area" style="position: absolute; border-style:dotted; border-width:2px; border-color: #666d68; z-index: 400"></div>');
        div.css({
          'left': e.pageX,
          'top': e.pageY,
          'width': 0,
          'height': 0
        });
        $('body').append(div);
      }

      var x1 = originalPos[0], y1 = originalPos[1], x2 = event.pageX, y2 = event.pageY;
      var coords = canvas.relMouseCoords(e);
      var origCanvasX = originalCanvasPos.x, origCanvasY = originalCanvasPos.y;
      var width = (x2 - x1), height = (y2 - y1);

      if (x1 > x2) {
        var tmp = x2;
        x2 = x1;
        x1 = tmp;
        width = (x2 - x1);
        origCanvasX -= width;
        coords.x += width;
      }

      if (y1 > y2) {
        var tmp = y2;
        y2 = y1;
        y1 = tmp;
        height = (y2 - y1);
        origCanvasY -= height;
        coords.y += height;
      }

      $('#selection_area').css({
        'left': x1,
        'top': y1,
        'width': width,
        'height': height
      });

      var mintime = getTime(-1);
      var maxtime = getTime(1);

      //!(r2.left > r1.right || r2.right < r1.left || r2.top > r1.bottom || r2.bottom < r1.top);
      for (i = 0; i < data[0].length; i++) {

        // There is something wrong with my collision code or something else is at play.
        // Either way, checking whether the image is currently visible seems to fix this.   
        if ((data[0][i].deleted) || !(data[0][i].time >= mintime && data[0][i].time <= maxtime)) continue;

        // Determine whether the selection rectangle intersects one of our data objects (also a rectangle)
        if (!(origCanvasX > data[0][i].x + data[0][i].w || coords.x < data[0][i].x || origCanvasY > data[0][i].y + data[0][i].h || coords.y < data[0][i].y)) {
          if ($.inArray(data[0][i], selectedImages) < 0) {
            selectedImages.push(data[0][i]);
            api.setDeleteMenu(true);
            refresh();
            break;
          }
        } else {
          var idx = $.inArray(data[0][i], selectedImages);
          if (idx >= 0) {
            selectedImages.splice(idx, 1);
            refresh();
          }
        }
      }
    }
  });

  $(document).mouseup(function (e) {
    $('#selection_area').remove();
    mouseSelection = false;
  });

  $("#tabs").tabs();

  rescale();
  refresh();
}

function padNumber(number, length) {
  var str = '' + number;
  while (str.length < length) {
    str = '0' + str;
  }
  return str;
}

function run_ct() {
  //console.log(projectPath+"/"+projectName+".timemachine/view.html");
  if (!api.invokeRubySubprocess([api.getRootAppPath()+'/ct/ct.rb', '-j', $("#num_jobs").val().toString(), definitionPath, projectPath+"/"+projectName+".timemachine"], ct_out)) {
    alert('There was an error starting the process.');
  }
}

function reactivateUI() {
  openProjectAction(true);
  saveAsAction(true);
  addImagesAction(true);
  addFoldersAction(true);
  recentlyAddedAction(true);
  newProjectAction(true);

  $("#tabs").tabs("enable","tabs-1");
  $("#status_window").html("");
  $("#compression").slider_x("enable")
  $("#fps").spinner("enable")
  $("#num_jobs").button("enable");
  $("#render_export_btn").show();
}

function ct_out(out) {
  //console.log("In ct_out, out=" + out);
  if (typeof out === 'number') {
    inProgress = false;
    $("#cancel_render_btn").hide();
    $("#status_window").append("Ended: " + get_current_time_formatted() + "<br/>");
    $("#status_window").append("<br/><button id='change_settings_btn'>Change settings for new render/export</button><br/>");
    $("#change_settings_btn").button().click(function () {
     reactivateUI();
    });

    //linux and mac report an error code of 0 when we kill ct.rb, so we check that we are actually done
    if (out == 0 && (api.fileExists(projectPath+"/"+projectName+".timemachine/COMPLETE"))) { //success
      $("#status_window").append("<br/><button id='view_timemachine'><b>View Time Machine</b></button><br/>");
      $("#view_timemachine").button().click(function () {
        openInBrowser(projectPath+"/"+projectName+".timemachine/view.html");
      });
      $("#status_text").html("<font color='green'>Time Machine completed.</font>");
    } else if (out == 1) { //error
      $("#status_text").html("<font color='red'>An error was encountered.</font>");
      $("#status_window").append("<br/><button id='retry_btn'>Retry</button><br/>");
      $("#retry_btn").button().click(function () {
          startRender();
      });
    } else { //canceled (62097?)
      $("#status_text").html("<font color='red'>Process canceled.</font>");
      $("#status_window").append("<br/><button id='retry_btn'>Resume</button><br/>");
      $("#retry_btn").button().click(function () {
          startRender();
      });
    }
  } else {
    output_string = out.replace(/\r\n|\r|\n/g, "<br />")
    output = output_string.match(/\d+.\d+%/g);
    //console.log(output[0]);
    if (output != null) $("#current_progress").progressBar(parseFloat(output[0].slice(0, -1)));
  }
}

function get_current_time_formatted() {
  var date = new Date();
  var time_formatted = date.getFullYear() + "-" + padNumber(date.getMonth()+1, 2) + "-" + padNumber(date.getDate(), 2) + " " + padNumber(date.getHours(), 2) + ":" + padNumber(date.getMinutes(), 2) + ":" + padNumber(date.getSeconds(), 2);
  //console.log(time_formatted);
  return time_formatted;
}

function setupSliderHandlers() {
  $(".ui-slider-handle").bind("mouseover mouseup", function() {
    this.style.cursor = 'url("assets/stylesheets/cursors/openhand.png") 10 10, move';
  });

  $(".ui-slider").bind({
    slide: function() {
      this.style.cursor = 'url("assets/stylesheets/cursors/closedhand.png") 10 10, move';
      $(".ui-slider-handle").bind("mousemove", function() {
        this.style.cursor = 'url("assets/stylesheets/cursors/closedhand.png") 10 10, move';
      });
    }, slidestop: function() {
      $(".ui-slider-handle").bind("mousemove", function() {
        this.style.cursor = 'url("assets/stylesheets/cursors/openhand.png") 10 10, move';
      });
    }, mouseover: function() {
      this.style.cursor = "pointer";
    }
  });
}

// Prevent browser from loading dropped files as if it is a new page
document.addEventListener("dragover", function(e) {
  e = e || event;
  e.preventDefault();
}, false);
document.addEventListener("drop", function(e) {
  e = e || event;
  e.preventDefault();
}, false);

function toggleFilmstrip() {;
  if($("#btnToggle").html() == 'Timeline View') {
    $("#btnToggle").html('Slideshow View');
    $("#btnToggle").attr('title', 'Slideshow View');
    filmstripMode = false;
    $("#filmstripControls").hide();
    $('#scroll').css({'overflow':'auto'});
  } else {
    $('#scroll').css({'overflow':'hidden'});
    $("#zoom").hide();
    $("#btnToggle").html('Timeline View');
    $("#btnToggle").attr('title', 'Timeline View');
    filmstripMode = true;
    filmstripIndex = 0;
    if (activeData.length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) return;
    $("#filmstripControls").show();
  }
  rescale();
  refresh();
}

function zoomIn() {
  zoom(2);
}

function zoomOut() {
  zoom(.5)
}

function noopHandler(evt) {
  $("#dropbox").css({
    "background": "Gainsboro"
  })
  evt.stopPropagation();
  evt.preventDefault();
}

function removeDropHighlight(evt) {
  $("#dropbox").css({
    "background": "#f5f5f5"
  })
}

function addDroppedFiles() {
  $('body').css( 'cursor', 'wait' );

  $("#btnToggle").button("enable");

  if (filmstripMode) {
    $("#filmstripControls").show();
    $('#scroll').css({'overflow':'auto'});
  } else {
    $('#zoom').show();
    $('#scroll').css({'overflow':'auto'});
  }

  projectModified = true;
  saveAction(true);
  var droppedFiles = api.droppedFilesRecursive();

  // TODO: single camera support only at this time
  if (data.length <= 0) data.push(new Array());

  //col = new Array();
  var rowNum = 0;
  for (i = 0; i < droppedFiles.length; i++) {
    if (droppedFiles[i].match(/\.jpg|.png$/i) == null) continue;
    var h = {};
    h['filename'] = droppedFiles[i]; //droppedFiles[i].replace(/\\/g,'/').replace( /.*\//, '' );
    h['time'] = api.exifTime(droppedFiles[i]);
    h['row'] = rowNum++;
    h['col'] = 0;
    h['deleted'] = false;
    data[0].push(h);
  }
  //data.push(col);
  data[0].sort(function(a,b){return a.time-b.time});

  for(i = 0; i < data[0].length; i++) {
    data[0][i].row = i;
    if (i > 0 && data[0][i].time == data[0][i-1].time) {
      data[0].splice(i,1)
      i--;
    }
  }
  //console.log(data);
  $("#numFrames").text(data[0].length + " Frames")

  //computeDeltas();
  if (data[0].length > 0) {
   updateActiveData();
   rescale();
   refresh();
  }
  $('body').css( 'cursor', 'default' );
}

function dropHandler(evt) {
  $("#dropbox").css({
    "background": "#f5f5f5"
  })
  evt.stopPropagation();
  evt.preventDefault();

  addDroppedFiles();
}

function checkData() {
    if (data.length == 0 || data[0].length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) $("#render_export_btn").button("disable");
    else $("#render_export_btn").button("enable");
}

function checkNumJobs(val) {
    if ((val) <= 0 || !isNumber(val)) $("#num_jobs").val(2);
}

HTMLCanvasElement.prototype.relMouseCoords = function (event) {
  var totalOffsetX = 0;
  var totalOffsetY = 0;
  var canvasX = 0;
  var canvasY = 0;
  var currentElement = this;

  do {
    totalOffsetX += currentElement.offsetLeft;
    totalOffsetY += currentElement.offsetTop;
  }
  while (currentElement = currentElement.offsetParent)

  canvasX = event.pageX - totalOffsetX;
  canvasY = event.pageY - totalOffsetY;

  // Fix for variable canvas width
  canvasX = Math.round(canvasX * (this.width / this.offsetWidth));
  canvasY = Math.round(canvasY * (this.height / this.offsetHeight));

  return {
    x: canvasX,
    y: canvasY
  }
}
