"use strict";

var definitionPath = '';
var projectPath = '';
var projectName = '';
var inProgress = false;
var projectModified = false;
var captureTimeParserPath = api.getRootAppPath() + "/ct/extract_exif_capturetimes.rb";
var undoArray = [];
var redoArray = [];
var filmstripIndex = 0;
var activeData = [];
var filmstripMode = false;
var data = [];
var scale = 64;
var view = "thumbnails";
var selectedImages = [];
var shiftAlreadyClicked = true;
var tileSize = 512;
var worldWidthAndExtra = 0;
var scrollHeightAndExtra = 0;
var ticktime = 0;
var mintime = 0;
var maxtime = 0;
var doFileHover = false;
// Margin of 50 and 9 extra for padding; keep images centered under the dropbox
var left_margin = -59;
var clickedCol = 0;
var dropbox_width = 0;
var half_dropbox_width = 0;
var canvas, context, canvas_times, context_times, min, max, scrollDiv, canvasDiv, contentDiv, dropBoxes;
var rotInDegrees = 0;
var timeTH = 0;
var counterPartImages = [];
var renderPaused = false;
var didTimestampWarning = false;
var sortBy = "timestamp";
var leftMargin = 304;
var $canvasTimes;

function setOutputSettings() {
  var outputData = {};
  outputData['sort_by'] = sortBy;
  outputData["source"] = {};
  outputData["source"]["type"] = "images";
  outputData["source"]["images"] = activeData["images"];
  outputData["source"]["capture_times"] = activeData["capture_times"];
  outputData["source"]["capture_time_parser"] = captureTimeParserPath;
  outputData["source"]["tilesize"] = tileSize;

  outputData["videosets"] = [];
  var videoSet = {};
  videoSet["type"] = "h.264";

  if ($('#video_size').val() == -1) {
    videoSet["label"] = $("#video_size_width").val() + "x" + $("#video_size_height").val();
    videoSet["size"] = [parseInt($("#video_size_width").val()), parseInt($("#video_size_height").val())];
  } else {
    videoSet["label"] = $('#video_size :selected').text();
    videoSet["size"] = $('#video_size').val().split(",").map(Number);
  }
  // We need to do absolute value because the slider is setup to depict quality, which increases left to right,
  // whereas compression decreses going from left to right.
  videoSet["compression"] = Math.abs($("#compression").labeledslider("value"));
  videoSet["fps"] = $("#fps").val();

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
    projectName = tmpPath.substring(startIndex + 1, endIndex);
    projectPath = tmpPath.substring(0, endIndex);
    var tmcPath = projectPath.substring(0, startIndex) + "/" + projectName + "/" + projectName + ".tmc";

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

  if (!definitionPath) {
    saveAs();
    return;
  } else if (!api.writeFile(definitionPath, JSON.stringify(outputData, undefined, 2))) {
    alert('Error creating ' + definitionPath);
    return;
  }
}

function newProject() {
  if (projectModified) {
    var dialogConfirm = $("#dialog-confirm");
    dialogConfirm.data('recentProjectPath', null);
    dialogConfirm.data('clearProject', true);
    dialogConfirm.dialog("open");
    $('.ui-dialog :button').blur();
  } else {
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
  doFileHover = false;

  // Bring up organize image tab, in the event we are on another tab
  $("#tabs").tabs("option", "active", 0);

  $("#numFrames").text("0 Frames");
  $('#canvas_times').hide();
  $('#scroll').css({'overflow-x':'hidden', 'overflow-y': 'auto'});

  api.setUndoMenu(false);
  api.setRedoMenu(false);
  saveAction(false);

  reactivateUI();

  rescale();
  refresh();
}

function openData(startDirectory) {
  var file;


  // Bring up organize image tab
  $("#tabs").tabs("option", "active", 0);

  if (typeof startDirectory == 'undefined') file = api.readFileDialog('Open Time Machine Definition', '', '*.tmc');
  else file = api.openProjectFile(startDirectory);

  if (!file) return;
  var tmd = JSON.parse(file);
  if (!tmd) return;

  api.showWaitCursor();

  $("#btnToggle").button("enable");

  if (filmstripMode) {
    $("#filmstripControls").show();
    $('#scroll').css({'overflow':'auto'});
  } else {
    $('#canvas_times').show();
    $('#scroll').css({'overflow':'auto'});
  }

  // Load data
  data = [];
  sortBy = tmd['sort_by'] || "timestamp";
  if (sortBy == "filename") {
    $canvasTimes = $("#canvas_times").detach();
    leftMargin = 200;
  } else {
    if ($("#canvas_times").length == 0)
      $("#scroll").prepend($canvasTimes)
    leftMargin = 304;
  }
  for (var c = 0; c < tmd["source"]["images"].length; c++) {
    //console.log(c);
    var col = [];
    for (var r = 0; r < tmd["source"]["images"][c].length; r++) {
      var h = {};
      h['filename_with_path'] = tmd["source"]["images"][c][r];
      h['filename'] = h['filename_with_path'].replace(/^.*[\\\/]/, '');
      if (tmd["source"]["capture_times"] == undefined) {
        h['time'] = api.exifTime(h['filename_with_path']);
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

  // Load video settings
  // We need to negate the value because the slider is setup to depict quality, which increases left to right,
  // whereas compression decreses going from left to right. So all the values need to be negative.
  $("#compression").labeledslider("value", -1 * tmd["videosets"][0]["compression"]);
  $("#fps").val(tmd["videosets"][0]["fps"]);

  var foundVideoSize = false;
  $("select#video_size option").each(function() {
    if ($(this).text() == tmd["videosets"][0]["label"]) {
      foundVideoSize = true;
      return false;
    }
  });

  if (isInt(tmd["videosets"][0]["size"][0])) {
    if (foundVideoSize) {
      $('select#video_size').selectmenu("value", tmd["videosets"][0]["size"][0] + "," + tmd["videosets"][0]["size"][1]);
    } else {
      $('select#video_size').selectmenu("value", "-1");
      $('#video_size_width').val(tmd["videosets"][0]["size"][0]);
      $('#video_size_height').val(tmd["videosets"][0]["size"][1]);
    }
  }

  updateActiveData();
  rescale();
  refresh();

  // TODO:
  // Assumes a single camera
  $("#numFrames").text(data[0].length + " Frames");

  definitionPath = api.getOpenedProjectPath();

  var endIndex = definitionPath.lastIndexOf(".tmc/");
  var tmpPath = definitionPath.substring(0, endIndex);
  endIndex = tmpPath.lastIndexOf("/");
  projectPath = tmpPath.substring(0, endIndex);
  var startIndex = projectPath.lastIndexOf("/");
  projectName = projectPath.substring(startIndex + 1);
  projectModified = false;

  // Display the finished message and view link if the time machine is complete.
  if (api.fileExists(projectPath + "/" + projectName + ".timemachine/COMPLETE")) {
    var status_window = $("#status_window");
    $("#render_export_btn").hide();
    $("#status_text").remove();
    status_window.html("");
    status_window.append("<p><font color='green'>Time Machine completed.</font></p>");
    status_window.append("<br/><button id='view_timemachine'><b>View Time Machine</b></button><br/>");
    status_window.append("<br/><button id='change_settings_btn'>Change Settings and Recreate</button><br/>");
    $("#change_settings_btn").button().click(function () {
      reactivateUI();
    });
    $("#view_timemachine").button().click(function () {
      openInBrowser(projectPath + "/" + projectName + ".timemachine/view.html");
    });
  } else {
    $("#render_export_btn").show();
    $("#status_window").html("");
  }
  api.hideWaitCursor();
}

function undoAction() {
  var itemsToUndo = undoArray.pop();
  redoArray.push(itemsToUndo);
  for (var i = 0; i < itemsToUndo.length; i++) {
    data[itemsToUndo[i].col][itemsToUndo[i].row].deleted = false;
  }
  // Enable the redo menu
  api.setRedoMenu(true);
  // Disables the undo menu if we have nothing else to undo
  if (undoArray.length == 0) api.setUndoMenu(false);
  updateActiveData();
  // TODO:
  // Single camera assumption
  $("#numFrames").text(activeData["images"][0].length + " Frames");
  rescale();
  refresh();
}

function redoAction() {
  var itemsToRedo = redoArray.pop();
  undoArray.push(itemsToRedo);
  for (var i = 0; i < itemsToRedo.length; i++) {
    data[itemsToRedo[i].col][itemsToRedo[i].row].deleted = true;
  }
  // Enables the undo menu
  api.setUndoMenu(true);
  // Disables the undo menu if we have nothing else to undo
  if (redoArray.length == 0) api.setRedoMenu(false);
  updateActiveData();
  // TODO:
  // Single camera assumption
  $("#numFrames").text(activeData["images"][0].length + " Frames");
  rescale();
  refresh();
}

// Called by the add images/directories menu
function imagesDropped() {
  addDroppedFiles();
}

// Enable or disable open project menu
function openProjectAction(state) {
  api.setOpenProjectMenu(state);
}

// Enable or disable save menu
function saveAction(state) {
  api.setSaveMenu(state);
}

// Enable or disable saveAs menu
function saveAsAction(state) {
  api.setSaveAsMenu(state);
}

// Enable or disable add images menu
function addImagesAction(state) {
  api.setAddImagesMenu(state);
}

// Enable or disable add folder menu
function addFoldersAction(state) {
  api.setAddFoldersMenu(state);
}

// Enable or disable recently added menu
function recentlyAddedAction(state) {
  api.setRecentlyAddedMenu(state);
}

// Enable or disable new project menu
function newProjectAction(state) {
  api.setNewProjectMenu(state);
}

// Called when the user tries to exit the app
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

// Open recent project
function openRecentProject(path) {
  if (projectModified) {
    $("#dialog-confirm").data('recentProjectPath', path).dialog("open");
    $('.ui-dialog :button').blur();
  } else {
    openData(path);
  }
}

// Open url in a new browser
function openInBrowser(url) {
  api.openBrowser(url);
}

function getTime(pct) {
  return min + (scrollDiv.scrollTop + canvas.height * pct) / scale;
}

function setTime(ctr) {
  scrollDiv.scrollTop = scale * (ctr - min) - canvas.height * 0.5;
}

function format2(n) {
  var ret = "" + n;
  return (ret.length == 1) ? "0" + ret : ret;
}

function isNumber(n) {
  return !isNaN(parseFloat(n)) && isFinite(n);
}

function jumpTo() {
  var jumpToFrameVal = $("#jumpToFrame").val();
  if (isNumber(jumpToFrameVal)) filmstripIndex = jumpToFrameVal - 1;
}

// Finds the index of the closest element in column b to element a in column 0 of data
// For multi camera support
function findClosest(a, b) {
  var dummyOffset = data[b].offset; // we must have one for each column 1-end
  var l = 0;
  var r = activeData['capture_times'][b].length - 1;

  while (r - l > 1) {
    var m = Math.floor((r + l) / 2);
    if (activeData['capture_times'][b][m] + dummyOffset < activeData['capture_times'][0][a]) l = m;
    else r = m;
  }

  if (Math.abs(activeData['capture_times'][b][l] + dummyOffset - activeData['capture_times'][0][a]) < Math.abs(activeData['capture_times'][b][r] + dummyOffset - activeData['capture_times'][0][a])) return l;
  return r;
}

// For multi camera support
// Unused
function computeDeltas() {
  // Deltas in first column are from previous image
  var col = data[0];
  for (var r = 1; r < col.length; r++) {
    col[r].delta = Math.round((col[r].time - col[r - 1].time) * 100) / 100;
  }
  // Deltas in other colums are relative to first column
  for (var c = 1; c < data.length; c++) {
    col = data[c];
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

      var delta = Math.abs(col[r].delta);
    }
  }
}

function updateActiveData() {
  activeData = {};
  activeData["images"] = [];
  activeData["capture_times"] = [];

  for (var i = 0; i < data.length; i++) {
    activeData["images"].push([]);
    activeData["capture_times"].push([]);
    for (var j = 0; j < data[i].length; j++) {
      if (data[i][j].deleted == false) {
        activeData["images"][i].push(data[i][j].filename_with_path);
        activeData["capture_times"][i].push(data[i][j].time);
      }
    }
  }
  getMinMaxTime();
}

// Rotate and draw an image
function drawObjectRot(img, x, y, width, height, deg){
  // Convert degrees to radian
  var rad = deg * Math.PI / 180;

  // Set the origin to the center of the image
  context.translate(x + width / 2, y + height / 2);

  // Rotate the canvas around the origin
  context.rotate(rad);

  // Draw the object
  if (img == "strokeRect") {
    context.strokeRect(width / 2 * (-1), height / 2 * (-1), width, height);
  } else if (img == null) {
    context.fillRect(width / 2 * (-1), height / 2 * (-1), width, height);
  } else {
    context.drawImage(img, width / 2 * (-1), height / 2 * (-1), width, height);
  }
  // Reset the canvas
  context.rotate(rad * ( -1 ) );
  context.translate((x + width / 2) * (-1), (y + height / 2) * (-1));
}

function refresh() {
  //console.log('refresh');
  if (activeData.length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) {
    canvas.width = canvas.width;
    context.font = '16px sans-serif';
    context.fillText("No images loaded.", (canvas.width / 2 - 135), 100);
    $("#btnToggle").button("disable");
    $("#canvas_times").hide();
    return;
  } else if (filmstripMode) {
    // TODO:
    // Single camera assumption in filmstrip mode
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

    $("#jumpToFrame").val(filmstripIndex + 1);

    $("#filmstripText").text(activeData["images"][0].length);
    var prepend = (navigator.appVersion.indexOf("Win") != -1) ? "file:///" : "file://";

    var imageObj = new Image();

    imageObj.src = prepend + activeData["images"][0][filmstripIndex];

    // Max width for the image
    var maxWidth = canvas.width - 40;
    // Max height for the image
    var maxHeight = canvas.height - 140;
    // Used for aspect ratio
    var ratio = 0;

    imageObj.onload = function() {
      // Current image width
      var Awidth = this.width;
      // Current image height
      var Aheight = this.height;

      // Check if the current width is larger than the max
      if (Awidth > maxWidth) {
        // Get ratio for scaling image
        ratio = maxWidth / Awidth;
        // Reset height to match scaled image
        Aheight = Aheight * ratio;
        // Reset width to match scaled image
        Awidth = Awidth * ratio;
      }

      // Check if current height is larger than max
      if (Aheight > maxHeight) {
        // Get ratio for scaling image
        ratio = maxHeight / Aheight;
        // Reset height to match scaled image
        Aheight = Aheight * ratio;
        // Reset width to match scaled image
        Awidth = Awidth * ratio;
      }

      var width = Awidth;
      var height = Aheight;
      var x = 40;
      var y = 55;

      context.clearRect(0, 0, canvas.width, canvas.height);
      context.drawImage(imageObj, x, y, width, height);
      context.fillText(activeData["images"][0][filmstripIndex], x, height + y + 8);
      var date = new Date(1000 * activeData["capture_times"][0][filmstripIndex]);
      var text = date.toLocaleDateString() + " " + date.toLocaleTimeString();
      context.fillText(text, x, y - 2);
    };
  } else {
    mintime = getTime(-1);
    var toptime = getTime(0);
    maxtime = getTime(1);

    canvas.width = canvas.width;
    canvas_times.width = canvas_times.width;
    //context.clearRect(0, 0, canvas.width, canvas.height);
    //context_times.clearRect(0, 0, canvas.width, canvas.height);

    context.font = '9px sans-serif';
    context_times.font = '9px sans-serif';

    var loading = false;

    if (sortBy == "timestamp") {
      if (activeData["images"].length > 0 && activeData["images"][0].length > 0) {
        for (var i = Math.floor(mintime / ticktime); i * ticktime <= maxtime; i++) {
          var time = i * ticktime;
          var y = scale * (time - toptime);
          var date = new Date(time * 1000);
          var label = date.getFullYear() + "-" + format2(date.getMonth() + 1) + "-" + format2(date.getDay()) + " " + format2(date.getHours()) + ":" + format2(date.getMinutes()) + ":" + format2(date.getSeconds()) + "." + Math.floor(date.getMilliseconds() / 100);
          context_times.fillText(label, 0, y);
        }
      }
    }
    var left_margin = 120;
    for (var c = 0; c < data.length; c++) {
      var col = data[c];
      var x = canvas.width/2 - 80;
      var y;
      for (var r = 0; r < col.length; r++) {
        var elt = col[r];
        if (elt.deleted) continue;
        // debugggg
        if (elt.time >= mintime && elt.time <= maxtime) {
          if (view == "thumbnails") {
            y = scale * (elt.time - toptime);

            if (!elt.image) {
              var thumbnail;
              if ((thumbnail = api.readThumbnail(elt.filename_with_path)) == null) {
                //drawObjectRot(null, x, y, 160, 120, rotInDegrees);
                context.fillRect(x, y, 160, 120);
              } else {
                var img = new Image();
                try {
                  thumbnail.assignToHTMLImageElement(img);
                } catch(e) {
                  thumbnail.assignTo(img);
                }
                elt.image = img;
              }
            }
            elt.w = 160;
            elt.h = 120;
            elt.x = x;
            elt.y = y;
            //context.save();
            //context.transform(0, -1, 1, 0, (x + 80) - (y + 80), (x + 80) + (y + 80));
            //console.log(elt.image);

            if (elt.image && !elt.image.complete) loading = true;
            if (elt.image && elt.image.complete && elt.image.width > 0) {
              //console.log("drawing image");
              //console.log(elt.image);
              context.drawImage(elt.image, x, y);
              //drawObjectRot(elt.image, x, y, elt.w, elt.h, rotInDegrees);
              if (jQuery.inArray(elt, selectedImages) >= 0) {
                //console.log(selectedImages);
                context.strokeStyle = '#CC0000';
                context.lineWidth = 2;
                //drawObjectRot("strokeRect", x, y, elt.w, elt.h, rotInDegrees);
                context.strokeRect(elt.x, elt.y, 160, 120);
              }
            }
          } else {
            //console.log("drawing lines");
            context.fillRect(x + 60, y, 25, 1);
          }
        }
      }
    }

    if (loading) setTimeout(refresh, 100);
  }

}

function getMinMaxTime() {
  min = 1e+100;
  max = -1e+100;
  if (data.length > 0) {
    for (var i = 0; i < data.length; i++) {
      if (activeData['capture_times'][i][0] < min) min = activeData['capture_times'][i][0];
      if (activeData['capture_times'][i][activeData['capture_times'][i].length - 1] > max) max = activeData['capture_times'][i][activeData['capture_times'][i].length - 1];
    }
  }
}

function rescale() {
  var tabsHeight = $("#tabs").height();

  if (contentDiv.style.overflow == "hidden") {
    scrollDiv.style.height = (tabsHeight - 134) + "px";
  } else {
    scrollDiv.style.height = (tabsHeight - 148) + "px";
  }
  scrollDiv.style.width = (dropBoxes.innerWidth() + 2) + "px";

  if (contentDiv.style.overflow == "hidden" && dropBoxes.innerWidth() > $(contentDiv).width()) {
    contentDiv.style.overflow = "auto";
    scrollDiv.style.height = parseInt(scrollDiv.style.height) - 2 + "px";
  } else if (contentDiv.style.overflow == "auto" && dropBoxes.innerWidth() == $(contentDiv).width()) {
    contentDiv.style.overflow = "hidden";
    scrollDiv.style.height = parseInt(scrollDiv.style.height) + 2 + "px";
  }

  $("#controls").css("height", (tabsHeight - 56) + "px");

  if (filmstripMode) {
    canvasDiv.style.height = scrollDiv.style.height;
    canvasDiv.style.width = scrollDiv.style.width;
    canvasDiv.style.left = "200px";

    canvas.width = parseInt(canvasDiv.style.width);
    canvas.height = parseInt(canvasDiv.style.height);

    return;
  }

  canvasDiv.style.height = scrollDiv.style.height;
  // 14 is the width of the scrollbar.
  canvasDiv.style.width = parseInt(scrollDiv.style.width) - (14 + (114 * (sortBy == "timestamp"))) + "px";
  canvasDiv.style.left = leftMargin + "px";

  canvas.width = parseInt(canvasDiv.style.width);
  canvas.height = parseInt(canvasDiv.style.height);

  canvas_times.height = parseInt(canvasDiv.style.height);

  // Magic number = height of a thumbnail (160) + height of filename_with_path text (8) + extra padding (32)
  $("#world").css("height", (max - min) * scale + 200);

  var ticktimes = [0.1, 0.2, 0.5, 1, 2, 5, 10, 30, 60, 60 * 2, 60 * 5, 60 * 15, 60 * 30, 3600, 2 * 3600, 4 * 3600, 12 * 3600, 86400];
  // We want to display time axis labels no closer than every N pixels
  var tickPixels = 100;
  ticktime = tickPixels / scale;
  for (var i = 0; i < ticktimes.length; i++) {
    if (ticktimes[i] >= ticktime) {
      ticktime = ticktimes[i];
      break;
    }
  }

  var msg = "";
  if (ticktime < 1) {
    var time = Math.round(ticktime * 1000);
    msg += (time > 1) ? time + " milliseconds" : time +  " millisecond";
  } else if (ticktime < 60) {
    msg += (ticktime > 1) ? ticktime + " seconds" : ticktime +  " second";
  } else if (ticktime < 3600) {
    var time = Math.round(ticktime / 60 * 100) / 100;
    msg += (time > 1) ? time + " minutes" : time +  " minute";
  } else {
    var time = Math.round(ticktime / 3600 * 100) / 100;
    msg += (time > 1) ? time + " hours" : time +  " hour";
  }
  $("#scale").html(msg);

  if (scale < 64) view = "lines";
  else view = "thumbnails";
  dropbox_width = parseInt($('#dropboxes td').eq(0).css("width"));
  half_dropbox_width = dropbox_width/2;
}

// Unused. Was previously used to descrease spacing between images and
// thus eventually display lines if things got to close together.
function zoom(factor) {
  // TODO:
  // Some bug here when decreasing the spacing

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
    // Numpad delete?
    case 127:
    // Backsapce key
    case 8:
    // Delete key
    case 46:
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
  for (var i = 0; i < selectedImages.length; i++) {
    // Mark as deleted, but do not actually delete. Useful for undo feature
    data[selectedImages[i].col][selectedImages[i].row].deleted = true;
  }
  undoArray.push([]);
  jQuery.extend(true, undoArray[undoArray.length - 1], selectedImages);
  if (undoArray.length > 0) api.setUndoMenu(true);
  selectedImages.length = 0;

  updateActiveData();
  // TODO:
  // Single camera assumption
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

function setRotationAmount(amt) {
  if (amt == undefined)
    rotInDegrees += 90;
  else
    rotInDegrees = amt;

  refresh();
}

function startRender() {
  inProgress = true;

  // Call save dialog window if the current project has not been saved
  if (!definitionPath) saveAs();
  else if (projectModified) save();

  if (definitionPath == "") {
    inProgress = false;
    return;
  }

  $("#tabs").tabs("disable", "tabs-1");
  $("#compression").labeledslider("disable");
  $("#fps").button("disable");
  $("#video_size").selectmenu("disable");
  $("#num_jobs").button("disable");
  $("#video_size_width").prop('disabled', true);
  $("#video_size_height").prop('disabled', true);

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
  var status_window = $("#status_window");
  status_window.html("");

  if ($("#status_text").length > 0)
    $("#status_text").html("<p id='status_text'>In progress...</p>");
  else
    status_window.append("<p id='status_text'>In progress...</p>");
  status_window.append("Destination: " + projectPath + "/" + projectName + ".timemachine" + "<br/>");
  status_window.append("Started: " + get_current_time_formatted() + "<br/>");
  status_window.append("Progress: <span id='current_progress'>0.0%</span>");
  status_window.append("<br/><button id='pause_render_btn'>Pause</button>");
  status_window.append("<button id='cancel_render_btn'>Cancel</button>");

  $("#cancel_render_btn").button().click(function () {
    renderPaused = false;
    api.killSubprocess();

  });

  $("#pause_render_btn").button().click(function () {
    renderPaused = true;
    api.killSubprocess();
  });

  $("#current_progress").progressBar({ barImage: 'assets/images/progressbg_black.gif'} );
  run_ct();
}

function init() {
  //console.log("loaded");

  canvas = document.getElementById("canvas");
  context = canvas.getContext("2d");
  context.fillStyle = 'black';
  context.textBaseline = 'top';

  canvas_times = document.getElementById('canvas_times');
  context_times = canvas_times.getContext('2d');
  context_times.fillStyle = 'black';
  context_times.textBaseline = 'top';

  scrollDiv = document.getElementById('scroll');
  canvasDiv = document.getElementById('canvasdiv');
  contentDiv = document.getElementById('content');

  dropBoxes = $("#dropboxes");

  $("#numFrames").text("0 Frames");
  $('#canvas_times').hide();

  api.setDeleteMenu(false);
  api.setUndoMenu(false);
  api.setRedoMenu(false);
  saveAction(false);

  $("#btnToggle").button({ disabled: true });
  $("#tmFormat").button();

  $("#render_export_btn").button().click(function () {
    startRender();
  });

  // TODO:
  // Sometimes dropping on the edge of the dropbox results in the picture opening in the whole window
  dropBoxes.on('dragenter', ".dropbox", function(evt) {
    if (evt.target === this) addDropHighlight(evt);
  }).on('drop', ".dropbox", function(evt) {
    dropHandler(evt);
  }).on('dragleave', ".dropbox", function(evt) {
    var rect = this.getBoundingClientRect();
    if (evt.originalEvent.clientX >= rect.left + rect.width || evt.originalEvent.clientX <= rect.left
       || evt.originalEvent.clientY >= rect.top + rect.height || evt.originalEvent.clientY <= rect.top) {
      removeDropHighlight(evt);
    }
  });

  $(document).bind('keydown', function(e){
    if (filmstripMode) {
      if (e.which==37 || e.which==39) {
        if ($(e.target).is('input')) { return; }
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
    if (event.keyCode == 13){
      jumpTo();
      refresh();
    }
  });

  $('.ui-state-default').hover(function () {
      $(this).addClass('ui-state-hover');
    }, function () {
      $(this).removeClass('ui-state-hover');
  });

  $('#compression').labeledslider({
    min: -30,
    max: -24,
    step: 2,
    tickInterval: 1,
    value: -26,
    tickArray: [-30, -28, -26, -24],
    tickLabels: {
      '-30': 'Low',
      '-28': 'Med',
      '-26': 'High',
      '-24': 'V. High'
    },
    change: function() {
      projectModified = true;
      saveAction(true);
    }
  });

  $("#change-format").dialog({
    autoOpen: false,
    modal: true,
    width: 550,
    resizable: false,
    draggable: false,
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
    draggable: false,
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
    draggable: false,
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
    draggable: false,
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
    draggable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });

  $("#video_size-dialog-message").dialog({
    autoOpen: false,
    modal: true,
    width: 430,
    resizable: false,
    draggable: false,
    buttons: {
      Ok: function () {
        $(this).dialog("close");
      }
    }
  });

  $("#dialog-confirm").dialog({
    autoOpen: false,
    resizable: false,
    draggable: false,
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

  $('input:text').button().css({
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

  $("#video_size_explanation").click(function () {
    $("#video_size-dialog-message").dialog("open");
  });

  $('#fps').button().change(function(){
    projectModified = true;
    saveAction(true);
  });

  $('#video_size_width, #video_size_height').change(function(event, manualTrigger){
      projectModified = true;
      if (!isInt(this.value)) {
        if (!manualTrigger) {
          alert("Please enter a valid integer");
        }
        $("#render_export_btn").button("disable");
      } else {
        if (isInt($("#video_size_width").val()) && isInt($("#video_size_height").val())) {
          $("#render_export_btn").button("enable");
          checkData();
        } else {
          $("#render_export_btn").button("disable");
        }
      }
  });

  $('#video_size').selectmenu({
    change: function(e, object){
       projectModified = true;
      if (object.value == -1) {
        $("#video_size_custom").show();
        $("#job_setting").css("margin-top", "0px");
        $("#render_export_btn").button("disable");
      } else {
        $("#video_size_width").val("");
        $("#video_size_height").val("");
        $("#video_size_custom").hide();
        $("#job_setting").css("margin-top", "-30px");
      }
    }
  });

  $("body").keypress(keypress);

  var mouseSelection = false;
  var originalPos;
  var originalCanvasPos;

  $('#world').mousedown(function (e) {
    if (data.length == 0 || data[0].length == 0 || filmstripMode) return;

    mouseSelection = true;
    originalPos = [e.pageX, e.pageY];
    originalCanvasPos = canvas.relMouseCoords(e);
    worldWidthAndExtra = parseInt($("#world").css("width")) + 200;
    scrollHeightAndExtra = parseInt(scrollDiv.style.height) + 150;

    var coords = originalCanvasPos;
    var itemInBounds = false;

    clickedCol = getDataCol(coords.x);

    for (var i = 0; i < data[clickedCol].length; i++) {
      // debugggg
      if ((data[clickedCol][i].deleted) || !(data[clickedCol][i].time >= mintime && data[clickedCol][i].time <= maxtime)) continue;

      if (inBounds(data[clickedCol][i], coords.x, coords.y)) {
        api.setDeleteMenu(true);
        if (!(e.ctrlKey || e.metaKey) && !e.shiftKey) selectedImages.length = 0;

        if (e.shiftKey) {

          if (selectedImages.length == 0) {
            for (var j = 0; j <= i; j++) {
              selectedImages.push(data[clickedCol][j]);
            }
          } else {
            var oneJustClicked = data[clickedCol][i];
            var lastOneClicked = selectedImages[selectedImages.length - 1];
            var lastOneClickedIndex = lastOneClicked.row;
            var firstOneClicked = selectedImages[0];
            var firstOneClickedIndex = firstOneClicked.row;

            if (shiftAlreadyClicked && (oneJustClicked.filename_with_path == lastOneClicked.filename_with_path)) break;

            selectedImages.length = 0;

            var startIndex = shiftAlreadyClicked ? firstOneClickedIndex : lastOneClickedIndex;

            if (i > lastOneClickedIndex) {
              //console.log("going down");
              for (var a = 0; a < data.length; a++) {
                for (var j = startIndex; j <= i; j++) {
                  // If we are shifting in the same column, do something different so that we can select multiple files in only that column
                  // This is different than normal shift behavior in a directory, but it makes sense to do it this way here.
                  if ((oneJustClicked.col == lastOneClicked.col && a != oneJustClicked.col) || ((a < lastOneClicked.col && j == lastOneClicked.row) || (a > oneJustClicked.col && j == oneJustClicked.row))) continue;
                  selectedImages.push(data[a][j]);
                }
              }
            } else {
              //console.log("going up");
              for (var a = 0; a < data.length; a++) {
                for (var j = startIndex; j >= i; j--) {
                  // If we are shifting in the same column, do something different so that we can select multiple files in only that column
                  // This is different than normal shift behavior in a directory, but it makes sense to do it this way here.
                  if ((oneJustClicked.col == lastOneClicked.col && a != oneJustClicked.col) || ((a > lastOneClicked.col && j == lastOneClicked.row) || (a < oneJustClicked.col && j == oneJustClicked.row))) continue;
                  selectedImages.push(data[a][j]);
                }
              }
            }
          }
          shiftAlreadyClicked = true;
        } else {
          shiftAlreadyClicked = false;
          if ($.inArray(data[clickedCol][i], selectedImages) < 0) selectedImages.push(data[clickedCol][i]);
          else selectedImages.splice($.inArray(data[clickedCol][i], selectedImages), 1);
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

  // Hackity hack - for some reason the top left corner is triggering the scroll div's mouse events
  // so we only do a hover when we actually are in the right area.
  // Honestly, I don't think this truly prevents the issue from occuring...
  $('#tabs, #controls').mouseleave(function () {
    //if (e.fromElement.id == "world") return; // entering the file_hover element counts as a leaving the world, so ignore this
    doFileHover = false;
  });

  $('#top_area, #canvas_times, #tabs').mouseenter(function (e) {
    //if (e.relatedTarget && e.relatedTarget.id == "tabs") return;
    if (((e.pageY - window.pageYOffset) < 10) || ((e.pageX - window.pageXOffset) < 10)) return;
    doFileHover = true;
  });

  $('#scroll').mousemove(function (e) {
    if (data.length == 0 || mouseSelection || filmstripMode || view == "lines" || doFileHover == false || document.hasFocus() == false) return;

    var leftScroll = parseInt(contentDiv.scrollLeft);
    var mouse_x = e.pageX - leftMargin + leftScroll;
    var mouse_y = e.pageY - 151;
    var found = false;

    var col = getDataCol(mouse_x);

    for (var i = 0; i < data[col].length; i++) {
      // debuggg
      if ((data[col][i].deleted) || !(data[col][i].time >= mintime && data[col][i].time <= maxtime)) continue;

      if ( mouse_x >= data[col][i].x && mouse_x <= data[col][i].x + data[col][i].w
      &&   mouse_y >= data[col][i].y && mouse_y <= data[col][i].y + data[col][i].h ) {
        if ($("#file_hover").length == 0) {
          var div = $('<div id="file_hover">' + data[col][i].filename_with_path + '</div>');
          div.css({
            'left': data[col][i].x + leftMargin - leftScroll,
            'top': data[col][i].y + 271
          });
          $('body').append(div);
        }
        found = true;
        document.body.style.cursor = 'pointer';
        break;
      }
    }
    if (!found) {
      $("#file_hover").remove();
      document.body.style.cursor = 'default';
    }
  }).mouseleave(function () {
    $("#file_hover").remove();
    document.body.style.cursor = 'default';
  });

  $('body').mousemove(function (e) {
    if (mouseSelection) {
      if ($('#selection_area').length == 0) {
        var div = $('<div id="selection_area"></div>');
        div.css({
          'left': e.pageX,
          'top': e.pageY,
          'width': 0,
          'height': 0
        });
        $('body').append(div);
      }

      var x1 = originalPos[0], y1 = originalPos[1], x2 = event.pageX, y2 = event.pageY;

      // Keep selection box within bounds
      // Margin-right of 200px + 2px of border + 100px of canvas_times
      if (x2 <= leftMargin) x2 = 302;
      // top_area + tabs = 150px + 2px of border
      if (y2 <= 153) y2 = 152;
      if (x2 > worldWidthAndExtra) x2 = worldWidthAndExtra;
      if (y2 > scrollHeightAndExtra) y2 = scrollHeightAndExtra;

      var coords = canvas.relMouseCoords(e);
      var origCanvasX = originalCanvasPos.x, origCanvasY = originalCanvasPos.y;
      var width = (x2 - x1), height = (y2 - y1);

      var tmpX1 = x1;
      var tmpX2 = x2;
      var tmp;

      if (x1 > x2) {
        tmp = x2;
        x2 = x1;
        x1 = tmp;
        width = (x2 - x1);
        origCanvasX -= width;
        coords.x += width;
      }

      if (y1 > y2) {
        tmp = y2;
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

      var col = clickedCol;
      //console.log("origCanvasX: " + origCanvasX + "coords.x: " + coords.x);

      while (col >= 0 && col < data.length) {

        for (var i = 0; i < data[col].length; i++) {

          // There is something wrong with my collision code or something else is at play.
          // Either way, checking whether the image is currently visible seems to fix this.
          // debugggg
          if ((data[col][i].deleted) || !(data[col][i].time >= mintime && data[col][i].time <= maxtime)) continue;

          // Determine whether the selection rectangle intersects one of our data objects (also a rectangle)
          if (!(origCanvasX > data[col][i].x + data[col][i].w || coords.x < data[col][i].x || origCanvasY > data[col][i].y + data[col][i].h || coords.y < data[col][i].y)) {
            //console.log("in rec");
            if ($.inArray(data[col][i], selectedImages) < 0) {
              //console.log("added to rec");
              selectedImages.push(data[col][i]);
              api.setDeleteMenu(true);
              refresh();
              break;
            }
          } else {
            //console.log("outside rec");
            //console.log(col);
            var idx = $.inArray(data[col][i], selectedImages);
            //console.log(idx);
            //console.log(selectedImages);
            if (idx >= 0) {
              selectedImages.splice(idx, 1);
              refresh();
            }
          }
        }

        if (tmpX1 > tmpX2) {
          col--;
        } else {
          col++;
        }
      }
    }
  });

  $(document).mouseup(function () {
    $('#selection_area').remove();
    mouseSelection = false;
  });

  $("#tabs").tabs({
    activate: function(event, ui){
      if ($('#tabs').tabs('option', 'active') == 0) {
        rescale();
        refresh();
      }
    }
  });

  setupSliderHandlers();
  rescale();
  refresh();
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

function getDataCol(xcoord) {
  var col = Math.floor((((dropbox_width + (left_margin / 2)) + xcoord) / (dropbox_width)) - 1);
  if (col < 0) col = 0;
  else if (col >= data.length) col--;
  return col;
}

function padNumber(number, length) {
  var str = '' + number;
  while (str.length < length) {
    str = '0' + str;
  }
  return str;
}

function run_ct() {
  //console.log(projectPath + "/"+projectName + ".timemachine/view.html");
  if (!api.invokeRubySubprocess([api.getRootAppPath() + '/ct/ct.rb', '-j', $("#num_jobs").val().toString(), definitionPath, projectPath + "/" + projectName + ".timemachine"], ct_out)) {
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

  $("#status_window").html("");
  $("#status_text").html("");
  $("#tabs").tabs("enable", "tabs-1");
  $("#compression").labeledslider("enable");
  $("#fps").button("enable");
  $("#video_size").selectmenu("enable");
  $("#num_jobs").button("enable");
  $("#video_size_width").prop('disabled', false);
  $("#video_size_height").prop('disabled', false);
  $("#render_export_btn").show();
}

function ct_out(out) {
  //console.log("In ct_out, out=" + out);
  if (typeof out === 'number') {
    inProgress = false;
    newProjectAction(true);
    $("#cancel_render_btn, #pause_render_btn").hide();
    $("#status_window").append("Ended: " + get_current_time_formatted() + "<br/>");
    // Linux and Mac report an error code of 0 when we kill ct.rb, so we check that we are actually done
    if (out == 0 && (api.fileExists(projectPath + "/" + projectName + ".timemachine/COMPLETE"))) { //success
      $("#status_window").append("<br/><button id='view_timemachine'><b>View Time Machine</b></button><br/>");
      $("#status_window").append("<br/><button id='change_settings_btn'>Change Settings and Recreate</button><br/>");
      $("#status_text").html("<font color='green'>Time Machine completed.</font>");
      $("#view_timemachine").button().click(function () {
        openInBrowser(projectPath + "/" + projectName + ".timemachine/view.html");
      });
      $("#change_settings_btn").button().click(function () {
        reactivateUI();
      });
    } else if (out == 1) { //error
      $("#status_text").html("<font color='red'>An error was encountered.</font>");
      $("#status_window").append("<br/><button id='retry_btn'>Retry</button><br/>");
      $("#retry_btn").button().click(function () {
        startRender();
      });
    } else { //canceled (62097?)
      if (renderPaused) {
        $("#status_text").html("<font color='blue'>Process was paused.</font>");
        $("#status_window").append("<br/><button id='retry_btn'>Resume</button><br/>");
        $("#retry_btn").button().click(function () {
          startRender();
        });
        $("#view_timemachine").button().click(function () {
          openInBrowser(projectPath + "/" + projectName + ".timemachine/view.html");
        });
      } else {
        $("#status_window").html("");

        reactivateUI();

        $("#render_export_btn").before("<p id='status_text'><font color='red'>Process was canceled.</font></p>");

        // TODO:
        // The user chose to cancel, so we should probably remove the files just made
      }

    }
  } else {
    var output_string = out.replace(/\r\n|\r|\n/g, "<br />");
    var output_tmp = output_string.match(/Rules \d+.\d+% rules finished/);
    if (output_tmp != null) {
      var output = output_tmp[0].match(/\d+.\d+%/);
      $("#current_progress").progressBar(parseFloat(output[0].slice(0, -1)));
    }
  }
}

function get_current_time_formatted() {
  var date = new Date();
  var time_formatted = date.getFullYear() + "-" + padNumber(date.getMonth() + 1, 2) + "-" + padNumber(date.getDate(), 2) + " " + padNumber(date.getHours(), 2) + ":" + padNumber(date.getMinutes(), 2) + ":" + padNumber(date.getSeconds(), 2);
  return time_formatted;
}

// Prevent browser from loading dropped files as if it is a new page
document.addEventListener("dragover", function(e) {
  e.preventDefault();
}, false);
document.addEventListener("drop", function(e) {
  e.preventDefault();
}, false);

function toggleFilmstrip() {
  if ($("#btnToggle").html() == 'Timeline View') {
    $("#btnToggle").html('Slideshow View');
    $("#btnToggle").attr('title', 'Click for slideshow view');
    filmstripMode = false;
    $("#filmstripControls").hide();
    $('#scroll').css({'overflow-x':'hidden', 'overflow-y': 'auto'});
    $("#canvas_times").show();
  } else {
    $('#scroll').css({'overflow':'hidden'});
    $("#btnToggle").html('Timeline View');
    $("#btnToggle").attr('title', 'Click for timeline view');
    filmstripMode = true;
    filmstripIndex = 0;
    if (activeData.length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) return;
    $("#filmstripControls").show();
    $("#canvas_times").hide();
  }
  rescale();
  refresh();
}

function addDropHighlight(evt) {
  evt.stopPropagation();
  evt.preventDefault();
  $(evt.target).css({
    "background": "Gainsboro"
  });
}

function removeDropHighlight(evt) {
  $(".dropbox").css({
    "background": "#f5f5f5"
  });
}

function addDroppedFiles(cameraNum) {
  api.showWaitCursor();

  projectModified = true;
  saveAction(true);
  var droppedFiles = api.droppedFilesRecursive();

  if (data.length == 0) data.push([]);

  if (cameraNum == null) cameraNum = 0;

  var rowNum = 0;
  var alertExifTimeMsg = "One or more images with missing, invalid, or duplicate exif time data was encountered. Most likely you are using an image that you stitched yourself or created in a program such as Photoshop." + '\n\n' + "Rather than sorting images by timstamps, they will instead be sorted alphabetically by filename."  + '\n';
  for (var i = 0; i < droppedFiles.length; i++) {
    if (droppedFiles[i].match(/\.jpg|.jpeg$/i) == null) continue; // only accepts jpg files, though the software can process pngs too
    var h = {};
    h['filename_with_path'] = droppedFiles[i];
    h['filename'] = h['filename_with_path'].replace(/^.*[\\\/]/, '');
    h['time'] = api.exifTime(droppedFiles[i]);
    // If we are missing or have corrupted exif time data, then prompt the user that
    // this was encountered and we will sort by file names instead.
    if (!didTimestampWarning && h['time'] == -1) {
      sortBy = "filename";
      didTimestampWarning = true;
      alert(alertExifTimeMsg);
    }
    h['row'] = rowNum++;
    h['col'] = cameraNum;
    h['deleted'] = false;
    data[cameraNum].push(h);
  }

  // Remove duplicate files
  for(var i = 0; i < data[cameraNum].length; i++) {
    data[cameraNum][i].row = i;
    if (!didTimestampWarning && i > 0 && data[cameraNum][i].time == data[cameraNum][i-1].time) {
      sortBy = "filename";
      didTimestampWarning = true;
      alert(alertExifTimeMsg);
    } else if (i > 0 && data[cameraNum][i].filename_with_path == data[cameraNum][i-1].filename_with_path) {
      data[cameraNum].splice(i, 1);
      i--;
    }
  }

  // TODO:
  // What if exifdata is bad (returns -1)
  // What if timestamps are the same but the files are different (custom made pics)

  data[cameraNum].sort(function(a, b){
    if (sortBy == "filename")
      return a.filename.localeCompare(b.filename);
    else
      return a.time - b.time;
  });

  if (sortBy == "filename") {
    $canvasTimes = $("#canvas_times").detach();
    leftMargin = 200;
    for(var i = 0; i < data[cameraNum].length; i++) {
      data[cameraNum][i].time = (i * 2.5);
    }
  }
  // TODO:
  // Assumes single camera
  $("#numFrames").text(data[cameraNum].length + " Frames");

  if (data[cameraNum].length > 0) {
    $("#btnToggle").button("enable");
    if (!filmstripMode && sortBy != "filename") {
      $('#canvas_times, #threshold_div').show();
    }
    //computeDeltas();
    updateActiveData();
    rescale();
    refresh();
  }
  api.hideWaitCursor();
}

function dropHandler(evt) {
  evt.stopPropagation();
  evt.preventDefault();
  var dropboxId = $(evt.target).parent(".dropbox").length == 0 ? evt.target.id : $(evt.target).parent(".dropbox").get(0).id;
  var cameraNum = parseInt(dropboxId.split("_")[1]) - 1;
  removeDropHighlight();
  addDroppedFiles(cameraNum);
}

function checkData() {
  if (data.length == 0 || data[0].length == 0 || activeData["images"].length == 0 || activeData["images"][0].length == 0) $("#render_export_btn").button("disable");
  else $("#render_export_btn").button("enable");
}

function isInt(value) {
  return (Math.floor(value) == value && $.isNumeric(value));
}

function isPositive(elem) {
  if ((elem.value) <= 0 || !isNumber(elem.value)) $(elem).val(2);
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
  } while (currentElement = currentElement.offsetParent);

  canvasX = event.pageX - totalOffsetX;
  canvasY = event.pageY - totalOffsetY;

  // Fix for variable canvas width
  canvasX = Math.round(canvasX * (this.width / this.offsetWidth));
  canvasY = Math.round(canvasY * (this.height / this.offsetHeight));

  return {
    x: canvasX,
    y: canvasY
  };
};
