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
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QScreen>
#include <QMetaObject>
#include <QPixmap>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kDefaultWidth = 1024;
constexpr int kDefaultHeight = 768;
constexpr int kPadding = 32;
constexpr int kMaxDimension = 16384; // conservative cap to avoid texture/pixmap limits
constexpr double kMinScale = 0.1;
constexpr double kMinClampScale = 0.01; // below this, fail fast instead of attempting a huge render
constexpr int kTileMemoryBudgetMb = 256; // soft tile memory budget to avoid Chromium tile truncation
constexpr int kTargetAllocationLimitMb = 1024; // raise allocation cap to tolerate large but bounded renders

QString normalizeCachePath(const QString& path) {
    const QFileInfo info(path);
    const QString base = info.fileName();
    const QString parent = info.dir().dirName();
    if (!path.isEmpty() && !base.isEmpty() && base == parent) {
        return info.dir().absolutePath();
    }
    return path;
}

QString formatClampDetail(double requestedScale,
                          double effectiveScale,
                          const QString& reason,
                          int viewWidth,
                          int viewHeight,
                          double devicePixelRatio)
{
    return QStringLiteral("Scale %1 clamped to %2 for %3 limit; render size %4x%5 (dpr %6)")
        .arg(requestedScale, 0, 'f', 2)
        .arg(effectiveScale, 0, 'f', 2)
        .arg(reason)
        .arg(viewWidth)
        .arg(viewHeight)
        .arg(devicePixelRatio, 0, 'f', 2);
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
    const double requestedBytes = requestedWidthPixels * requestedHeightPixels * 4.0; // RGBA

    double clampScale = 1.0;

    // Dimension-based clamp to avoid exceeding texture/pixmap limits (account for devicePixelRatio)
    if (requestedWidthPixels > kMaxDimension || requestedHeightPixels > kMaxDimension) {
        const double dimScale = std::min(kMaxDimension / requestedWidthPixels, kMaxDimension / requestedHeightPixels);
        clampScale = std::min(clampScale, dimScale);
    }

    // Tile memory budget clamp to avoid Chromium tile manager failures
    const qint64 tileBudgetBytes = static_cast<qint64>(kTileMemoryBudgetMb) * 1024 * 1024;
    if (tileBudgetBytes > 0 && requestedBytes > static_cast<double>(tileBudgetBytes)) {
        const double tileScale = std::sqrt(static_cast<double>(tileBudgetBytes) / requestedBytes);
        clampScale = std::min(clampScale, tileScale);
    }

    // Memory-based clamp using QImageReader allocation limit (in MB)
    const int allocLimitMb = QImageReader::allocationLimit();
    const qint64 maxBytes = allocLimitMb > 0 ? static_cast<qint64>(allocLimitMb) * 1024 * 1024 : 0;
    if (maxBytes > 0) {
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
        sizing.detail = QStringLiteral("Scale %1 clamped to %2; render size %3x%4 (dpr %5)")
                            .arg(scaleFactor, 0, 'f', 2)
                            .arg(sizing.effectiveScale, 0, 'f', 2)
                            .arg(sizing.viewWidth)
                            .arg(sizing.viewHeight)
                            .arg(dpr, 0, 'f', 2);
    }

    return sizing;
}

QString MermaidRenderService::formatClampDetail(double requestedScale,
                                                double effectiveScale,
                                                const QString& reason,
                                                int viewWidth,
                                                int viewHeight,
                                                double devicePixelRatio)
{
    return ::formatClampDetail(requestedScale, effectiveScale, reason, viewWidth, viewHeight, devicePixelRatio);
}

void MermaidRenderService::renderOnMainThread(const QString& mermaidCode, const QString& outputPath, double scaleFactor, RenderResult* result) {
    if (!result) {
        qWarning() << "MermaidRenderService::renderOnMainThread called with null result pointer";
        return;
    }

    *result = RenderResult{};
    result->requestedScale = scaleFactor;
    result->effectiveScale = scaleFactor;
    if (scaleFactor < 0.1) {
        scaleFactor = 0.1;
    }

    // Increase the global image allocation limit so large-but-bounded renders can be read back
    // (still subject to explicit clamping checks below).
    static bool allocationLimitRaised = false;
    if (!allocationLimitRaised) {
        const int currentLimit = QImageReader::allocationLimit();
        const int targetLimit = std::max(currentLimit, kTargetAllocationLimitMb);
        QImageReader::setAllocationLimit(targetLimit);
        allocationLimitRaised = true;
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

    const QString enforcedCss = QStringLiteral(
        "<style>"
        "html, body { margin: 0; padding: 0; overflow: hidden !important; }"
        "#mermaid-container { display: block; margin: 0; padding: 0; }"
        "</style>");
    const QString headCloseTag = QStringLiteral("</head>");
    if (templateHtml.contains(headCloseTag, Qt::CaseInsensitive)) {
        templateHtml.replace(headCloseTag, enforcedCss + headCloseTag, Qt::CaseInsensitive);
    } else {
        templateHtml.prepend(enforcedCss);
    }

    const QString runNonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString codePath = outputDir.filePath(QStringLiteral("mermaid_input_%1.mmd").arg(runNonce));
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

    const QString artifactPath = outputDir.filePath(QStringLiteral("mermaid_debug_%1.html").arg(runNonce));
    const QString logPrefix = QStringLiteral("[MermaidRender %1]").arg(runNonce);

    QWebEngineView view;
    view.setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* page = view.page();

    QUrl inputUrl = QUrl::fromLocalFile(codePath);
    inputUrl.setQuery(runNonce);
    const QString inlineInputPath = inputUrl.toString(QUrl::FullyEncoded).replace(QStringLiteral("'"), QStringLiteral("\\'"));

    const QString fullHtml = templateHtml + QStringLiteral(R"(
<!-- Injected by MermaidRenderService -->
<script>window.__mermaidInputPath='%1';</script>
<!-- Instrumentation note: initial HTML written before render; a post-render snapshot will overwrite this file. -->
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

    QUrl artifactUrl = QUrl::fromLocalFile(artifactPath);
    artifactUrl.setQuery(runNonce);
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
        "      const rect = svg && svg.getBoundingClientRect ? svg.getBoundingClientRect() : null;"
        "      const width = Math.max(bbox ? bbox.width : 0, rect ? rect.width : 0);"
        "      const height = Math.max(bbox ? bbox.height : 0, rect ? rect.height : 0);"
        "      const docEl = document.documentElement;"
        "      const body = document.body;"
        "      const bodyStyle = body && window.getComputedStyle ? window.getComputedStyle(body) : null;"
        "      const metrics = {"
        "        htmlScrollWidth: docEl ? docEl.scrollWidth : null,"
        "        htmlScrollHeight: docEl ? docEl.scrollHeight : null,"
        "        htmlClientWidth: docEl ? docEl.clientWidth : null,"
        "        htmlClientHeight: docEl ? docEl.clientHeight : null,"
        "        bodyScrollWidth: body ? body.scrollWidth : null,"
        "        bodyScrollHeight: body ? body.scrollHeight : null,"
        "        bodyClientWidth: body ? body.clientWidth : null,"
        "        bodyClientHeight: body ? body.clientHeight : null,"
        "        bodyMarginLeft: bodyStyle ? bodyStyle.marginLeft : null,"
        "        bodyMarginRight: bodyStyle ? bodyStyle.marginRight : null,"
        "        bodyOverflowX: bodyStyle ? bodyStyle.overflowX : null,"
        "        bodyOverflowY: bodyStyle ? bodyStyle.overflowY : null"
        "      };"
        "      if (!width || !height) { return fail('mermaid produced zero-sized svg'); }"
        "      window.__mermaidRenderResult = { ok: !!svg, error: svg ? null : 'no svg generated', width: width, height: height, bboxWidth: bbox ? bbox.width : null, bboxHeight: bbox ? bbox.height : null, rectWidth: rect ? rect.width : null, rectHeight: rect ? rect.height : null, svgPresent: !!svg, codeLength: code.length, ...metrics, ...meta };"
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
    const double bboxWidth = renderResult.value(QStringLiteral("bboxWidth")).toDouble();
    const double bboxHeight = renderResult.value(QStringLiteral("bboxHeight")).toDouble();
    const double rectWidth = renderResult.value(QStringLiteral("rectWidth")).toDouble();
    const double rectHeight = renderResult.value(QStringLiteral("rectHeight")).toDouble();

    if (svgWidth <= 0.0 || svgHeight <= 0.0) {
        result->error = QStringLiteral("Mermaid render returned zero size (bbox %1x%2, rect %3x%4)")
                            .arg(bboxWidth, 0, 'f', 2)
                            .arg(bboxHeight, 0, 'f', 2)
                            .arg(rectWidth, 0, 'f', 2)
                            .arg(rectHeight, 0, 'f', 2);
        return;
    }

    const qreal viewDpr = view.devicePixelRatioF();
    const qreal screenDpr = qApp && qApp->primaryScreen() ? qApp->primaryScreen()->devicePixelRatio() : 1.0;
    const qreal dpr = std::max(viewDpr, screenDpr);
    result->devicePixelRatio = dpr;

    QString postRenderHtml;
    {
        QEventLoop htmlLoop;
        QTimer htmlTimeout;
        htmlTimeout.setSingleShot(true);
        htmlTimeout.setInterval(2000);
        QObject::connect(&htmlTimeout, &QTimer::timeout, &htmlLoop, &QEventLoop::quit);
        page->toHtml([&postRenderHtml, &htmlLoop](const QString& html) {
            postRenderHtml = html;
            htmlLoop.quit();
        });
        htmlTimeout.start();
        htmlLoop.exec();
    }

    if (!postRenderHtml.isEmpty()) {
        QFile artifactFile(artifactPath);
        if (artifactFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            artifactFile.write(postRenderHtml.toUtf8());
            artifactFile.close();
        } else {
            qWarning() << logPrefix << "Failed to write post-render artifact" << artifactPath << artifactFile.errorString();
        }
    }

    RenderSizing sizing = planRenderSizing(svgWidth, svgHeight, scaleFactor, dpr);
    if (!sizing.error.isEmpty()) {
        result->error = sizing.error;
        return;
    }

    result->clamped = sizing.clamped;
    result->effectiveScale = sizing.effectiveScale;

    QStringList detailParts;
    if (sizing.clamped && sizing.detail.isEmpty()) {
        detailParts << QStringLiteral("Scale adjusted due to size limits.");
    } else if (sizing.clamped) {
        detailParts << sizing.detail;
    }

    const auto waitForResize = [&view, page](int targetWidth, int targetHeight) -> bool {
        if (targetWidth <= 0 || targetHeight <= 0) {
            return false;
        }

        QEventLoop waitLoop;
        QTimer pollTimer;
        QTimer timeoutTimer;
        bool matched = false;
        QElapsedTimer logClock;
        logClock.start();
        int lastJsWidth = -1;
        int lastJsHeight = -1;
        double lastJsDpr = -1.0;

        pollTimer.setSingleShot(false);
        pollTimer.setInterval(50);
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(2000);

        const auto check = [&]() {
            const int viewWidth = view.width();
            const int viewHeight = view.height();
            if (viewWidth <= 0 || viewHeight <= 0) {
                return;
            }

            static const QString script = QStringLiteral("(() => [window.innerWidth, window.innerHeight, window.devicePixelRatio])()");
            page->runJavaScript(script, [&, viewWidth, viewHeight](const QVariant& value) {
                const QVariantList dims = value.toList();
                if (dims.size() >= 2) {
                    const int jsWidth = dims.at(0).toInt();
                    const int jsHeight = dims.at(1).toInt();
                    const double jsDpr = dims.size() >= 3 ? dims.at(2).toDouble() : -1.0;
                    lastJsWidth = jsWidth;
                    lastJsHeight = jsHeight;
                    lastJsDpr = jsDpr;

                    const qreal zoomFactor = page->zoomFactor();
                    if (zoomFactor <= 0.0) {
                        return;
                    }

                    const int expectedJsWidth = qRound(static_cast<qreal>(viewWidth) / zoomFactor);
                    const int expectedJsHeight = qRound(static_cast<qreal>(viewHeight) / zoomFactor);
                    const auto withinTolerance = [](int a, int b) {
                        return std::abs(a - b) <= 2;
                    };

                    if (withinTolerance(jsWidth, expectedJsWidth) && withinTolerance(jsHeight, expectedJsHeight)) {
                        matched = true;
                        timeoutTimer.stop();
                        waitLoop.quit();
                        return;
                    }

                    if (logClock.elapsed() >= 1000) {
                        qWarning() << "Waiting for resize: JS says" << jsWidth << "x" << jsHeight
                                   << "DPR" << jsDpr
                                   << "Expected" << expectedJsWidth << "x" << expectedJsHeight
                                   << "(View:" << viewWidth << "x" << viewHeight << "Zoom:" << zoomFactor << ")";
                        logClock.restart();
                    }
                }
            });
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        };

        QObject::connect(&pollTimer, &QTimer::timeout, &waitLoop, check);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);

        timeoutTimer.start();
        pollTimer.start();
        check();
        waitLoop.exec();
        pollTimer.stop();
        timeoutTimer.stop();

        if (!matched) {
            if (lastJsWidth >= 0 && lastJsHeight >= 0) {
                qWarning() << "Timed out waiting for viewport resize after clamping. Last JS" << lastJsWidth << "x" << lastJsHeight
                           << "DPR" << lastJsDpr
                           << "View" << view.width() << "x" << view.height()
                           << "Zoom" << page->zoomFactor();
            }
            return false;
        }

        QElapsedTimer settleClock;
        settleClock.start();
        while (settleClock.elapsed() < 100) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }

        return true;
    };

    page->setZoomFactor(sizing.effectiveScale);
    view.resize(sizing.viewWidth, sizing.viewHeight);
    view.show();

    if (!waitForResize(sizing.viewWidth, sizing.viewHeight)) {
        result->error = QStringLiteral("Timed out waiting for viewport resize");
        return;
    }

    const auto grabAndEstimate = [&view](QPixmap& pix, qint64& pixelWidth, qint64& pixelHeight, qint64& estimatedBytes, qreal& pixDpr) {
        pix = view.grab();
        if (pix.isNull()) return false;
        pixDpr = pix.devicePixelRatio();
        pixelWidth = static_cast<qint64>(std::ceil(static_cast<double>(pix.width()) * pixDpr));
        pixelHeight = static_cast<qint64>(std::ceil(static_cast<double>(pix.height()) * pixDpr));
        estimatedBytes = pixelWidth * pixelHeight * 4; // RGBA
        return true;
    };

    QPixmap shot;
    qint64 shotPixelWidth = 0;
    qint64 shotPixelHeight = 0;
    qint64 estimatedBytes = 0;
    qreal shotDpr = dpr;

    if (!grabAndEstimate(shot, shotPixelWidth, shotPixelHeight, estimatedBytes, shotDpr)) {
        result->error = QStringLiteral("Failed to capture Mermaid image (empty pixmap)");
        return;
    }

    // If the actual grab DPR is higher than our sizing DPR, recompute sizing before enforcing limits.
    if (shotDpr > dpr + 1e-3) {
        sizing = planRenderSizing(svgWidth, svgHeight, sizing.effectiveScale, shotDpr);
        result->clamped = result->clamped || sizing.clamped;
        result->effectiveScale = sizing.effectiveScale;
        page->setZoomFactor(sizing.effectiveScale);
        view.resize(sizing.viewWidth, sizing.viewHeight);

        if (!waitForResize(sizing.viewWidth, sizing.viewHeight)) {
            result->error = QStringLiteral("Timed out waiting for viewport resize after DPR rescale");
            return;
        }

        if (!grabAndEstimate(shot, shotPixelWidth, shotPixelHeight, estimatedBytes, shotDpr)) {
            result->error = QStringLiteral("Failed to capture Mermaid image after DPR rescale");
            return;
        }
    }

    if (shot.isNull()) {
        result->error = QStringLiteral("Failed to capture Mermaid image (empty pixmap)");
        return;
    }

    const qint64 tileMaxBytes = static_cast<qint64>(kTileMemoryBudgetMb) * 1024 * 1024;
    const qint64 allocMaxBytes = static_cast<qint64>(QImageReader::allocationLimit()) * 1024 * 1024;

    auto downscaleForLimit = [&](qint64 byteLimit, const QString& label) -> bool {
        if (byteLimit <= 0 || estimatedBytes <= byteLimit) {
            return true;
        }

        const double byteClamp = std::sqrt(static_cast<double>(byteLimit) / static_cast<double>(estimatedBytes));
        const double targetScale = sizing.effectiveScale * byteClamp * 0.98; // small buffer below limit

        if (targetScale < kMinClampScale) {
            result->error = QStringLiteral("Render size %1x%2 at dpr %3 exceeds %4 limit (%5 MB); requested scale %6, applied %7")
                                .arg(shotPixelWidth)
                                .arg(shotPixelHeight)
                                .arg(shotDpr, 0, 'f', 2)
                                .arg(label)
                                .arg(byteLimit / (1024 * 1024))
                                .arg(scaleFactor, 0, 'f', 2)
                                .arg(sizing.effectiveScale, 0, 'f', 2);
            return false;
        }

        const RenderSizing retrySizing = planRenderSizing(svgWidth, svgHeight, targetScale, shotDpr);
        if (!retrySizing.error.isEmpty()) {
            result->error = retrySizing.error;
            return false;
        }

        sizing = retrySizing;
        result->clamped = true;
        result->effectiveScale = retrySizing.effectiveScale;
        detailParts << MermaidRenderService::formatClampDetail(scaleFactor,
                                                               retrySizing.effectiveScale,
                                                               label,
                                                               retrySizing.viewWidth,
                                                               retrySizing.viewHeight,
                                                               shotDpr);

        page->setZoomFactor(retrySizing.effectiveScale);
        view.resize(retrySizing.viewWidth, retrySizing.viewHeight);

        if (!waitForResize(retrySizing.viewWidth, retrySizing.viewHeight)) {
            result->error = QStringLiteral("Timed out waiting for viewport resize after clamping");
            return false;
        }

        if (!grabAndEstimate(shot, shotPixelWidth, shotPixelHeight, estimatedBytes, shotDpr)) {
            result->error = QStringLiteral("Failed to capture Mermaid image after downscaling for %1 limit").arg(label);
            return false;
        }

        if (estimatedBytes > byteLimit) {
            result->error = QStringLiteral("Render size %1x%2 at dpr %3 still exceeds %4 limit (%5 MB) after clamping; requested scale %6, applied %7")
                                .arg(shotPixelWidth)
                                .arg(shotPixelHeight)
                                .arg(shotDpr, 0, 'f', 2)
                                .arg(label)
                                .arg(byteLimit / (1024 * 1024))
                                .arg(scaleFactor, 0, 'f', 2)
                                .arg(retrySizing.effectiveScale, 0, 'f', 2);
            return false;
        }

        return true;
    };

    if (!downscaleForLimit(tileMaxBytes, QStringLiteral("tile memory"))) {
        return;
    }

    if (!downscaleForLimit(allocMaxBytes, QStringLiteral("allocation"))) {
        return;
    }

    result->devicePixelRatio = shotDpr;

    const bool saved = shot.save(outputPath);
    if (!saved) {
        result->error = QStringLiteral("Failed to save Mermaid image to %1").arg(outputPath);
        return;
    }

    const QString summary = QStringLiteral("Rendered %1x%2 (requested scale %3, applied %4, dpr %5)")
                                .arg(sizing.viewWidth)
                                .arg(sizing.viewHeight)
                                .arg(scaleFactor, 0, 'f', 2)
                                .arg(result->effectiveScale, 0, 'f', 2)
                                .arg(result->devicePixelRatio, 0, 'f', 2);

    detailParts << summary;
    result->detail = detailParts.join(QStringLiteral("; "));

    result->ok = true;
}
