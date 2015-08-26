// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "syzygy/bard/events/linked_event.h"

namespace bard {
namespace events {

LinkedEvent::LinkedEvent(scoped_ptr<EventInterface> event)
    : waitable_event_(true, false) {
  DCHECK_NE(static_cast<EventInterface*>(nullptr), event.get());
  event_ = std::move(event);
}

void LinkedEvent::AddPrequel(LinkedEvent* prequel) {
  DCHECK_NE(static_cast<LinkedEvent*>(nullptr), prequel);
  prequels_.insert(prequel);
}

bool LinkedEvent::Play(void* backdrop) {
  DCHECK_NE(static_cast<void*>(nullptr), backdrop);

  for (auto& event : prequels_)
    event->waitable_event_.Wait();

  if (!event_->Play(backdrop))
    return false;

  waitable_event_.Signal();
  return true;
}

}  // namespace events
}  // namespace bard