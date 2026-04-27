# Model Catalog Configuration

CognitivePipelines ships a curated provider/model catalog in `resources/model_caps.json`.
At startup the app loads that file and then applies user overrides from:

- macOS: `~/Library/Application Support/CognitivePipelines/model_catalog.json`
- Other platforms: the Qt generic config location under `CognitivePipelines/model_catalog.json`
- Current working directory: `model_catalog.json`

The user file is intentionally partial. Add only the entries you want to replace,
add, or disable. Arrays are merged by `id`.

## Providers

Use `providers` to enable or disable visible backends, override display names,
set local endpoints, and provide optional headers. API keys are still best kept
in environment variables or `accounts.json`, but `api_key` is supported for
local/private gateways.

```json
{
  "providers": [
    {
      "id": "ollama",
      "enabled": true,
      "requiresCredential": false,
      "base_url": "http://127.0.0.1:11434"
    },
    {
      "id": "google",
      "enabled": false
    }
  ]
}
```

Ollama normally does not require a key. If a hosted or proxied Ollama endpoint
does, set `OLLAMA_API_KEY`, add an `ollama` entry to `accounts.json`, or add
`api_key`/`headers.Authorization` to `model_catalog.json`.

For CI or headless environments without a local Ollama daemon, set
`CP_DISABLE_OLLAMA=1`. The GitHub Actions workflow does this so test selectors
cannot accidentally probe `127.0.0.1:11434`.

## Driver Profiles

Driver profiles name the protocol family a model should use. Rules map model IDs
to a driver profile with `driver`.

```json
{
  "driver_profiles": [
    {
      "id": "openai-responses",
      "provider": "openai",
      "protocol": "chat",
      "endpoint": "/v1/responses"
    }
  ]
}
```

The current runtime understands chat, completion, assistant, embedding, and image
driver families. New profiles are useful for investigation even before a backend
implementation uses every field.

## Rules

Rules are regular expressions over provider model IDs. Higher `priority` wins.
Capabilities drive filtering in UI windows; driver profiles drive backend
selection hints and diagnostics.

```json
{
  "rules": [
    {
      "id": "my-new-frontier-model",
      "backend": "openai",
      "priority": 250,
      "pattern": "^gpt-6(?:$|[-.].*)",
      "driver": "openai-chat-completions",
      "role_mode": "developer",
      "capabilities": ["chat", "vision", "reasoning", "longcontext"],
      "parameter_constraints": {
        "omitTemperature": true,
        "tokenFieldName": "max_completion_tokens"
      }
    }
  ]
}
```

For broad provider-specific rules, add `"requires_backend": true` so the rule is
used only when the selector already knows the provider. This is useful for local
Ollama patterns that intentionally accept many model names.

To remove a shipped entry, add an object with the same `id` and `"disabled": true`.

```json
{
  "rules": [
    { "id": "openai-defaults", "disabled": true }
  ],
  "virtual_models": [
    { "id": "gpt-cost-optimised", "disabled": true }
  ]
}
```

## UI Investigation Flow

Provider/model selectors show recommended and available models by default. Enable
`Show filtered models` to inspect models hidden by capability or driver rules and
see the filter reason inline. Use `Test Selection` to run a small chat or
embedding call through the selected provider/model/driver path.
