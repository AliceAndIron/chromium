/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

static const double kLongTaskObserverThreshold = 0.05;

static const char kUnknownAttribution[] = "unknown";
static const char kAmbiguousAttribution[] = "multiple-contexts";
static const char kSameOriginAttribution[] = "same-origin";
static const char kSameOriginSelfAttribution[] = "self";
static const char kSameOriginAncestorAttribution[] = "same-origin-ancestor";
static const char kSameOriginDescendantAttribution[] = "same-origin-descendant";
static const char kCrossOriginAncestorAttribution[] = "cross-origin-ancestor";
static const char kCrossOriginDescendantAttribution[] =
    "cross-origin-descendant";
static const char kCrossOriginAttribution[] = "cross-origin-unreachable";

namespace blink {

namespace {

String GetFrameAttribute(HTMLFrameOwnerElement* frame_owner,
                         const QualifiedName& attr_name,
                         bool truncate) {
  String attr_value;
  if (frame_owner->hasAttribute(attr_name)) {
    attr_value = frame_owner->getAttribute(attr_name);
    if (truncate && attr_value.length() > 100)
      attr_value = attr_value.Substring(0, 100);  // Truncate to 100 chars
  }
  return attr_value;
}

const char* SameOriginAttribution(Frame* observer_frame, Frame* culprit_frame) {
  if (observer_frame == culprit_frame)
    return kSameOriginSelfAttribution;
  if (observer_frame->Tree().IsDescendantOf(culprit_frame))
    return kSameOriginAncestorAttribution;
  if (culprit_frame->Tree().IsDescendantOf(observer_frame))
    return kSameOriginDescendantAttribution;
  return kSameOriginAttribution;
}

bool IsSameOrigin(String key) {
  return key == kSameOriginAttribution ||
         key == kSameOriginDescendantAttribution ||
         key == kSameOriginAncestorAttribution ||
         key == kSameOriginSelfAttribution;
}

}  // namespace

static TimeTicks ToTimeOrigin(LocalDOMWindow* window) {
  Document* document = window->document();
  if (!document)
    return TimeTicks();

  DocumentLoader* loader = document->Loader();
  if (!loader)
    return TimeTicks();

  return loader->GetTiming().ReferenceMonotonicTime();
}

WindowPerformance::WindowPerformance(LocalDOMWindow* window)
    : Performance(
          ToTimeOrigin(window),
          window->document()->GetTaskRunner(TaskType::kPerformanceTimeline)),
      DOMWindowClient(window) {}

WindowPerformance::~WindowPerformance() = default;

ExecutionContext* WindowPerformance::GetExecutionContext() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetDocument();
}

PerformanceTiming* WindowPerformance::timing() const {
  if (!timing_)
    timing_ = PerformanceTiming::Create(GetFrame());

  return timing_.Get();
}

PerformanceNavigation* WindowPerformance::navigation() const {
  if (!navigation_)
    navigation_ = PerformanceNavigation::Create(GetFrame());

  return navigation_.Get();
}

MemoryInfo* WindowPerformance::memory() const {
  return MemoryInfo::Create();
}

PerformanceNavigationTiming*
WindowPerformance::CreateNavigationTimingInstance() {
  if (!RuntimeEnabledFeatures::PerformanceNavigationTiming2Enabled())
    return nullptr;
  if (!GetFrame())
    return nullptr;
  const DocumentLoader* document_loader =
      GetFrame()->Loader().GetDocumentLoader();
  DCHECK(document_loader);
  ResourceTimingInfo* info = document_loader->GetNavigationTimingInfo();
  if (!info)
    return nullptr;
  WebVector<WebServerTimingInfo> server_timing =
      PerformanceServerTiming::ParseServerTiming(*info);
  if (!server_timing.empty())
    UseCounter::Count(GetFrame(), WebFeature::kPerformanceServerTiming);
  return new PerformanceNavigationTiming(GetFrame(), info, time_origin_,
                                         server_timing);
}

void WindowPerformance::UpdateLongTaskInstrumentation() {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return;

  if (HasObserverFor(PerformanceEntry::kLongTask)) {
    UseCounter::Count(&GetFrame()->LocalFrameRoot(),
                      WebFeature::kLongTaskObserver);
    GetFrame()->GetPerformanceMonitor()->Subscribe(
        PerformanceMonitor::kLongTask, kLongTaskObserverThreshold, this);
  } else {
    GetFrame()->GetPerformanceMonitor()->UnsubscribeAll(this);
  }
}

void WindowPerformance::BuildJSONValue(V8ObjectBuilder& builder) const {
  Performance::BuildJSONValue(builder);
  builder.Add("timing", timing()->toJSONForBinding(builder.GetScriptState()));
  builder.Add("navigation",
              navigation()->toJSONForBinding(builder.GetScriptState()));
}

void WindowPerformance::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigation_);
  visitor->Trace(timing_);
  Performance::Trace(visitor);
  PerformanceMonitor::Client::Trace(visitor);
  DOMWindowClient::Trace(visitor);
}

static bool CanAccessOrigin(Frame* frame1, Frame* frame2) {
  const SecurityOrigin* security_origin1 =
      frame1->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* security_origin2 =
      frame2->GetSecurityContext()->GetSecurityOrigin();
  return security_origin1->CanAccess(security_origin2);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
// static
std::pair<String, DOMWindow*> WindowPerformance::SanitizedAttribution(
    ExecutionContext* task_context,
    bool has_multiple_contexts,
    LocalFrame* observer_frame) {
  if (has_multiple_contexts) {
    // Unable to attribute, multiple script execution contents were involved.
    return std::make_pair(kAmbiguousAttribution, nullptr);
  }

  if (!task_context || !task_context->IsDocument() ||
      !ToDocument(task_context)->GetFrame()) {
    // Unable to attribute as no script was involved.
    return std::make_pair(kUnknownAttribution, nullptr);
  }

  // Exactly one culprit location, attribute based on origin boundary.
  Frame* culprit_frame = ToDocument(task_context)->GetFrame();
  DCHECK(culprit_frame);
  if (CanAccessOrigin(observer_frame, culprit_frame)) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(SameOriginAttribution(observer_frame, culprit_frame),
                          culprit_frame->DomWindow());
  }
  // For cross-origin, if the culprit is the descendant or ancestor of
  // observer then indicate the *closest* cross-origin frame between
  // the observer and the culprit, in the corresponding direction.
  if (culprit_frame->Tree().IsDescendantOf(observer_frame)) {
    // If the culprit is a descendant of the observer, then walk up the tree
    // from culprit to observer, and report the *last* cross-origin (from
    // observer) frame.  If no intermediate cross-origin frame is found, then
    // report the culprit directly.
    Frame* last_cross_origin_frame = culprit_frame;
    for (Frame* frame = culprit_frame; frame != observer_frame;
         frame = frame->Tree().Parent()) {
      if (!CanAccessOrigin(observer_frame, frame)) {
        last_cross_origin_frame = frame;
      }
    }
    return std::make_pair(kCrossOriginDescendantAttribution,
                          last_cross_origin_frame->DomWindow());
  }
  if (observer_frame->Tree().IsDescendantOf(culprit_frame)) {
    return std::make_pair(kCrossOriginAncestorAttribution, nullptr);
  }
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

void WindowPerformance::ReportLongTask(
    double start_time,
    double end_time,
    ExecutionContext* task_context,
    bool has_multiple_contexts,
    const SubTaskAttribution::EntriesVector& sub_task_attributions) {
  if (!GetFrame())
    return;
  std::pair<String, DOMWindow*> attribution =
      WindowPerformance::SanitizedAttribution(
          task_context, has_multiple_contexts, GetFrame());
  DOMWindow* culprit_dom_window = attribution.second;
  SubTaskAttribution::EntriesVector empty_vector;
  if (!culprit_dom_window || !culprit_dom_window->GetFrame() ||
      !culprit_dom_window->GetFrame()->DeprecatedLocalOwner()) {
    AddLongTaskTiming(
        TimeTicksFromSeconds(start_time), TimeTicksFromSeconds(end_time),
        attribution.first, g_empty_string, g_empty_string, g_empty_string,
        IsSameOrigin(attribution.first) ? sub_task_attributions : empty_vector);
  } else {
    HTMLFrameOwnerElement* frame_owner =
        culprit_dom_window->GetFrame()->DeprecatedLocalOwner();
    AddLongTaskTiming(
        TimeTicksFromSeconds(start_time), TimeTicksFromSeconds(end_time),
        attribution.first,
        GetFrameAttribute(frame_owner, HTMLNames::srcAttr, false),
        GetFrameAttribute(frame_owner, HTMLNames::idAttr, false),
        GetFrameAttribute(frame_owner, HTMLNames::nameAttr, true),
        IsSameOrigin(attribution.first) ? sub_task_attributions : empty_vector);
  }
}

bool WindowPerformance::ShouldBufferEventTiming() {
  // We buffer Long-latency events until onload, i.e., LoadEventStart
  // is not reached yet.
  const DocumentLoader* loader = GetFrame()->Loader().GetDocumentLoader();
  DCHECK(loader);
  TimeTicks load_start = loader->GetTiming().LoadEventStart();
  return load_start.is_null();
}

bool WindowPerformance::ObservingEventTimingEntries() {
  return HasObserverFor(PerformanceEntry::kEvent);
}

void WindowPerformance::AddEventTiming(String event_type,
                                       TimeTicks start_time,
                                       TimeTicks processing_start,
                                       TimeDelta duration,
                                       bool cancelable) {
  DCHECK(RuntimeEnabledFeatures::EventTimingEnabled());

  DCHECK(!start_time.is_null());
  DCHECK(!processing_start.is_null());
  PerformanceEventTiming* entry = PerformanceEventTiming::Create(
      event_type, start_time - time_origin_, processing_start - time_origin_,
      start_time + duration - time_origin_, cancelable);

  if (ObservingEventTimingEntries())
    NotifyObserversOfEntry(*entry);

  if (ShouldBufferEventTiming() && !IsEventTimingBufferFull())
    AddEventTimingBuffer(*entry);
}

}  // namespace blink
