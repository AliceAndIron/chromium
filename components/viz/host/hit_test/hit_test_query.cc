// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/hit_test/hit_test_query.h"

#include "base/metrics/histogram_macros.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace viz {
namespace {

// If we want to add new source type here, consider switching to use
// ui::EventPointerType instead of EventSource.
bool RegionMatchEventSource(EventSource event_source, uint32_t flags) {
  if (event_source == EventSource::TOUCH)
    return (flags & mojom::kHitTestTouch) != 0u;
  if (event_source == EventSource::MOUSE)
    return (flags & mojom::kHitTestMouse) != 0u;
  return (flags & (mojom::kHitTestMouse | mojom::kHitTestTouch)) != 0u;
}

bool CheckChildCount(int32_t child_count, uint32_t child_count_max) {
  return (child_count >= 0) &&
         (static_cast<uint32_t>(child_count) < child_count_max);
}

}  // namespace

HitTestQuery::HitTestQuery(base::RepeatingClosure bad_message_gpu_callback)
    : bad_message_gpu_callback_(std::move(bad_message_gpu_callback)) {}

HitTestQuery::~HitTestQuery() = default;

void HitTestQuery::OnAggregatedHitTestRegionListUpdated(
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  hit_test_data_.clear();
  hit_test_data_ = hit_test_data;
  hit_test_data_size_ = hit_test_data.size();
}

Target HitTestQuery::FindTargetForLocation(
    EventSource event_source,
    const gfx::PointF& location_in_root) const {
  SCOPED_UMA_HISTOGRAM_TIMER("Event.VizHitTest.TargetTime");

  Target target;
  if (!hit_test_data_size_)
    return target;

  FindTargetInRegionForLocation(event_source, location_in_root, 0, &target);
  return target;
}

bool HitTestQuery::TransformLocationForTarget(
    EventSource event_source,
    const std::vector<FrameSinkId>& target_ancestors,
    const gfx::PointF& location_in_root,
    gfx::PointF* transformed_location) const {
  SCOPED_UMA_HISTOGRAM_TIMER("Event.VizHitTest.TransformTime");

  if (!hit_test_data_size_)
    return false;

  if (target_ancestors.size() == 0u ||
      target_ancestors[target_ancestors.size() - 1] !=
          hit_test_data_[0].frame_sink_id) {
    return false;
  }

  // TODO(riajiang): Cache the matrix product such that the transform can be
  // done immediately. crbug/758062.
  *transformed_location = location_in_root;
  return TransformLocationForTargetRecursively(event_source, target_ancestors,
                                               target_ancestors.size() - 1, 0,
                                               transformed_location);
}

bool HitTestQuery::GetTransformToTarget(const FrameSinkId& target,
                                        gfx::Transform* transform) const {
  if (!hit_test_data_size_)
    return false;

  return GetTransformToTargetRecursively(target, 0, transform);
}

bool HitTestQuery::FindTargetInRegionForLocation(
    EventSource event_source,
    const gfx::PointF& location_in_parent,
    uint32_t region_index,
    Target* target) const {
  gfx::PointF location_transformed(location_in_parent);
  hit_test_data_[region_index].transform().TransformPoint(
      &location_transformed);
  if (!gfx::RectF(hit_test_data_[region_index].rect)
           .Contains(location_transformed)) {
    return false;
  }

  const int32_t region_child_count = hit_test_data_[region_index].child_count;
  if (!CheckChildCount(region_child_count, hit_test_data_size_ - region_index))
    return false;

  uint32_t child_region = region_index + 1;
  uint32_t child_region_end = child_region + region_child_count;
  gfx::PointF location_in_target =
      location_transformed -
      hit_test_data_[region_index].rect.OffsetFromOrigin();
  while (child_region < child_region_end) {
    if (FindTargetInRegionForLocation(event_source, location_in_target,
                                      child_region, target)) {
      return true;
    }

    const int32_t child_region_child_count =
        hit_test_data_[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  const uint32_t flags = hit_test_data_[region_index].flags;
  if (!RegionMatchEventSource(event_source, flags))
    return false;
  if (flags & (mojom::kHitTestMine | mojom::kHitTestAsk)) {
    target->frame_sink_id = hit_test_data_[region_index].frame_sink_id;
    target->location_in_target = location_in_target;
    target->flags = flags;
    return true;
  }
  return false;
}

bool HitTestQuery::TransformLocationForTargetRecursively(
    EventSource event_source,
    const std::vector<FrameSinkId>& target_ancestors,
    size_t target_ancestor,
    uint32_t region_index,
    gfx::PointF* location_in_target) const {
  const uint32_t flags = hit_test_data_[region_index].flags;
  if ((flags & mojom::kHitTestChildSurface) == 0u &&
      !RegionMatchEventSource(event_source, flags)) {
    return false;
  }

  hit_test_data_[region_index].transform().TransformPoint(location_in_target);
  location_in_target->Offset(-hit_test_data_[region_index].rect.x(),
                             -hit_test_data_[region_index].rect.y());
  if (!target_ancestor)
    return true;

  const int32_t region_child_count = hit_test_data_[region_index].child_count;
  if (!CheckChildCount(region_child_count, hit_test_data_size_ - region_index))
    return false;

  uint32_t child_region = region_index + 1;
  uint32_t child_region_end = child_region + region_child_count;
  while (child_region < child_region_end) {
    if (hit_test_data_[child_region].frame_sink_id ==
        target_ancestors[target_ancestor - 1]) {
      return TransformLocationForTargetRecursively(
          event_source, target_ancestors, target_ancestor - 1, child_region,
          location_in_target);
    }

    const int32_t child_region_child_count =
        hit_test_data_[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  return false;
}

bool HitTestQuery::GetTransformToTargetRecursively(
    const FrameSinkId& target,
    uint32_t region_index,
    gfx::Transform* transform) const {
  // TODO(riajiang): Cache the matrix product such that the transform can be
  // found immediately.
  if (hit_test_data_[region_index].frame_sink_id == target) {
    *transform = hit_test_data_[region_index].transform();
    transform->Translate(-hit_test_data_[region_index].rect.x(),
                         -hit_test_data_[region_index].rect.y());
    return true;
  }

  const int32_t region_child_count = hit_test_data_[region_index].child_count;
  if (!CheckChildCount(region_child_count, hit_test_data_size_ - region_index))
    return false;

  uint32_t child_region = region_index + 1;
  uint32_t child_region_end = child_region + region_child_count;
  while (child_region < child_region_end) {
    gfx::Transform transform_to_child;
    if (GetTransformToTargetRecursively(target, child_region,
                                        &transform_to_child)) {
      gfx::Transform region_transform(hit_test_data_[region_index].transform());
      region_transform.Translate(-hit_test_data_[region_index].rect.x(),
                                 -hit_test_data_[region_index].rect.y());
      *transform = transform_to_child * region_transform;
      return true;
    }

    const int32_t child_region_child_count =
        hit_test_data_[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  return false;
}

void HitTestQuery::ReceivedBadMessageFromGpuProcess() const {
  if (!bad_message_gpu_callback_.is_null())
    bad_message_gpu_callback_.Run();
}

}  // namespace viz
