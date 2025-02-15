// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MEMORY_METRICS_REPORTER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MEMORY_METRICS_REPORTER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

// The TabMemoryMetricsReporter reports each tab's Memory_Experimental UKM
// 1, 5, 10, 15 minutes after the tab is loaded.
class TabMemoryMetricsReporter : public TabLoadTracker::Observer {
 public:
  TabMemoryMetricsReporter();
  ~TabMemoryMetricsReporter() override;

  // Returns the singleton TabMemoryMetricsReporter instance.
  static TabMemoryMetricsReporter* Get();

  void StartReporting(TabLoadTracker* tracker);

  void OnStartTracking(content::WebContents* web_contents,
                       LoadingState loading_state) override;
  void OnLoadingStateChange(content::WebContents* web_contents,
                            LoadingState loading_state) override;
  void OnStopTracking(content::WebContents* web_contents,
                      LoadingState loading_state) override;

 private:
  // For unittesting.
  friend class TestTabMemoryMetricsReporter;

  // Constructor for TabMemoryMetricsReporterTest.
  explicit TabMemoryMetricsReporter(const base::TickClock* tick_clock);

  void UpdateTimerCallback();

  enum ReportState {
    NO_METRICS_EMITTED,
    EMITTED_1MIN_METRIC,
    EMITTED_5MIN_METRIC,
    EMITTED_10MIN_METRIC,
    EMITTED_ALL_METRICS,
    CONTENT_GONE
  };

  struct WebContentsData {
    base::TimeTicks page_loaded_time;
    base::TimeTicks next_emit_time;
    ReportState state;
    content::WebContents* web_contents;
  };

  struct WebContentsDataComparator {
    bool operator()(const WebContentsData& a, const WebContentsData& b);
  };

  void MonitorWebContents(content::WebContents* web_contents);
  void RemoveWebContentsDataFromMonitoredListIfExists(
      content::WebContents* web_contents);

  void RestartTimerIfNeeded(base::TimeTicks current_time);

  // Returns next emit time after page loaded.
  static base::TimeDelta NextEmitTimeAfterPageLoaded(ReportState);

  // Returns next state of process memory dump after page loaded.
  static ReportState NextStateOfEmitMemoryDumpAfterPageLoaded(
      base::TimeDelta time_passed);
  // Emit process memory dump of the specified process.
  // Make this method virtual for TabMemoryMetricsReporterTest.
  virtual void EmitMemoryMetricsAfterPageLoaded(
      const WebContentsData& content_data);

  // true if this TabMemoryMetricsReporter has already invoked
  // TabLoadTracker::AddObserver.
  // TODO(tasak): clean up this up once there's a single cross-platform
  // static initialization codepath in resource_coordinator.
  bool reporting_started_ = false;

  // Timer to periodically update the stats of the renderers.
  base::OneShotTimer update_timer_;

  // A list of web contents to be reported their memory usage, sorted by
  // next_emit_time.
  std::set<WebContentsData, WebContentsDataComparator> monitored_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabMemoryMetricsReporter);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MEMORY_METRICS_REPORTER_H_
