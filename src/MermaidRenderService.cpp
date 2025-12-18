//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "MermaidRenderService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QPixmap>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kDefaultWidth = 1024;
constexpr int kDefaultHeight = 768;
constexpr int kPadding = 32;
constexpr int kMaxDimension = 16384; // conservative cap to avoid texture/pixmap limits
constexpr double kMinScale = 0.1;
constexpr double kMinClampScale = 0.01; // below this, fail fast instead of attempting a huge render

QString normalizeCachePath(const QString& path) {
    const QFileInfo info(path);
    const QString base = info.fileName();
    const QString parent = info.dir().dirName();
    if (!path.isEmpty() && !base.isEmpty() && base == parent) {
        return info.dir().absolutePath();
    }
    return path;
}
} // namespace

MermaidRenderService& MermaidRenderService::instance() {
    static MermaidRenderService instance;
    return instance;
}

MermaidRenderService::MermaidRenderService(QObject* parent)
    : QObject(parent) {
    // Ensure the service lives on the main (GUI) thread so rendering happens
    // where QWebEngine is valid. If the singleton is first touched from a
    // worker thread, move it to the application's thread.
    if (QCoreApplication::instance() && thread() != QCoreApplication::instance()->thread()) {
        moveToThread(QCoreApplication::instance()->thread());
    }
}

void MermaidRenderService::ensureProfileInitialized() {
    if (m_profileInitialized) {
        return;
    }

    if (QCoreApplication::instance()) {
        Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    }

    const QString originalCacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString cacheBase = normalizeCachePath(originalCacheBase);
    QDir cacheDir(cacheBase.isEmpty() ? QDir::tempPath() : cacheBase);
    if (!cacheDir.exists()) {
        cacheDir.mkpath(QStringLiteral("."));
    }
    const QString webCachePath = cacheDir.filePath(QStringLiteral("qtwebengine_cache"));
    const QString webStoragePath = cacheDir.filePath(QStringLiteral("qtwebengine_storage"));
    QDir().mkpath(webCachePath);
    QDir().mkpath(webStoragePath);

    auto* profile = QWebEngineProfile::defaultProfile();
    profile->setCachePath(webCachePath);
    profile->setPersistentStoragePath(webStoragePath);
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);

    m_profileInitialized = true;
}

MermaidRenderService::RenderResult MermaidRenderService::renderMermaid(const QString& mermaidCode, const QString& outputPath, double scaleFactor) {
    RenderResult result;

    // Avoid BlockingQueuedConnection deadlock when already on the target thread
    // (e.g., UI thread). Use a direct call in that case; otherwise block until
    // the render finishes on the object's (GUI) thread.
    if (QThread::currentThread() == thread()) {
        renderOnMainThread(mermaidCode, outputPath, scaleFactor, &result);
    } else {
        QMetaObject::invokeMethod(
            this,
            "renderOnMainThread",
            Qt::BlockingQueuedConnection,
            Q_ARG(QString, mermaidCode),
            Q_ARG(QString, outputPath),
            Q_ARG(double, scaleFactor),
            Q_ARG(MermaidRenderService::RenderResult*, &result));
    }

    if (!result.ok && result.error.isEmpty()) {
        result.error = QStringLiteral("Mermaid rendering failed for %1").arg(outputPath);
    }

    if (!result.ok) {
        qWarning() << "MermaidRenderService::renderMermaid failed for output" << outputPath << "error" << result.error;
    }

    return result;
}

MermaidRenderService::RenderSizing MermaidRenderService::planRenderSizing(double svgWidth, double svgHeight, double scaleFactor, double devicePixelRatio) {
    RenderSizing sizing;

    double scale = scaleFactor;
    if (scale < kMinScale) {
        scale = kMinScale;
    }
    double dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;

    const double paddedWidth = (svgWidth > 0.0 ? std::ceil(svgWidth) : kDefaultWidth) + kPadding;
    const double paddedHeight = (svgHeight > 0.0 ? std::ceil(svgHeight) : kDefaultHeight) + kPadding;

    const double requestedWidth = paddedWidth * scale;
    const double requestedHeight = paddedHeight * scale;

    const double requestedWidthPixels = requestedWidth * dpr;
    const double requestedHeightPixels = requestedHeight * dpr;

    double clampScale = 1.0;

    // Dimension-based clamp to avoid exceeding texture/pixmap limits (account for devicePixelRatio)
    if (requestedWidthPixels > kMaxDimension || requestedHeightPixels > kMaxDimension) {
        const double dimScale = std::min(kMaxDimension / requestedWidthPixels, kMaxDimension / requestedHeightPixels);
        clampScale = std::min(clampScale, dimScale);
    }

    // Memory-based clamp using QImageReader allocation limit (in MB)
    const int allocLimitMb = QImageReader::allocationLimit();
    const qint64 maxBytes = allocLimitMb > 0 ? static_cast<qint64>(allocLimitMb) * 1024 * 1024 : 0;
    if (maxBytes > 0) {
        const double requestedBytes = requestedWidthPixels * requestedHeightPixels * 4.0; // RGBA
        if (requestedBytes > static_cast<double>(maxBytes)) {
            const double byteScale = std::sqrt(static_cast<double>(maxBytes) / requestedBytes);
            clampScale = std::min(clampScale, byteScale);
        }
    }

    if (clampScale < kMinClampScale) {
        sizing.error = QStringLiteral("Requested render size %.0fx%.0f (scale %.2f, dpr %.2f) exceeds safe limits; reduce the resolution scale.")
                           .arg(std::ceil(requestedWidthPixels))
                           .arg(std::ceil(requestedHeightPixels))
                           .arg(scaleFactor, 0, 'f', 2)
                           .arg(dpr, 0, 'f', 2);
        return sizing;
    }

    sizing.effectiveScale = scale * clampScale;
    sizing.viewWidth = static_cast<int>(std::ceil(paddedWidth * sizing.effectiveScale));
    sizing.viewHeight = static_cast<int>(std::ceil(paddedHeight * sizing.effectiveScale));

    const double maxViewWidth = static_cast<double>(kMaxDimension) / dpr;
    const double maxViewHeight = static_cast<double>(kMaxDimension) / dpr;
    if (sizing.viewWidth > maxViewWidth) sizing.viewWidth = static_cast<int>(std::floor(maxViewWidth));
    if (sizing.viewHeight > maxViewHeight) sizing.viewHeight = static_cast<int>(std::floor(maxViewHeight));

    sizing.clamped = std::abs(sizing.effectiveScale - scale) > 1e-6
                     || sizing.viewWidth < static_cast<int>(std::ceil(requestedWidth))
                     || sizing.viewHeight < static_cast<int>(std::ceil(requestedHeight));
    if (sizing.clamped) {
        sizing.detail = QStringLiteral("Scale %.2f clamped to %.2f; render size %1x%2 (dpr %.2f)")
                            .arg(scaleFactor, 0, 'f', 2)
                            .arg(sizing.effectiveScale, 0, 'f', 2)
                            .arg(sizing.viewWidth)
                            .arg(sizing.viewHeight)
                            .arg(dpr, 0, 'f', 2);
    }

    return sizing;
}

void MermaidRenderService::renderOnMainThread(const QString& mermaidCode, const QString& outputPath, double scaleFactor, RenderResult* result) {
    if (!result) {
        qWarning() << "MermaidRenderService::renderOnMainThread called with null result pointer";
        return;
    }

    *result = RenderResult{};
    if (scaleFactor < 0.1) {
        scaleFactor = 0.1;
    }

    ensureProfileInitialized();

    const QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.dir();
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        result->error = QStringLiteral("Could not create output directory: %1").arg(outputPath);
        return;
    }

    const QString templateResource = QStringLiteral(":/mermaid/template.html");
    const QString jsResource = QStringLiteral(":/mermaid/mermaid.min.js");

    QFile templateFile(templateResource);
    QString templateHtml;
    if (templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        templateHtml = QString::fromUtf8(templateFile.readAll());
        templateFile.close();
    } else {
        result->error = QStringLiteral("Could not read Mermaid template HTML");
        return;
    }

    QFile libFile(jsResource);
    QString mermaidLibJs;
    if (libFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        mermaidLibJs = QString::fromUtf8(libFile.readAll());
        libFile.close();
    } else {
        result->error = QStringLiteral("Could not read mermaid library script");
        return;
    }

    const QString inlineScriptTag = QStringLiteral("<script>%1</script>").arg(mermaidLibJs);
    const QStringList scriptTagsToReplace = {
        QStringLiteral("<script src=\"mermaid.min.js\"></script>"),
        QStringLiteral("<script src=\"https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js\"></script>")
    };
    bool scriptReplaced = false;
    for (const auto& tag : scriptTagsToReplace) {
        if (templateHtml.contains(tag)) {
            templateHtml.replace(tag, inlineScriptTag);
            scriptReplaced = true;
        }
    }
    if (!scriptReplaced) {
        templateHtml.prepend(inlineScriptTag);
    }

    const QString codePath = outputDir.filePath(QStringLiteral("mermaid_input.mmd"));
    {
        QFile codeFile(codePath);
        if (codeFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            const QByteArray utf8 = mermaidCode.toUtf8();
            if (codeFile.write(utf8) != utf8.size()) {
                result->error = QStringLiteral("Failed to write Mermaid input to %1").arg(codePath);
                return;
            }
            codeFile.close();
        } else {
            result->error = QStringLiteral("Could not open %1 for writing").arg(codePath);
            return;
        }
    }

    const QString artifactPath = outputDir.filePath(QStringLiteral("mermaid_debug.html"));

    QWebEngineView view;
    view.setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* page = view.page();

    const QString inlineInputPath = QUrl::fromLocalFile(codePath).toString(QUrl::FullyEncoded).replace(QStringLiteral("'"), QStringLiteral("\\'"));

    const QString fullHtml = templateHtml + QStringLiteral(R"(
<!-- Injected by MermaidRenderService -->
<script>window.__mermaidInputPath='%1';</script>
)")
                                         .arg(inlineInputPath);

    {
        QFile artifactFile(artifactPath);
        if (artifactFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            artifactFile.write(fullHtml.toUtf8());
            artifactFile.close();
        } else {
            result->error = QStringLiteral("Failed to write artifact %1: %2").arg(artifactPath, artifactFile.errorString());
            return;
        }
    }

    QObject::connect(page, &QWebEnginePage::renderProcessTerminated, &view, [result](QWebEnginePage::RenderProcessTerminationStatus status, int code) {
        if (!result->ok) {
            result->error = QStringLiteral("Render process terminated (%1) code %2").arg(static_cast<int>(status)).arg(code);
        }
    });

    QObject::connect(page, &QWebEnginePage::loadingChanged, &view, [result](const QWebEngineLoadingInfo& info) {
        const bool httpOk = info.errorDomain() == QWebEngineLoadingInfo::HttpStatusCodeDomain && info.errorCode() == 200;
        const bool isFailure = info.status() == QWebEngineLoadingInfo::LoadFailedStatus && !httpOk;
        if (isFailure && result->error.isEmpty()) {
            result->error = QStringLiteral("Load failed: %1 (%2:%3)")
                                .arg(info.errorString())
                                .arg(static_cast<int>(info.errorDomain()))
                                .arg(static_cast<int>(info.errorCode()));
        }
    });

    QEventLoop loop;
    bool loadSuccess = false;
    QObject::connect(page, &QWebEnginePage::loadFinished, &loop, [&loop, &loadSuccess](bool ok) {
        loadSuccess = ok;
        loop.quit();
    });

    const QUrl artifactUrl = QUrl::fromLocalFile(artifactPath);
    view.load(artifactUrl);
    loop.exec();

    if (!loadSuccess) {
        if (result->error.isEmpty()) {
            result->error = QStringLiteral("Failed to load artifact %1").arg(artifactPath);
        }
        return;
    }

    const QString startRenderJs = QStringLiteral(
        "(() => {"
        "  const inputPath = window.__mermaidInputPath;"
        "  const container = document.getElementById('mermaid-container');"
        "  const meta = { mermaidType: typeof mermaid, hasContainer: !!container, inputPath: inputPath };"
        "  window.__mermaidRenderResult = null;"
        "  const fail = (msg) => { window.__mermaidRenderResult = { ok: false, error: msg, ...meta }; return 'fail'; };"
        "  if (typeof mermaid === 'undefined') { return fail('FATAL: mermaid object is undefined. Library did not load.'); }"
        "  if (!container) { return fail('mermaid container not found'); }"
        "  const run = async () => {"
        "    try {"
        "      const resp = await fetch(inputPath);"
        "      if (!resp || !resp.ok) { return fail('fetch failed with status ' + (resp ? resp.status : 'unknown')); }"
        "      const code = (await resp.text()).trim();"
        "      if (!code) { return fail('mermaid code is empty after fetch'); }"
        "      if (!window.__mermaidInitialized) { mermaid.initialize({ startOnLoad: false, securityLevel: 'loose' }); window.__mermaidInitialized = true; }"
        "      const renderResult = await mermaid.render('rendered-mermaid', code, container);"
        "      container.innerHTML = renderResult && renderResult.svg ? renderResult.svg : '';"
        "      const svg = container.querySelector('svg');"
        "      const bbox = svg && svg.getBBox ? svg.getBBox() : null;"
        "      window.__mermaidRenderResult = { ok: !!svg, error: svg ? null : 'no svg generated', width: bbox ? bbox.width : null, height: bbox ? bbox.height : null, svgPresent: !!svg, codeLength: code.length, ...meta };"
        "      return window.__mermaidRenderResult.ok ? 'render-succeeded' : 'render-no-svg';"
        "    } catch (e) { return fail('JS Exception: ' + (e ? (e.message || e.toString()) : 'Unknown error')); }"
        "  };"
        "  run();"
        "  return 'render-started';"
        "})()"
    );

    view.page()->runJavaScript(startRenderJs);

    QVariantMap renderResult;
    QEventLoop renderLoop;
    QTimer renderTimeout;
    QTimer pollTimer;
    renderTimeout.setSingleShot(true);
    renderTimeout.setInterval(10000);

    auto pollResult = [&renderResult, &renderLoop, &renderTimeout, page]() {
        page->runJavaScript(QStringLiteral("window.__mermaidRenderResult"), [&renderResult, &renderLoop, &renderTimeout](const QVariant& value) {
            const QVariantMap candidate = value.toMap();
            if (!candidate.isEmpty()) {
                renderResult = candidate;
                renderTimeout.stop();
                renderLoop.quit();
            }
        });
    };

    QObject::connect(&renderTimeout, &QTimer::timeout, &renderLoop, [&renderLoop]() {
        renderLoop.quit();
    });
    QObject::connect(&pollTimer, &QTimer::timeout, &renderLoop, [&pollResult]() {
        pollResult();
    });

    renderTimeout.start();
    pollTimer.start(150);
    pollResult();
    renderLoop.exec();
    pollTimer.stop();

    if (renderResult.isEmpty()) {
        result->error = QStringLiteral("Mermaid render script did not return a result");
        return;
    }

    const bool renderOk = renderResult.value(QStringLiteral("ok")).toBool();
    if (!renderOk) {
        const QString error = renderResult.value(QStringLiteral("error")).toString();
        result->error = error.isEmpty() ? QStringLiteral("Mermaid render failed") : error;
        return;
    }

    const double svgWidth = renderResult.value(QStringLiteral("width")).toDouble();
    const double svgHeight = renderResult.value(QStringLiteral("height")).toDouble();

    const qreal dpr = view.devicePixelRatio();
    const RenderSizing sizing = planRenderSizing(svgWidth, svgHeight, scaleFactor, dpr);
    if (!sizing.error.isEmpty()) {
        result->error = sizing.error;
        return;
    }

    if (sizing.clamped && sizing.detail.isEmpty()) {
        result->detail = QStringLiteral("Scale adjusted due to size limits.");
    } else if (sizing.clamped) {
        result->detail = sizing.detail;
    }

    page->setZoomFactor(sizing.effectiveScale);
    view.resize(sizing.viewWidth, sizing.viewHeight);
    view.show();

    QEventLoop waitLoop;
    const int waitMs = (sizing.viewWidth > 8000 || sizing.viewHeight > 8000) ? 400 : 200;
    QTimer::singleShot(waitMs, &waitLoop, &QEventLoop::quit);
    waitLoop.exec();

    const QPixmap shot = view.grab();
    if (shot.isNull()) {
        result->error = QStringLiteral("Failed to capture Mermaid image (empty pixmap)");
        return;
    }

    const bool saved = shot.save(outputPath);
    if (!saved) {
        result->error = QStringLiteral("Failed to save Mermaid image to %1").arg(outputPath);
        return;
    }

    const QString summary = QStringLiteral("Rendered %1x%2 (zoom %3)")
                                .arg(sizing.viewWidth)
                                .arg(sizing.viewHeight)
                                .arg(sizing.effectiveScale, 0, 'f', 2);
    if (!result->detail.isEmpty()) {
        result->detail.append(QStringLiteral("; ")).append(summary);
    } else {
        result->detail = summary;
    }

    result->ok = true;
}
