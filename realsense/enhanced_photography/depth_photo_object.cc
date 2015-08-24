// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "realsense/enhanced_photography/depth_photo_object.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"

namespace realsense {
namespace enhanced_photography {

DepthPhotoObject::DepthPhotoObject(PXCPhoto* photo)
    : photo_(photo) {
  handler_.Register("getColorImage",
                    base::Bind(&DepthPhotoObject::OnGetColorImage,
                               base::Unretained(this)));
  handler_.Register("getDepthImage",
                    base::Bind(&DepthPhotoObject::OnGetDepthImage,
                               base::Unretained(this)));
  handler_.Register("setColorImage",
                    base::Bind(&DepthPhotoObject::OnSetColorImage,
                               base::Unretained(this)));
  handler_.Register("setDepthImage",
                    base::Bind(&DepthPhotoObject::OnSetDepthImage,
                               base::Unretained(this)));
}

DepthPhotoObject::~DepthPhotoObject() {
  DestroyPhoto();
}

void DepthPhotoObject::DestroyPhoto() {
  if (photo_) {
    photo_->Release();
    photo_ = nullptr;
  }
}

void DepthPhotoObject::OnGetColorImage(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  jsapi::depth_photo::Image img;
  if (!photo_) {
    info->PostResult(GetColorImage::Results::Create(img,
        "Invalid photo object"));
    return;
  }

  PXCImage* imColor = photo_->QueryReferenceImage();
  if (!CopyColorImage(imColor, &img)) {
    info->PostResult(GetColorImage::Results::Create(img,
        "Failed to get color image data."));
    return;
  }

  info->PostResult(GetColorImage::Results::Create(img, std::string()));
}

void DepthPhotoObject::OnGetDepthImage(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  jsapi::depth_photo::Image img;
  if (!photo_) {
    info->PostResult(GetDepthImage::Results::Create(img,
        "Invalid photo object."));
    return;
  }

  PXCImage* imDepth = photo_->QueryDepthImage();
  if (!CopyDepthImage(imDepth, &img)) {
    info->PostResult(GetDepthImage::Results::Create(img,
        "Failed to get depth image data."));
    return;
  }

  info->PostResult(GetDepthImage::Results::Create(img, std::string()));
}

void DepthPhotoObject::OnSetColorImage(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  if (!photo_) {
    info->PostResult(SetColorImage::Results::Create(std::string(),
        "Invalid photo object"));
    return;
  }

  scoped_ptr<SetColorImage::Params> params(
      SetColorImage::Params::Create(*info->arguments()));
  if (!params) {
    info->PostResult(
        SetColorImage::Results::Create(std::string(), "Malformed parameters"));
    return;
  }

  PXCImage* out = photo_->QueryReferenceImage();
  PXCImage::ImageInfo outInfo = out->QueryInfo();
  if (params->image.width != outInfo.width ||
      params->image.height != outInfo.height) {
    info->PostResult(SetColorImage::Results::Create(std::string(),
        "Wrong image width and height"));
    return;
  }

  PXCImage::ImageData outData;
  pxcStatus photoSts = out->AcquireAccess(PXCImage::ACCESS_READ_WRITE,
                                          PXCImage::PIXEL_FORMAT_RGB32,
                                          &outData);
  if (photoSts != PXC_STATUS_NO_ERROR) {
    info->PostResult(SetColorImage::Results::Create(std::string(),
        "Failed to get color data"));
    return;
  }

  for (int y = 0; y < outInfo.height; y++) {
    for (int x = 0; x < outInfo.width; x++) {
      int i = x * 4 + outData.pitches[0] * y;
      outData.planes[0][i] = params->image.data[i + 2];
      outData.planes[0][i + 1] = params->image.data[i + 1];
      outData.planes[0][i + 2] = params->image.data[i];
      outData.planes[0][i + 3] = params->image.data[i + 3];
    }
  }
  out->ReleaseAccess(&outData);

  info->PostResult(SetColorImage::Results::Create(std::string("Success"),
                                                  std::string()));
}

void DepthPhotoObject::OnSetDepthImage(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  if (!photo_) {
    info->PostResult(SetDepthImage::Results::Create(std::string(),
        "Invalid photo object"));
    return;
  }

  scoped_ptr<SetDepthImage::Params> params(
      SetDepthImage::Params::Create(*info->arguments()));
  if (!params) {
    info->PostResult(
        SetDepthImage::Results::Create(std::string(), "Malformed parameters"));
    return;
  }

  PXCImage* out = photo_->QueryDepthImage();
  PXCImage::ImageInfo outInfo = out->QueryInfo();
  if (params->image.width != outInfo.width ||
      params->image.height != outInfo.height) {
    info->PostResult(SetDepthImage::Results::Create(std::string(),
        "Wrong image width and height"));
    return;
  }

  PXCImage::ImageData outData;
  pxcStatus photoSts = out->AcquireAccess(PXCImage::ACCESS_READ_WRITE,
                                          PXCImage::PIXEL_FORMAT_DEPTH,
                                          &outData);
  if (photoSts != PXC_STATUS_NO_ERROR) {
    info->PostResult(SetDepthImage::Results::Create(std::string(),
        "Failed to get depth data"));
    return;
  }

  int i = 0;
  for (int y = 0; y < outInfo.height; ++y) {
    for (int x = 0; x < outInfo.width; ++x) {
      uint16_t* depth16 = reinterpret_cast<uint16_t*>(
        outData.planes[0] + outData.pitches[0] * y);
      depth16[x] = params->image.data[i];
      i++;
    }
  }
  out->ReleaseAccess(&outData);

  info->PostResult(SetDepthImage::Results::Create(std::string("Success"),
                                                  std::string()));
}

bool DepthPhotoObject::CopyColorImage(PXCImage* pxcimage,
                                      jsapi::depth_photo::Image* img) {
  if (!pxcimage) return false;

  PXCImage::ImageInfo image_info = pxcimage->QueryInfo();
  PXCImage::ImageData image_data;
  if (pxcimage->AcquireAccess(PXCImage::ACCESS_READ,
      PXCImage::PIXEL_FORMAT_RGB32, &image_data) < PXC_STATUS_NO_ERROR) {
    return false;
  }

  img->format = PixelFormat::PIXEL_FORMAT_RGB32;
  img->width = image_info.width;
  img->height = image_info.height;

  uint8_t* rgb32 = reinterpret_cast<uint8_t*>(image_data.planes[0]);
  for (int y = 0; y < image_info.height; y++) {
    for (int x = 0; x < image_info.width; x++) {
      int i = x * 4 + image_data.pitches[0] * y;
      img->data.push_back(rgb32[i + 2]);
      img->data.push_back(rgb32[i + 1]);
      img->data.push_back(rgb32[i]);
      img->data.push_back(rgb32[i + 3]);
    }
  }

  pxcimage->ReleaseAccess(&image_data);
  return true;
}

bool DepthPhotoObject::CopyDepthImage(PXCImage* depth,
                                      jsapi::depth_photo::Image* img) {
  if (!depth) return false;

  PXCImage::ImageInfo depth_info = depth->QueryInfo();
  img->format = PixelFormat::PIXEL_FORMAT_DEPTH;
  img->width = depth_info.width;
  img->height = depth_info.height;

  PXCImage::ImageData depth_data;
  if (depth->AcquireAccess(PXCImage::ACCESS_READ,
      PXCImage::PIXEL_FORMAT_DEPTH, &depth_data) < PXC_STATUS_NO_ERROR) {
    return false;
  }

  for (int y = 0; y < depth_info.height; ++y) {
    for (int x = 0; x < depth_info.width; ++x) {
      uint16_t* depth16 = reinterpret_cast<uint16_t*>(
          depth_data.planes[0] + depth_data.pitches[0] * y);
      img->data.push_back(depth16[x]);
    }
  }
  depth->ReleaseAccess(&depth_data);
  return true;
}

}  // namespace enhanced_photography
}  // namespace realsense