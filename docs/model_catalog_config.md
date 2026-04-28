# Provider and Model Catalog Configuration Guide

CognitivePipelines uses two configuration layers for provider and model behavior:

- Distribution baseline: the shipped catalog compiled into the app as `:/resources/model_caps.json`. In the source tree this comes from `resources/model_caps.json`.
- User copy: a local `model_catalog.json` written only when you save provider/catalog changes from the app.

The baseline is read-only at runtime. The user copy is merged over the baseline by `id`, so app updates can still bring in new shipped models, drivers, and rules unless you override those same ids.

## File Locations

The distribution baseline is:

```text
:/resources/model_caps.json
```

For developers, edit this source file when changing the shipped defaults:

```text
resources/model_caps.json
```

The user copy is:

```text
macOS:   ~/Library/Application Support/CognitivePipelines/model_catalog.json
Linux:   ~/.config/CognitivePipelines/model_catalog.json
Windows: %LOCALAPPDATA%\CognitivePipelines\model_catalog.json
```

Use `Edit -> Manage Providers... -> Catalog Overrides` to see the exact path on the running machine. If the user copy does not exist, the app is using only the distribution baseline.

## Reset Behavior

`Reset to Distribution Baseline` removes the local user copy and reloads the bundled catalog. It does not edit `resources/model_caps.json` or any file inside the installed application bundle.

After reset:

- provider/model selectors use the shipped defaults
- copied or edited regex rules are gone
- local Ollama host overrides are gone
- API credentials in `accounts.json` are not changed

## What Goes Where

Use `accounts.json` or environment variables for secrets:

- `OPENAI_API_KEY`
- `GOOGLE_API_KEY`, `GOOGLE_GENAI_API_KEY`, `GOOGLE_AI_API_KEY`
- `ANTHROPIC_API_KEY`
- optional `OLLAMA_API_KEY` for hosted/proxied Ollama endpoints

Use `model_catalog.json` for non-secret behavior:

- provider visibility
- local provider base URLs, including Ollama host and port
- driver profile definitions
- regex rules that map model names to drivers and capabilities
- virtual model aliases
- optional private gateway headers or API keys, if you deliberately want that local file to contain them

## Manage Providers Dialog

Open `Edit -> Manage Providers...`.

The `Providers` tab edits the user copy for provider-level settings:

- `Enabled` controls whether the provider appears in selectors.
- `Requires Key` controls credential gating for providers or private gateways.
- `Base URL` overrides local/proxy endpoints, especially Ollama.
- `Save Provider Overrides` writes the local user copy.

The `Model Inspector` tab queries the selected provider and capability:

- `Capability` filters the catalog for chat, embedding, or image use.
- `Show filtered models` reveals models hidden by capability/driver rules.
- `Test Selection` runs a small provider/model probe through the selected driver.

The `Rules` tab shows the effective regex mappings loaded from baseline plus user copy:

- `Regex` is matched against provider model ids.
- `Driver` is the backend protocol mapping used for the model.
- `Capabilities` drive UI filtering.
- `Copy Rule to Overrides` adds the selected rule to the editor so you can adjust it locally.

The `Catalog Overrides` tab is the raw JSON editor for the user copy:

- `Reload User Copy` reloads the local file.
- `Format JSON` reformats the editor contents.
- `Save User Copy` writes the local file and reloads the registry.
- `Reset to Distribution Baseline` deletes the local file and reloads shipped defaults.

## Merge Rules

Top-level arrays are merged by each entry's `id`:

- `providers`
- `driver_profiles`
- `virtual_models`
- `rules`

If the user copy contains an entry with the same `id` as the distribution baseline, the user entry replaces the shipped entry. To remove a shipped entry, add `"disabled": true`.

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

## Providers

Use `providers` to enable or disable visible backends, override display names, set local endpoints, and provide optional headers.

```json
{
  "providers": [
    {
      "id": "ollama",
      "name": "Ollama (Local)",
      "enabled": true,
      "requiresCredential": false,
      "baseUrl": "http://127.0.0.1:11434"
    },
    {
      "id": "google",
      "enabled": false
    }
  ]
}
```

Ollama normally does not require a key. If a hosted or proxied Ollama-compatible endpoint does require one, set `OLLAMA_API_KEY`, add an `ollama` entry to `accounts.json`, or add `api_key`/`headers.Authorization` to the local catalog.

For CI or headless environments without a local Ollama daemon, set:

```text
CP_DISABLE_OLLAMA=1
```

## Driver Profiles

Driver profiles describe the protocol family a model should use. Rules refer to them with `driver`.

```json
{
  "driver_profiles": [
    {
      "id": "openai-responses",
      "name": "OpenAI Responses",
      "provider": "openai",
      "protocol": "chat",
      "endpoint": "/v1/responses"
    }
  ]
}
```

The current runtime understands chat, completion, assistant, embedding, and image driver families. A profile can be added before every backend field is fully used, which is useful when investigating a new provider protocol.

## Rules

Rules are regular expressions over provider model ids. Higher `priority` wins. Rules decide capabilities, role mode, parameter constraints, and driver mapping.

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

For broad provider-specific rules, add `"requires_backend": true` so the rule is used only when the selector already knows the provider. This is useful for local Ollama patterns that intentionally accept many model names.

## Virtual Models

Virtual models are stable aliases that point to concrete provider model ids. Use them for curated choices such as flagship, reasoning, coding, cost-optimized, or high-throughput models.

```json
{
  "virtual_models": [
    {
      "id": "local-coding",
      "target": "qwen2.5-coder:14b",
      "backend": "ollama",
      "name": "Local Coding Model"
    }
  ]
}
```

Aliases resolve before capability matching, so the target model still needs a matching rule.

## Troubleshooting

If a new model is visible only when `Show filtered models` is enabled, inspect the reason column. The usual fixes are:

- add or adjust a regex rule for the model id
- map the rule to the correct driver profile
- add the required capability, such as `vision`, `embedding`, or `image`
- use `Test Selection` to confirm the selected driver works

If reset does not appear to change provider credentials, that is expected. Credentials live separately in environment variables or `accounts.json`.
