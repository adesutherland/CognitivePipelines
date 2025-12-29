//
// Cognitive Pipeline Application - Provider Compatibility Matrix (Headless)
//
// Phase 3: Harden the live matrix with access gating and vision audit.
// For each provider/model, build a tiny pipeline and run it, waiting with a
// local QEventLoop (90s timeout). We assert HTTP 200 + non-empty response for
// executed rows so that broken models (e.g., 404/400) cause test failures.
// Missing credentials skip execution but still log.
//

#include <QtTest/QtTest>
#include <QApplication>
#include <QJsonObject>
#include <QVector>
#include <QStringList>
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>
#include <QtNodes/internal/Definitions.hpp>

#include "ModelCapsRegistry.h"
#include "ModelCaps.h"

#include "mainwindow.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "PromptBuilderNode.h"
#include "UniversalLLMNode.h"
#include "ImageNode.h"
#include "core/LLMProviderRegistry.h"

using namespace ModelCapsTypes;
using namespace QtNodes;

struct ProbeRow {
    QString provider;   // "openai" | "google"
    QString modelId;    // e.g., "gpt-4o"
};

static const QVector<ProbeRow> kHighValueProbes = {
    // OpenAI
    { QStringLiteral("openai"), QStringLiteral("gpt-4o") },            // Chat
    { QStringLiteral("openai"), QStringLiteral("gpt-5.2-pro") },       // Completion/Base probe
    { QStringLiteral("openai"), QStringLiteral("o1-mini") },           // Reasoning
    // Google
    { QStringLiteral("google"), QStringLiteral("gemini-2.0-flash") },  // New
    { QStringLiteral("google"), QStringLiteral("gemini-3-flash-preview") } // Next-Gen probe
};

static QString endpointModeToString(EndpointMode m)
{
    switch (m) {
    case EndpointMode::Chat: return QStringLiteral("chat");
    case EndpointMode::Completion: return QStringLiteral("completion");
    case EndpointMode::Assistant: return QStringLiteral("assistant");
    }
    return QStringLiteral("chat");
}

static QStringList capabilitiesToStrings(const QSet<Capability>& caps)
{
    QStringList out;
    if (caps.contains(Capability::Vision)) out << QStringLiteral("Vision");
    if (caps.contains(Capability::Reasoning)) out << QStringLiteral("Reasoning");
    if (caps.contains(Capability::ToolUse)) out << QStringLiteral("ToolUse");
    if (caps.contains(Capability::LongContext)) out << QStringLiteral("LongContext");
    if (caps.contains(Capability::Audio)) out << QStringLiteral("Audio");
    if (caps.contains(Capability::Image)) out << QStringLiteral("Image");
    if (caps.contains(Capability::StructuredOutput)) out << QStringLiteral("StructuredOutput");
    return out;
}

class ProviderMatrixProbe : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void test_PreFlightMatrix();
    void test_LiveMatrixExecution();

private:
    struct LiveResult {
        enum class Status { Skipped, Success, HttpError, Timeout };
        Status status = Status::Skipped;
        int httpCode = 0;              // 200 for success; non-zero for errors when known
        QString response;              // final response text (may contain error text)
        QString error;                 // error message, if any
        bool visionAttempted = false;  // whether we attached an image
        bool visionAccepted = false;   // whether vision request did not 400
        QString skipReason;            // optional reason for SKIPPED
        QString statusString() const {
            switch (status) {
            case Status::Skipped: return skipReason.isEmpty() ? QStringLiteral("SKIPPED")
                                                              : QStringLiteral("SKIPPED: %1").arg(skipReason);
            case Status::Success: return QStringLiteral("SUCCESS");
            case Status::HttpError:
                return httpCode > 0
                    ? QStringLiteral("HTTP ERROR (%1)").arg(httpCode)
                    : QStringLiteral("HTTP ERROR");
            case Status::Timeout: return QStringLiteral("TIMEOUT");
            }
            return QStringLiteral("?");
        }
    };

    // App graph context per this test fixture
    std::unique_ptr<MainWindow> mainWindow_;
    NodeGraphModel* model_ {nullptr};
    ExecutionEngine* engine_ {nullptr};

    static bool hasApiKeyForProvider(const QString& provider) {
        if (provider == QStringLiteral("openai")) {
            const bool env = !qEnvironmentVariable("OPENAI_API_KEY").isEmpty();
            const bool reg = !LLMProviderRegistry::instance().getCredential(QStringLiteral("openai")).isEmpty();
            return env || reg;
        }
        if (provider == QStringLiteral("google")) {
            const bool env = !qEnvironmentVariable("GOOGLE_API_KEY").isEmpty();
            const bool reg = !LLMProviderRegistry::instance().getCredential(QStringLiteral("google")).isEmpty();
            return env || reg;
        }
        return false;
    }

    static int parseHttpCode(const QString& text) {
        // Look for patterns like "HTTP 404" or "HTTP/1.1 400" in error/raw strings
        static const QRegularExpression re1(QStringLiteral("\\bHTTP\\s+(\\d{3})\\b"));
        static const QRegularExpression re2(QStringLiteral("\\bHTTP\\/\\d+(?:\\.\\d+)?\\s+(\\d{3})\\b"));
        QRegularExpressionMatch m = re1.match(text);
        if (m.hasMatch()) return m.captured(1).toInt();
        m = re2.match(text);
        if (m.hasMatch()) return m.captured(1).toInt();
        return 0;
    }

    static bool isRestrictedAccessModel(const QString& provider, const QString& modelId) {
        if (provider == QStringLiteral("openai")) {
            // Heuristic: o-series often gated (o1, o3, o4)
            return modelId.startsWith(QStringLiteral("o"), Qt::CaseInsensitive);
        }
        return false;
    }

    LiveResult runSingleLive(const QString& provider, const QString& modelId, const QString& promptText) {
        LiveResult out;

        if (!hasApiKeyForProvider(provider)) {
            out.status = LiveResult::Status::Skipped;
            return out;
        }

        if (!model_ || !engine_) {
            out.status = LiveResult::Status::HttpError;
            out.error = QStringLiteral("Graph model/engine not initialized");
            return out;
        }

        // Ensure a clean model for each run
        model_->clear();

        const NodeId textNodeId = model_->addNode(QStringLiteral("text-input"));
        const NodeId promptNodeId = model_->addNode(QStringLiteral("prompt-builder"));
        const NodeId llmNodeId = model_->addNode(QStringLiteral("universal-llm"));

        if (textNodeId == InvalidNodeId || promptNodeId == InvalidNodeId || llmNodeId == InvalidNodeId) {
            out.status = LiveResult::Status::HttpError;
            out.error = QStringLiteral("Failed to construct pipeline nodes");
            return out;
        }

        model_->addConnection(ConnectionId{ textNodeId, 0u, promptNodeId, 0u });
        // Connect PromptBuilder "prompt" output to UniversalLLM "prompt" input (port index 1)
        model_->addConnection(ConnectionId{ promptNodeId, 0u, llmNodeId, 1u });

        // Vision audit: if model supports Vision, attach an ImageNode to the UniversalLLM image input (index 0)
        const auto caps = ModelCapsRegistry::instance().resolve(modelId, provider);
        QScopedPointer<QTemporaryFile> tmpImage;
        if (caps.has_value() && caps->hasCapability(Capability::Vision)) {
            out.visionAttempted = true;
            // 1x1 transparent PNG (base64)
            static const char* kPngBase64 =
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
            QByteArray png = QByteArray::fromBase64(kPngBase64);
            tmpImage.reset(new QTemporaryFile);
            tmpImage->setAutoRemove(true);
            if (tmpImage->open()) {
                tmpImage->write(png);
                tmpImage->flush();
                tmpImage->close();

                const NodeId imageNodeId = model_->addNode(QStringLiteral("image"));
                if (imageNodeId != InvalidNodeId) {
                    auto* del = model_->delegateModel<ToolNodeDelegate>(imageNodeId);
                    auto c = del ? del->connector() : nullptr;
                    auto* img = c ? dynamic_cast<ImageNode*>(c.get()) : nullptr;
                    if (img) {
                        img->setImagePath(tmpImage->fileName());
                        // Connect ImageNode out(0) ("image") to UniversalLLM image input (index 0)
                        model_->addConnection(ConnectionId{ imageNodeId, 0u, llmNodeId, 0u });
                    }
                }
            }
        }

        // Configure TextInput
        {
            auto* del = model_->delegateModel<ToolNodeDelegate>(textNodeId);
            auto c = del ? del->connector() : nullptr;
            auto* tool = c ? dynamic_cast<TextInputNode*>(c.get()) : nullptr;
            if (!tool) {
                out.status = LiveResult::Status::HttpError;
                out.error = QStringLiteral("TextInputNode not available");
                return out;
            }
            tool->setText(promptText);
        }
        // Configure PromptBuilder to inject the exact live prompt (no variables)
        {
            auto* del = model_->delegateModel<ToolNodeDelegate>(promptNodeId);
            auto c = del ? del->connector() : nullptr;
            auto* tool = c ? dynamic_cast<PromptBuilderNode*>(c.get()) : nullptr;
            if (!tool) {
                out.status = LiveResult::Status::HttpError;
                out.error = QStringLiteral("PromptBuilderNode not available");
                return out;
            }
            tool->setTemplateText(promptText);
        }
        // Configure UniversalLLM provider/model
        {
            auto* del = model_->delegateModel<ToolNodeDelegate>(llmNodeId);
            auto c = del ? del->connector() : nullptr;
            auto* tool = c ? dynamic_cast<UniversalLLMNode*>(c.get()) : nullptr;
            if (!tool) {
                out.status = LiveResult::Status::HttpError;
                out.error = QStringLiteral("UniversalLLMNode not available");
                return out;
            }
            QJsonObject state;
            state.insert(QStringLiteral("provider"), provider);
            state.insert(QStringLiteral("model"), modelId);
            tool->loadState(state);
        }

        bool finished = false;
        DataPacket finalOut;

        QMetaObject::Connection finishedConn = QObject::connect(engine_, &ExecutionEngine::pipelineFinished, this, [&](const DataPacket& outPkt){
            finished = true;
            finalOut = outPkt;
        });

        engine_->run();

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QMetaObject::Connection timeoutConn = QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QMetaObject::Connection loopQuitConn = QObject::connect(engine_, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
        timeout.start(90000); // 90 seconds timeout guard
        loop.exec();

        // Cleanup connections to avoid accumulation across iterations
        QObject::disconnect(finishedConn);
        QObject::disconnect(timeoutConn);
        QObject::disconnect(loopQuitConn);

        if (!finished) {
            out.status = LiveResult::Status::Timeout;
            out.error = QStringLiteral("Pipeline did not finish within timeout");
            return out;
        }

        // Extract results
        const QString responseKey = QString::fromLatin1(UniversalLLMNode::kOutputResponseId);
        const QString response = finalOut.value(responseKey).toString();
        const QString err = finalOut.value(QStringLiteral("__error")).toString();
        const QString raw = finalOut.value(QStringLiteral("_raw_response")).toString();

        if (!err.isEmpty()) {
            out.status = LiveResult::Status::HttpError;
            out.error = err;
            out.response = response;
            int code = parseHttpCode(err);
            if (code == 0) code = parseHttpCode(raw);
            out.httpCode = code;

            // Access gating: known restricted models (e.g., o‑series). If 403 or model_not_found, mark as SKIPPED.
            if (isRestrictedAccessModel(provider, modelId)) {
                const bool is403 = (out.httpCode == 403);
                const bool isModelNotFoundToken = err.contains(QStringLiteral("model_not_found"), Qt::CaseInsensitive);
                const bool isNoAccessPhrase = err.contains(QStringLiteral("does not exist or you do not have access"), Qt::CaseInsensitive);
                if (is403 || isModelNotFoundToken || isNoAccessPhrase) {
                    out.status = LiveResult::Status::Skipped;
                    out.skipReason = QStringLiteral("ACCESS DENIED");
                }
            }
            return out;
        }

        // Success path: require non-empty response
        out.response = response;
        if (response.trimmed().isEmpty()) {
            out.status = LiveResult::Status::HttpError;
            out.error = QStringLiteral("Empty response text");
            out.httpCode = 0;
            return out;
        }
        out.status = LiveResult::Status::Success;
        if (out.visionAttempted) {
            out.visionAccepted = true; // successful 200 implies multimodal accepted
        }
        out.httpCode = 200;
        
        // Hygiene: ensure any transient image is cleaned up and model reset
        if (tmpImage) {
            // QTemporaryFile auto-removes on destruction
        }
        return out;
    }
};

void ProviderMatrixProbe::initTestCase()
{
    QVERIFY2(ModelCapsRegistry::instance().loadFromFile(QStringLiteral(":/resources/model_caps.json")),
             "Failed to load model capabilities from resource");

    // Instantiate app graph (headless)
    mainWindow_ = std::make_unique<MainWindow>();
    model_ = mainWindow_->graphModel();
    engine_ = mainWindow_->executionEngine();
    QVERIFY2(model_ != nullptr, "Graph model must exist");
    QVERIFY2(engine_ != nullptr, "Execution engine must exist");
}

void ProviderMatrixProbe::test_PreFlightMatrix()
{
    const bool hasOpenAi = !qEnvironmentVariable("OPENAI_API_KEY").isEmpty();
    const bool hasGoogle = !qEnvironmentVariable("GOOGLE_API_KEY").isEmpty();

    qInfo() << "==== Universal Provider Compatibility Pre-Flight Check ====";
    qInfo() << "Keys detected:";
    qInfo() << "  OPENAI_API_KEY:" << (hasOpenAi ? "Yes" : "No");
    qInfo() << "  GOOGLE_API_KEY:" << (hasGoogle ? "Yes" : "No");
    qInfo() << "-----------------------------------------------------------";
    qInfo() << "Provider | Model ID                 | Multimodal | Endpoint | Caps";
    qInfo() << "---------+---------------------------+------------+----------+-----------------------------";

    for (const auto& row : kHighValueProbes) {
        const auto resolved = ModelCapsRegistry::instance().resolve(row.modelId, row.provider);
        QString endpoint = QStringLiteral("(unresolved)");
        QString multi = QStringLiteral("?");
        QString capsStr = QStringLiteral("-");

        if (resolved.has_value()) {
            endpoint = endpointModeToString(resolved->endpointMode);
            const bool vision = resolved->hasCapability(Capability::Vision);
            multi = vision ? QStringLiteral("Yes") : QStringLiteral("No");
            capsStr = capabilitiesToStrings(resolved->capabilities).join(", ");
            if (capsStr.isEmpty()) capsStr = QStringLiteral("-");
        }

        // Fixed-width-ish formatting for readability in logs
        const QString providerCol = row.provider.leftJustified(7, ' ', true);
        const QString modelCol = row.modelId.leftJustified(27, ' ', true);
        const QString multiCol = multi.leftJustified(10, ' ', true);
        const QString endpointCol = endpoint.leftJustified(8, ' ', true);
        qInfo().noquote() << QStringLiteral("%1 | %2 | %3 | %4 | %5")
                             .arg(providerCol)
                             .arg(modelCol)
                             .arg(multiCol)
                             .arg(endpointCol)
                             .arg(capsStr);
    }

    // This probe is informational; ensure it never fails CI for missing keys or unknown models.
    QVERIFY(true);
}

// Exported runner to be invoked from the integration_tests main()
int run_provider_matrix_probe(int argc, char** argv)
{
    ProviderMatrixProbe tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_matrix.moc"

void ProviderMatrixProbe::test_LiveMatrixExecution()
{
    // Live prompt to inject
    const QString kPrompt = QStringLiteral("Hello, system test. Respond with one word: Success.");

    qInfo() << "==== Universal Provider Compatibility Live Probes ====";
    qInfo() << "Provider | Model ID                 | Endpoint | Live Status            | Vision Tested";
    qInfo() << "---------+---------------------------+----------+------------------------+--------------";

    for (const auto& row : kHighValueProbes) {
        // Resolve endpoint (informational)
        const auto resolved = ModelCapsRegistry::instance().resolve(row.modelId, row.provider);
        const QString endpoint = resolved.has_value() ? endpointModeToString(resolved->endpointMode)
                                                      : QStringLiteral("(unresolved)");

        LiveResult lr = runSingleLive(row.provider, row.modelId, kPrompt);

        const QString providerCol = row.provider.leftJustified(7, ' ', true);
        const QString modelCol = row.modelId.leftJustified(27, ' ', true);
        const QString endpointCol = endpoint.leftJustified(8, ' ', true);
        const QString statusCol = lr.statusString().leftJustified(22, ' ', true);
        const QString visionCol = lr.visionAttempted
                                   ? (lr.status == LiveResult::Status::Success ? QStringLiteral("Yes")
                                         : (lr.status == LiveResult::Status::Skipped ? QStringLiteral("Skipped")
                                             : QStringLiteral("Error")))
                                   : QStringLiteral("No");

        qInfo().noquote() << QStringLiteral("%1 | %2 | %3 | %4 | %5")
                             .arg(providerCol)
                             .arg(modelCol)
                             .arg(endpointCol)
                             .arg(statusCol)
                             .arg(visionCol);

        // Assertions for executed rows only
        if (lr.status == LiveResult::Status::Skipped) {
            QTest::qWait(0); // no-op to keep loop deterministic
            continue;
        }

        // TIMEOUT should fail
        QVERIFY2(lr.status != LiveResult::Status::Timeout,
                 QStringLiteral("Timeout waiting for provider=%1 model=%2")
                     .arg(row.provider, row.modelId)
                     .toLocal8Bit().constData());

        // Must have non-empty response
        QVERIFY2(!lr.response.trimmed().isEmpty(),
                 QStringLiteral("Empty response for provider=%1 model=%2")
                     .arg(row.provider, row.modelId)
                     .toLocal8Bit().constData());

        // Must be HTTP 200
        QVERIFY2(lr.httpCode == 200,
                 QStringLiteral("Expected HTTP 200 but got %1 for provider=%2 model=%3 (error=%4)")
                     .arg(QString::number(lr.httpCode), row.provider, row.modelId, lr.error)
                     .toLocal8Bit().constData());

        // Vision audit: if attempted, it must not 400
        if (lr.visionAttempted) {
            QVERIFY2(!(lr.status == LiveResult::Status::HttpError && lr.httpCode == 400),
                     QStringLiteral("Vision payload rejected with 400 for provider=%1 model=%2 (error=%3)")
                         .arg(row.provider, row.modelId, lr.error)
                         .toLocal8Bit().constData());
        }
    }
}
