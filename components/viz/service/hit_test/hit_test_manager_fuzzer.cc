// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/command_line.h"
#include "base/test/fuzzed_data_provider.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "components/viz/service/hit_test/hit_test_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_latest_local_surface_id_lookup_delegate.h"

namespace {

constexpr uint32_t kMaxDepthAllowed = 255;

// TODO(riajiang): Move into common functions that can be used by the fuzzer
// for HitTestQuery.
uint32_t GetNextUInt32NonZero(base::FuzzedDataProvider* fuzz) {
  return fuzz->ConsumeUint32InRange(1, std::numeric_limits<uint32_t>::max());
}

gfx::Transform GetNextTransform(base::FuzzedDataProvider* fuzz) {
  gfx::Transform transform;
  if (fuzz->ConsumeBool() && fuzz->remaining_bytes() >= sizeof(transform)) {
    std::string matrix_bytes = fuzz->ConsumeBytes(sizeof(gfx::Transform));
    memcpy(&transform, matrix_bytes.data(), sizeof(gfx::Transform));
  }
  return transform;
}

void SubmitHitTestRegionList(
    base::FuzzedDataProvider* fuzz,
    viz::TestLatestLocalSurfaceIdLookupDelegate* delegate,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    const viz::SurfaceId& surface_id,
    bool support_is_root,
    const uint32_t depth);

void AddHitTestRegion(base::FuzzedDataProvider* fuzz,
                      std::vector<viz::mojom::HitTestRegionPtr>* regions,
                      uint32_t child_count,
                      viz::TestLatestLocalSurfaceIdLookupDelegate* delegate,
                      viz::FrameSinkManagerImpl* frame_sink_manager,
                      const viz::SurfaceId& surface_id,
                      const uint32_t depth) {
  if (!child_count || depth > kMaxDepthAllowed)
    return;

  // If there's not enough space left for a HitTestRegion, then skip.
  if (fuzz->remaining_bytes() < sizeof(viz::mojom::HitTestRegion))
    return;

  auto hit_test_region = viz::mojom::HitTestRegion::New();
  hit_test_region->flags = fuzz->ConsumeUint16();
  if (fuzz->ConsumeBool())
    hit_test_region->flags |= viz::mojom::kHitTestChildSurface;
  hit_test_region->frame_sink_id =
      viz::FrameSinkId(fuzz->ConsumeUint8(), fuzz->ConsumeUint8());
  hit_test_region->rect =
      gfx::Rect(fuzz->ConsumeUint8(), fuzz->ConsumeUint8(),
                fuzz->ConsumeUint16(), fuzz->ConsumeUint16());
  hit_test_region->transform = GetNextTransform(fuzz);

  if (fuzz->ConsumeBool() &&
      (hit_test_region->flags & viz::mojom::kHitTestChildSurface)) {
    // If there's not enough space left for a LocalSurfaceId, then skip.
    if (fuzz->remaining_bytes() < sizeof(viz::LocalSurfaceId))
      return;

    uint32_t last_frame_sink_id_client_id =
        surface_id.frame_sink_id().client_id();
    uint32_t last_frame_sink_id_sink_id = surface_id.frame_sink_id().sink_id();
    viz::FrameSinkId frame_sink_id(last_frame_sink_id_client_id + 1,
                                   last_frame_sink_id_sink_id + 1);
    viz::LocalSurfaceId local_surface_id(GetNextUInt32NonZero(fuzz),
                                         GetNextUInt32NonZero(fuzz),
                                         base::UnguessableToken::Create());
    SubmitHitTestRegionList(fuzz, delegate, frame_sink_manager,
                            viz::SurfaceId(frame_sink_id, local_surface_id),
                            false /* support_is_root */, depth + 1);
  }

  regions->push_back(std::move(hit_test_region));
  AddHitTestRegion(fuzz, regions, child_count - 1, delegate, frame_sink_manager,
                   surface_id, depth + 1);
}

void SubmitHitTestRegionList(
    base::FuzzedDataProvider* fuzz,
    viz::TestLatestLocalSurfaceIdLookupDelegate* delegate,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    const viz::SurfaceId& surface_id,
    bool support_is_root,
    const uint32_t depth) {
  // If there's not enough space left for a HitTestRegionList, then skip.
  if ((fuzz->remaining_bytes() < sizeof(viz::mojom::HitTestRegionList)) ||
      depth > kMaxDepthAllowed) {
    return;
  }

  auto hit_test_region_list = viz::mojom::HitTestRegionList::New();
  hit_test_region_list->flags = fuzz->ConsumeUint16();
  if (fuzz->ConsumeBool())
    hit_test_region_list->flags |= viz::mojom::kHitTestChildSurface;
  hit_test_region_list->bounds =
      gfx::Rect(fuzz->ConsumeUint8(), fuzz->ConsumeUint8(),
                fuzz->ConsumeUint16(), fuzz->ConsumeUint16());
  hit_test_region_list->transform = GetNextTransform(fuzz);

  uint32_t child_count = fuzz->ConsumeUint16();
  AddHitTestRegion(fuzz, &hit_test_region_list->regions, child_count, delegate,
                   frame_sink_manager, surface_id, depth + 1);

  delegate->SetSurfaceIdMap(surface_id);
  viz::CompositorFrameSinkSupport support(
      nullptr /* client */, frame_sink_manager, surface_id.frame_sink_id(),
      support_is_root, false /* needs_sync_points */);
  support.SubmitCompositorFrame(
      surface_id.local_surface_id(), viz::MakeDefaultCompositorFrame(),
      fuzz->ConsumeBool() ? std::move(hit_test_region_list) : nullptr);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t num_bytes) {
  base::FuzzedDataProvider fuzz(data, num_bytes);
  viz::FrameSinkManagerImpl frame_sink_manager;
  viz::TestLatestLocalSurfaceIdLookupDelegate delegate;
  viz::TestLatestLocalSurfaceIdLookupDelegate* lsi_delegate =
      fuzz.ConsumeBool() ? &delegate : nullptr;

  // If there's not enough space left for a LocalSurfaceId, then skip.
  if (fuzz.remaining_bytes() < sizeof(viz::LocalSurfaceId))
    return 0;

  constexpr uint32_t root_client_id = 1;
  constexpr uint32_t root_sink_id = 1;
  viz::FrameSinkId frame_sink_id(root_client_id, root_sink_id);
  viz::LocalSurfaceId local_surface_id(GetNextUInt32NonZero(&fuzz),
                                       GetNextUInt32NonZero(&fuzz),
                                       base::UnguessableToken::Create());
  viz::SurfaceId surface_id(frame_sink_id, local_surface_id);
  viz::HitTestAggregator aggregator(
      frame_sink_manager.hit_test_manager(), &frame_sink_manager, lsi_delegate,
      frame_sink_id, 10 /* initial_region_size */, 100 /* max_region_size */);

  SubmitHitTestRegionList(&fuzz, &delegate, &frame_sink_manager, surface_id,
                          true /* support_is_root */, 0 /* depth */);

  viz::SurfaceId aggregate_surface_id = surface_id;
  if (fuzz.ConsumeBool() && fuzz.remaining_bytes() >= sizeof(viz::SurfaceId)) {
    viz::FrameSinkId frame_sink_id(GetNextUInt32NonZero(&fuzz),
                                   GetNextUInt32NonZero(&fuzz));
    viz::LocalSurfaceId local_surface_id(GetNextUInt32NonZero(&fuzz),
                                         GetNextUInt32NonZero(&fuzz),
                                         base::UnguessableToken::Create());
    aggregate_surface_id = viz::SurfaceId(frame_sink_id, local_surface_id);
  }
  aggregator.Aggregate(aggregate_surface_id);
  viz::Surface* surface = frame_sink_manager.surface_manager()->GetSurfaceForId(
      aggregate_surface_id);
  if (surface)
    frame_sink_manager.surface_manager()->SurfaceDiscarded(surface);

  return 0;
}
