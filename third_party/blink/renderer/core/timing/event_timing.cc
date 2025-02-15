// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_timing.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// Events taking longer than this threshold to finish being processed are
// regarded as long-latency events by event-timing. Shorter-latency events are
// ignored to reduce performance impact.
constexpr TimeDelta kEventTimingDurationThreshold =
    TimeDelta::FromMilliseconds(50);

EventTiming::EventTiming(LocalDOMWindow* window) {
  performance_ = DOMWindowPerformance::performance(*window);
}

void EventTiming::WillDispatchEvent(const Event* event) {
  // Assume each event can be dispatched only once.
  DCHECK(!finished_will_dispatch_event_);
  if (!performance_ || !event->isTrusted())
    return;

  // Although we screen the events for timing by setting these conditions here,
  // we cannot assume that the conditions should still hold true in
  // DidDispatchEvent. These conditions have to be re-tested before an entry is
  // dispatched.
  if ((performance_->ShouldBufferEventTiming() &&
       !performance_->IsEventTimingBufferFull()) ||
      performance_->ObservingEventTimingEntries()) {
    processing_start_ = CurrentTimeTicks();
    finished_will_dispatch_event_ = true;
  }
}

void EventTiming::DidDispatchEvent(const Event* event) {
  if (!finished_will_dispatch_event_ ||
      (!event->executedListenerOrDefaultAction() && !event->DefaultHandled())) {
    return;
  }

  TimeTicks start_time;
  if (event->IsPointerEvent())
    start_time = ToPointerEvent(event)->OldestPlatformTimeStamp();
  else
    start_time = event->PlatformTimeStamp();

  const TimeDelta duration = CurrentTimeTicks() - start_time;
  if (duration < kEventTimingDurationThreshold)
    return;

  performance_->AddEventTiming(event->type(), start_time, processing_start_,
                               duration, event->cancelable());
}

}  // namespace blink
