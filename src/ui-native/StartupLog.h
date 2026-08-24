#pragma once

#include <string>

// A minimal, dependency-free logger written next to the running executable,
// used for lifecycle/crash diagnostics that must survive even if the rest
// of the app (DetectionLogger, DetectionWorker, ...) never gets far enough
// to initialize - a machine where the app crashes or fails silently on
// startup wouldn't otherwise leave any trace at all. Opens/closes the file
// on every call (rather than holding it open) so a message written right
// before a crash is never lost to a missing flush.
namespace StartupLog {
    void Write(const std::string& message);
}
