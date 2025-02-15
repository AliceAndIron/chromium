// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/media_router/web_contents_display_observer_view.h"

#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace media_router {

// static
std::unique_ptr<WebContentsDisplayObserver> WebContentsDisplayObserver::Create(
    content::WebContents* web_contents,
    base::RepeatingClosure callback) {
  return std::make_unique<WebContentsDisplayObserverView>(web_contents,
                                                          std::move(callback));
}

WebContentsDisplayObserverView::WebContentsDisplayObserverView(
    content::WebContents* web_contents,
    base::RepeatingClosure callback)
    : web_contents_(web_contents),
      widget_(views::Widget::GetWidgetForNativeWindow(
          web_contents->GetTopLevelNativeWindow())),
      callback_(std::move(callback)) {
  // |widget_| may be null in tests.
  if (widget_) {
    display_ = GetDisplayNearestWidget();
    widget_->AddObserver(this);
  }
  BrowserList::GetInstance()->AddObserver(this);
}

WebContentsDisplayObserverView::~WebContentsDisplayObserverView() {
  if (widget_)
    widget_->RemoveObserver(this);
  BrowserList::GetInstance()->RemoveObserver(this);
}

void WebContentsDisplayObserverView::OnBrowserSetLastActive(Browser* browser) {
  // This gets called when a browser tab detaches from a window or gets merged
  // into another window. We update the widget to observe, if necessary.
  views::Widget* new_widget = views::Widget::GetWidgetForNativeWindow(
      web_contents_->GetTopLevelNativeWindow());
  if (new_widget != widget_) {
    if (widget_)
      widget_->RemoveObserver(this);
    widget_ = new_widget;
    if (widget_) {
      widget_->AddObserver(this);
      CheckForDisplayChange();
    }
  }
}

void WebContentsDisplayObserverView::OnWidgetClosing(views::Widget* widget) {
  if (widget_)
    widget_->RemoveObserver(this);
  widget_ = nullptr;
}

void WebContentsDisplayObserverView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  CheckForDisplayChange();
}

const display::Display& WebContentsDisplayObserverView::GetCurrentDisplay()
    const {
  return display_;
}

void WebContentsDisplayObserverView::CheckForDisplayChange() {
  display::Display new_display = GetDisplayNearestWidget();
  if (new_display.id() == display_.id())
    return;

  display_ = new_display;
  callback_.Run();
}

display::Display WebContentsDisplayObserverView::GetDisplayNearestWidget()
    const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      widget_->GetNativeWindow());
}

}  // namespace media_router
