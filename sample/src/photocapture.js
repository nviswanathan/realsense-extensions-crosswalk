var videoElement = document.getElementById('preview');
var depthCanvas = document.getElementById('depth');
depthCanvas.style.display = 'none';
var depthContext = depthCanvas.getContext('2d');
var cameraLabel = document.getElementById('camera');
var resolutionSelect = document.getElementById('resolution');
var takePhotoButton = document.getElementById('takephoto');
var startButton = document.getElementById('start');
var stopButton = document.getElementById('stop');
var statusElement = document.getElementById('status');
var qualityElement = document.getElementById('quality');
resolutionSelect.value = 'fhd';
var depthMapCheckBox = document.getElementById('depthmap');
var viewerDiv = document.getElementById('viewer');
var depthResolution = document.getElementById('depthResolution');

var eventsFPS = new Stats();
eventsFPS.domElement.style.position = 'absolute';
eventsFPS.domElement.style.top = '0px';
eventsFPS.domElement.style.left = '0px';
document.body.appendChild(eventsFPS.domElement);

const cameraName = 'Intel(R) RealSense(TM) 3D Camera R200';
var cameraId = '';

var previewStream = null;
var photoCapture = null;
var depthPhoto = null;

var constraintsMap = {
  'qvga': {width: 320, height: 240, fps: 60},
  'vga': {width: 640, height: 480, fps: 60},
  'hd': {width: 1280, height: 720, fps: 30},
  'fhd': {width: 1920, height: 1080, fps: 30},
};

var qualityMap = {
  'good': {text: 'GOOD', color: 'green'},
  'fair': {text: 'FAIR', color: 'orange'},
  'bad' : {text: 'BAD ', color: 'red'}
};

function gotDevices(deviceInfos) {
  for (var i = 0; i < deviceInfos.length; ++i) {
    var deviceInfo = deviceInfos[i];
    if (deviceInfo.kind === 'videoinput') {
      console.log(deviceInfo.label);
      if (deviceInfo.label === cameraName) {
        cameraId = deviceInfo.deviceId;
        break;
      }
    }
  }
  if (cameraId !== '') {
    cameraLabel.innerHTML = cameraName;
  } else {
    cameraLabel.innerHTML = cameraName + ' is not available';
    cameraLabel.style.color = 'red';
  }
}

function main() {
  // Trigger the user permission prompt by a getUserMedia
  navigator.mediaDevices.getUserMedia({video: true})
      .then(function(stream) {
        stream.getTracks().forEach(function(track) {
          track.stop();
        });
        navigator.mediaDevices.enumerateDevices().then(gotDevices, errorCallback);
      }, errorCallback);
}

function errorCallback(error) {
  statusElement.innerHTML = error;
}

var previewing = false;

startButton.onclick = function(e) {
  if (previewing) return;
  if (cameraId === '') {
    return;
  }
  var resolution = constraintsMap[resolutionSelect.value];
  var constraints = {video: {
    width: {exact: resolution.width},
    height: {exact: resolution.height},
    frameRate: {exact: resolution.fps}}};
  constraints.video.deviceId = {exact: cameraId};
  navigator.mediaDevices.getUserMedia(constraints)
      .then(function(stream) {
        previewStream = stream;
        videoElement.srcObject = stream;
        try {
          photoCapture = new realsense.DepthEnabledPhotography.PhotoCapture(stream);
        } catch (e) {
          statusElement.innerHTML = e.message;
          return;
        }
        previewing = true;
        photoCapture.onerror = function(e) {
          statusElement.innerHTML = e.message;
        };
        photoCapture.ondepthquality = function(e) {
          eventsFPS.update();
          var quality = e.quality;
          qualityElement.innerHTML = qualityMap[quality].text;
          qualityElement.style.color = qualityMap[quality].color;
          if (depthMapCheckBox.checked) {
            photoCapture.getDepthImage().then(
                function(image) {
                  depthResolution.innerHTML = '(' + image.width + 'x' + image.height + ')';
                  if (image.width != depthCanvas.width || image.height != depthCanvas.height) {
                    depthCanvas.width = image.width;
                    depthCanvas.height = image.height;
                    depthContext = depthCanvas.getContext('2d');
                  }
                  depthContext.clearRect(0, 0, depthCanvas.width, depthCanvas.height);
                  var imageData = depthContext.createImageData(image.width, image.height);
                  RSUtils.ConvertDepthToRGBUsingHistogram(
                      image, [255, 255, 255], [0, 0, 0], imageData.data);
                  depthContext.putImageData(imageData, 0, 0);
                },
                function(e) { statusElement.innerHTML = e.message; });
          } else {
            depthResolution.innerHTML = '';
          }
        };
      }, errorCallback);
};

stopButton.onclick = function(e) {
  if (previewStream) {
    // Remove listeners as we don't care about the events.
    photoCapture.onerror = null;
    photoCapture.ondepthquality = null;
    previewStream.getTracks().forEach(function(track) {
      track.stop();
    });
    previewing = false;
  }
};

takePhotoButton.onclick = function(e) {
  if (photoCapture) {
    statusElement.innerHTML = 'Taking depth photo...';
    photoCapture.takePhoto().then(function(photo) {
      statusElement.innerHTML = 'Saving depth photo...';
      realsense.DepthEnabledPhotography.XDMUtils.saveXDM(photo).then(
          function(blob) {
            xwalk.experimental.native_file_system.requestNativeFileSystem('pictures',
                function(fs) {
                  var fileName = '/pictures/depthphoto_' + RSUtils.getDateString() + '.jpg';
                  fs.root.getFile(fileName, { create: true }, function(entry) {
                    entry.createWriter(function(writer) {
                      writer.onwriteend = function(e) {
                        statusElement.innerHTML =
                            'The depth photo has been saved to ' + fileName + ' successfully.';
                      };
                      writer.onerror = function(e) {
                        statusElement.innerHTML = 'Failed to save depth photo.';
                      };
                      writer.write(blob);
                    },
                    function(e) { statusElement.innerHTML = e; });
                  },
                  function(e) { statusElement.innerHTML = e; });
                });
          },
          function(e) { statusElement.innerHTML = e.message; });
    },
    function(e) { statusElement.innerHTML = e.message; });
  }
};

depthMapCheckBox.onchange = function(e) {
  if (depthMapCheckBox.checked) {
    videoElement.style.display = 'none';
    depthCanvas.style.display = 'inline';
  } else {
    videoElement.style.display = 'inline';
    depthCanvas.style.display = 'none';
  }
};
