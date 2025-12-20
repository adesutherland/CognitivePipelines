//
// Global test application initializer
// Ensures a QApplication exists before any unit test uses QWidget/QWebEngine
// to avoid crashes on platforms where tests may run before helpers call
// ensureApp().
//

#include "test_app.h"

namespace {
QApplication* init_test_app = sharedTestApp();
}
