// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "services/shape_detection/face_detection_impl_mac_vision.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gl/gl_switches.h"

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace shape_detection {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

std::unique_ptr<mojom::FaceDetection> CreateFaceDetectorImplMac(
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  return std::make_unique<FaceDetectionImplMac>(
      mojom::FaceDetectorOptions::New());
}

std::unique_ptr<mojom::FaceDetection> CreateFaceDetectorImplMacVision(
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  if (@available(macOS 10.13, *)) {
    return std::make_unique<FaceDetectionImplMacVision>();
  } else {
    return nullptr;
  }
}

using FaceDetectorFactory =
    base::Callback<std::unique_ptr<mojom::FaceDetection>(
        shape_detection::mojom::FaceDetectorOptionsPtr)>;

}  // anonymous namespace

struct TestParams {
  bool fast_mode;
  int image_width;
  int image_height;
  const char* image_path;
  size_t num_faces;
  size_t num_landmarks;
  FaceDetectorFactory factory;
} kTestParams[] = {
    {false, 120, 120, "services/test/data/mona_lisa.jpg", 1, 3,
     base::Bind(&CreateFaceDetectorImplMac)},
    {true, 120, 120, "services/test/data/mona_lisa.jpg", 1, 3,
     base::Bind(&CreateFaceDetectorImplMac)},
    {false, 120, 120, "services/test/data/mona_lisa.jpg", 1, 4,
     base::Bind(&CreateFaceDetectorImplMacVision)},
    {false, 240, 240, "services/test/data/the_beatles.jpg", 3, 3,
     base::Bind(&CreateFaceDetectorImplMac)},
    {true, 240, 240, "services/test/data/the_beatles.jpg", 3, 3,
     base::Bind(&CreateFaceDetectorImplMac)},
    {false, 240, 240, "services/test/data/the_beatles.jpg", 4, 4,
     base::Bind(&CreateFaceDetectorImplMacVision)},
};

class FaceDetectionImplMacTest : public TestWithParam<struct TestParams> {
 public:
  ~FaceDetectionImplMacTest() override {}

  void DetectCallback(size_t num_faces,
                      size_t num_landmarks,
                      std::vector<mojom::FaceDetectionResultPtr> results) {
    EXPECT_EQ(num_faces, results.size());
    for (const auto& face : results) {
      EXPECT_EQ(num_landmarks, face->landmarks.size());
      if (face->landmarks.size() == 3u) {
        EXPECT_EQ(mojom::LandmarkType::EYE, face->landmarks[0]->type);
        EXPECT_EQ(mojom::LandmarkType::EYE, face->landmarks[1]->type);
        EXPECT_EQ(mojom::LandmarkType::MOUTH, face->landmarks[2]->type);
      }
    }
    Detection();
  }
  MOCK_METHOD0(Detection, void(void));

  std::unique_ptr<mojom::FaceDetection> impl_;
  const base::MessageLoop message_loop_;
};

TEST_P(FaceDetectionImplMacTest, CreateAndDestroy) {
  impl_ = GetParam().factory.Run(mojom::FaceDetectorOptions::New());
  if (!impl_ && base::mac::IsAtMostOS10_12()) {
    LOG(WARNING) << "FaceDetectionImplMacVision is not available before Mac "
                    "OSX 10.13. Skipping test.";
    return;
  }
}

TEST_P(FaceDetectionImplMacTest, ScanOneFace) {
  // Face detection test needs a GPU.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  auto options = shape_detection::mojom::FaceDetectorOptions::New();
  options->fast_mode = GetParam().fast_mode;
  impl_ = GetParam().factory.Run(std::move(options));
  if (!impl_ && base::mac::IsAtMostOS10_12()) {
    LOG(WARNING) << "FaceDetectionImplMacVision is not available before Mac "
                    "OSX 10.13. Skipping test.";
    return;
  }

  // Load image data from test directory.
  base::FilePath image_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &image_path));
  image_path = image_path.AppendASCII(GetParam().image_path);
  ASSERT_TRUE(base::PathExists(image_path));
  std::string image_data;
  ASSERT_TRUE(base::ReadFileToString(image_path, &image_data));

  std::unique_ptr<SkBitmap> image = gfx::JPEGCodec::Decode(
      reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size());
  ASSERT_TRUE(image);
  ASSERT_EQ(GetParam().image_width, image->width());
  ASSERT_EQ(GetParam().image_height, image->height());

  const gfx::Size size(image->width(), image->height());
  const size_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
  // This assert assumes there is no padding in the bitmap's rowbytes
  ASSERT_EQ(num_bytes, image->computeByteSize());

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  // Send the image to Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection()).WillOnce(RunClosure(quit_closure));
  impl_->Detect(*image,
                base::BindOnce(&FaceDetectionImplMacTest::DetectCallback,
                               base::Unretained(this), GetParam().num_faces,
                               GetParam().num_landmarks));

  run_loop.Run();
}

INSTANTIATE_TEST_CASE_P(, FaceDetectionImplMacTest, ValuesIn(kTestParams));

}  // shape_detection namespace
